/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Simple BELLBOARD Interrupt Test - FLPR Side
 * 
 * This test validates basic BELLBOARD interrupt functionality:
 * - FLPR sends interrupts to M33
 * - FLPR receives interrupts from M33
 * - Shared memory communication works
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/mbox.h>
#include <zephyr/logging/log.h>
#include "ipc_pingpong.h"

LOG_MODULE_REGISTER(flpr_bellboard_test, LOG_LEVEL_INF);

/* Control block structure in shared memory */
struct control_block {
	volatile uint32_t flpr_counter;
	volatile uint32_t m33_counter;
	volatile uint32_t flpr_to_m33_count;
	volatile uint32_t m33_to_flpr_count;
};

static struct control_block *ctrl = (struct control_block *)CONTROL_BLOCK_ADDR;

/* Track last used buffer for round-robin selection */
static uint8_t last_buffer_used = 1;  /* Start with 1 so first acquisition tries buffer 0 */

/* MBOX device for BELLBOARD */
static const struct device *mbox_dev;
static const struct device *mbox_tx_dev;

/**
 * Notify M33 core via VEVIF interrupt
 * 
 * This function:
 * - Executes a memory barrier to ensure all writes are visible
 * - Triggers a VEVIF interrupt to M33 (channel 20)
 * - Handles errors from mbox_send
 * 
 * Requirements: 4.4, 7.1
 * 
 * @return BUFFER_OK on success, BUFFER_ERR_INVALID on failure
 */
static int vevif_notify_m33(void)
{
	int ret;
	
	/* Execute memory barrier before VEVIF trigger to ensure data visibility */
	MEMORY_BARRIER_FULL();
	
	/* Trigger VEVIF interrupt to M33 */
	ret = mbox_send_dt(&(struct mbox_dt_spec){
		.dev = mbox_tx_dev,
		.channel_id = 20
	}, NULL);
	
	if (ret < 0) {
		LOG_ERR("Failed to notify M33: %d", ret);
		return BUFFER_ERR_INVALID;
	}
	
	return BUFFER_OK;
}

/**
 * Acquire next available buffer for writing (FLPR write path)
 * 
 * This function:
 * - Searches for next IDLE buffer in round-robin order
 * - Uses atomic CAS to transition IDLE → WRITING
 * - Implements timeout using k_uptime_get() and polling
 * - Detects overrun when both buffers are not IDLE
 * - Increments overrun counter on overrun detection
 * - Returns buffer handle with pointer to data region
 * 
 * Requirements: 3.1, 3.2, 3.3, 6.1, 6.2, 6.3
 * 
 * @param handle Pointer to buffer_handle_t to populate
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
 * @return BUFFER_OK on success, BUFFER_ERR_TIMEOUT on timeout, BUFFER_ERR_INVALID on invalid params
 */
int buffer_acquire_for_write(buffer_handle_t *handle, uint32_t timeout_ms)
{
	uint64_t start_time;
	uint64_t current_time;
	bool overrun_detected = false;
	
	/* Validate parameters */
	if (handle == NULL) {
		LOG_ERR("Invalid handle parameter");
		return BUFFER_ERR_INVALID;
	}
	
	/* Record start time for timeout */
	start_time = k_uptime_get();
	
	/* Try to acquire a buffer with timeout */
	while (1) {
		/* Try next buffer in round-robin order */
		uint8_t buffer_id = (last_buffer_used + 1) % 2;
		buffer_state_t current_state = buffer_get_state(buffer_id);
		
		if (current_state == BUFFER_STATE_IDLE) {
			/* Try to atomically transition IDLE → WRITING */
			if (atomic_cas_state(&CONTROL_BLOCK->buffer_states[buffer_id],
			                     BUFFER_STATE_IDLE,
			                     BUFFER_STATE_WRITING)) {
				/* Successfully acquired buffer */
				handle->id = buffer_id;
				handle->data = (void *)(buffer_id == 0 ? BUFFER_0_ADDR : BUFFER_1_ADDR);
				handle->size = BUFFER_SIZE;
				handle->state = &CONTROL_BLOCK->buffer_states[buffer_id];
				
				/* Update last used buffer for round-robin */
				last_buffer_used = buffer_id;
				
				LOG_DBG("Acquired buffer %d for writing", buffer_id);
				return BUFFER_OK;
			}
			/* CAS failed, another core may have acquired it, continue trying */
		}
		
		/* Check if both buffers are not IDLE (overrun condition) */
		buffer_state_t state0 = buffer_get_state(0);
		buffer_state_t state1 = buffer_get_state(1);
		
		if (state0 != BUFFER_STATE_IDLE && state1 != BUFFER_STATE_IDLE) {
			/* Overrun detected - both buffers are occupied */
			if (!overrun_detected) {
				/* Increment overrun counter only once per acquisition attempt */
				atomic_inc((atomic_t *)&CONTROL_BLOCK->overrun_count);
				overrun_detected = true;
				LOG_WRN("Buffer overrun detected (count: %d)", CONTROL_BLOCK->overrun_count);
			}
		}
		
		/* Check timeout */
		current_time = k_uptime_get();
		if ((current_time - start_time) >= timeout_ms) {
			LOG_WRN("Buffer acquisition timeout after %u ms", timeout_ms);
			atomic_inc((atomic_t *)&CONTROL_BLOCK->timeout_count);
			return BUFFER_ERR_TIMEOUT;
		}
		
		/* Small delay before retry to avoid busy-waiting */
		k_usleep(100);
	}
	
	/* Should never reach here */
	return BUFFER_ERR_INVALID;
}

/**
 * Commit written buffer and notify M33 (FLPR write path)
 * 
 * This function:
 * - Uses atomic CAS to transition WRITING → READY
 * - Executes memory barrier before notification
 * - Calls vevif_notify_m33() to trigger interrupt
 * - Increments write counter for the buffer
 * - Records timestamp using k_uptime_get()
 * 
 * Requirements: 3.4, 4.1, 7.1, 10.1, 10.2
 * 
 * @param handle Pointer to buffer_handle_t for the buffer to commit
 * @return BUFFER_OK on success, BUFFER_ERR_INVALID on invalid params, BUFFER_ERR_STATE on wrong state
 */
int buffer_commit(buffer_handle_t *handle)
{
	int ret;
	
	/* Validate parameters */
	if (handle == NULL) {
		LOG_ERR("Invalid handle parameter");
		return BUFFER_ERR_INVALID;
	}
	
	if (handle->id > 1) {
		LOG_ERR("Invalid buffer ID: %d", handle->id);
		return BUFFER_ERR_INVALID;
	}
	
	/* Try to atomically transition WRITING → READY */
	if (!atomic_cas_state(&CONTROL_BLOCK->buffer_states[handle->id],
	                      BUFFER_STATE_WRITING,
	                      BUFFER_STATE_READY)) {
		/* State transition failed - buffer is not in WRITING state */
		buffer_state_t current_state = buffer_get_state(handle->id);
		LOG_ERR("Buffer %d commit failed: expected WRITING, got %d", 
		        handle->id, current_state);
		return BUFFER_ERR_STATE;
	}
	
	/* Increment write counter for this buffer */
	atomic_inc((atomic_t *)&CONTROL_BLOCK->write_count[handle->id]);
	
	/* Record timestamp */
	uint64_t timestamp = k_uptime_get();
	CONTROL_BLOCK->last_write_ts[handle->id] = timestamp;
	
	/* Execute memory barrier before notification to ensure data visibility */
	MEMORY_BARRIER_FULL();
	
	/* Notify M33 that buffer is ready */
	ret = vevif_notify_m33();
	if (ret != BUFFER_OK) {
		LOG_ERR("Failed to notify M33 after commit: %d", ret);
		/* Note: Buffer is already in READY state, so M33 can still process it
		 * even if notification failed. This is a non-fatal error. */
	}
	
	LOG_DBG("Committed buffer %d (write count: %d, timestamp: %llu)",
	        handle->id, CONTROL_BLOCK->write_count[handle->id], timestamp);
	
	return BUFFER_OK;
}

/* Callback for BELLBOARD interrupt from M33 */
static void mbox_callback(const struct device *dev, uint32_t channel,
			  void *user_data, struct mbox_msg *msg)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);
	ARG_UNUSED(msg);

	/* Increment interrupt counter */
	ctrl->m33_to_flpr_count++;

	LOG_INF("FLPR: Received interrupt #%d from M33 (M33 counter: %d)",
		ctrl->m33_to_flpr_count, ctrl->m33_counter);
}

int main(void)
{
	int ret;

	LOG_INF("=== FLPR VEVIF Interrupt Test ===");
	LOG_INF("Shared memory base: 0x%08X", (uint32_t)SHARED_MEM_BASE);
	LOG_INF("Control block: 0x%08X", (uint32_t)CONTROL_BLOCK_ADDR);

	/* Get MBOX device (VEVIF RX/TX) */
	mbox_dev = DEVICE_DT_GET(DT_NODELABEL(cpuflpr_vevif_rx));
	if (!device_is_ready(mbox_dev)) {
		LOG_ERR("MBOX RX device not ready");
		return -ENODEV;
	}

	LOG_INF("MBOX RX device ready");

	/* Get TX device */
	mbox_tx_dev = DEVICE_DT_GET(DT_NODELABEL(cpuflpr_vevif_tx));
	if (!device_is_ready(mbox_tx_dev)) {
		LOG_ERR("MBOX TX device not ready");
		return -ENODEV;
	}

	LOG_INF("MBOX TX device ready");

	/* Configure RX channel (receive from M33) - Channel 21 */
	ret = mbox_set_enabled_dt(&(struct mbox_dt_spec){
		.dev = mbox_dev,
		.channel_id = 21
	}, true);
	if (ret < 0) {
		LOG_ERR("Failed to enable RX channel: %d", ret);
		return ret;
	}

	/* Register callback for interrupts from M33 */
	ret = mbox_register_callback_dt(&(struct mbox_dt_spec){
		.dev = mbox_dev,
		.channel_id = 21
	}, mbox_callback, NULL);
	if (ret < 0) {
		LOG_ERR("Failed to register callback: %d", ret);
		return ret;
	}

	LOG_INF("RX channel configured (Channel 21: M33 -> FLPR)");
	LOG_INF("TX channel configured (Channel 20: FLPR -> M33)");
	LOG_INF("Starting ping-pong buffer test loop...");

	/* Statistics tracking */
	uint32_t iteration = 0;
	uint32_t last_stats_time = k_uptime_get_32();

	/* Main loop: acquire buffer, write data, commit, repeat */
	while (1) {
		buffer_handle_t buf;
		
		/* Acquire next available buffer for writing (1 second timeout) */
		ret = buffer_acquire_for_write(&buf, 1000);
		
		if (ret == BUFFER_ERR_TIMEOUT) {
			LOG_ERR("Failed to acquire buffer: timeout");
			/* Handle timeout - continue trying */
			k_msleep(100);
			continue;
		} else if (ret != BUFFER_OK) {
			LOG_ERR("Failed to acquire buffer: %d", ret);
			/* Handle other errors */
			k_msleep(100);
			continue;
		}
		
		/* Successfully acquired buffer - write test data pattern */
		uint32_t *data = (uint32_t *)buf.data;
		uint32_t pattern_base = iteration;
		
		/* Fill buffer with test pattern: incrementing values based on iteration */
		for (size_t i = 0; i < (buf.size / sizeof(uint32_t)); i++) {
			data[i] = pattern_base + i;
		}
		
		LOG_DBG("FLPR: Wrote test pattern to buffer %d (base: 0x%08X)", 
		        buf.id, pattern_base);
		
		/* Commit buffer and notify M33 */
		ret = buffer_commit(&buf);
		
		if (ret != BUFFER_OK) {
			LOG_ERR("Failed to commit buffer %d: %d", buf.id, ret);
			/* Buffer is in inconsistent state - this is a critical error */
			/* In production, might want to reset the buffer state */
		} else {
			LOG_INF("FLPR: Committed buffer %d (iteration %u)", buf.id, iteration);
		}
		
		/* Increment iteration counter */
		iteration++;
		
		/* Log statistics periodically (every 5 seconds) */
		uint32_t current_time = k_uptime_get_32();
		if ((current_time - last_stats_time) >= 5000) {
			LOG_INF("=== FLPR Statistics ===");
			LOG_INF("  Iterations: %u", iteration);
			LOG_INF("  Buffer 0 writes: %u", CONTROL_BLOCK->write_count[0]);
			LOG_INF("  Buffer 1 writes: %u", CONTROL_BLOCK->write_count[1]);
			LOG_INF("  Overruns: %u", CONTROL_BLOCK->overrun_count);
			LOG_INF("  Timeouts: %u", CONTROL_BLOCK->timeout_count);
			LOG_INF("  Legacy FLPR->M33: %u, M33->FLPR: %u",
			        ctrl->flpr_to_m33_count, ctrl->m33_to_flpr_count);
			
			last_stats_time = current_time;
		}
		
		/* Small delay between writes to simulate realistic data production rate */
		k_msleep(500);
	}

	return 0;
}
