/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Simple BELLBOARD Interrupt Test - M33 Side
 * 
 * This test validates basic BELLBOARD interrupt functionality:
 * - M33 receives interrupts from FLPR
 * - M33 sends interrupts to FLPR
 * - Shared memory communication works
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/mbox.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "ipc_pingpong.h"

LOG_MODULE_REGISTER(m33_bellboard_test, LOG_LEVEL_INF);

/* Control block structure in shared memory */
struct control_block {
	volatile uint32_t flpr_counter;
	volatile uint32_t m33_counter;
	volatile uint32_t flpr_to_m33_count;
	volatile uint32_t m33_to_flpr_count;
};

static struct control_block *ctrl = (struct control_block *)CONTROL_BLOCK_ADDR;

/**
 * Initialize the ping-pong buffer control block
 * 
 * This function:
 * - Zeros the entire control block structure
 * - Sets both buffer states to IDLE
 * - Initializes all counters to zero
 * - Executes a memory barrier to ensure visibility
 * 
 * Requirements: 8.1, 8.2, 8.3
 * 
 * @return BUFFER_OK on success, BUFFER_ERR_INIT on failure
 */
static int control_block_init(void)
{
	control_block_t *cb = CONTROL_BLOCK;
	
	/* Zero entire control block structure */
	memset((void *)cb, 0, sizeof(control_block_t));
	
	/* Set both buffer states to IDLE */
	cb->buffer_states[0] = BUFFER_STATE_IDLE;
	cb->buffer_states[1] = BUFFER_STATE_IDLE;
	
	/* Initialize all counters to zero (already done by memset, but explicit for clarity) */
	cb->write_count[0] = 0;
	cb->write_count[1] = 0;
	cb->read_count[0] = 0;
	cb->read_count[1] = 0;
	cb->overrun_count = 0;
	cb->timeout_count = 0;
	
	/* Initialize timestamps */
	cb->last_write_ts[0] = 0;
	cb->last_write_ts[1] = 0;
	cb->last_read_ts[0] = 0;
	cb->last_read_ts[1] = 0;
	
	/* Initialize synchronization flags */
	cb->flpr_ready = 0;
	cb->m33_ready = 1;  /* M33 is ready after initialization */
	
	/* Set configuration */
	cb->buffer_size = BUFFER_SIZE;
	cb->timeout_ms = 1000;  /* Default 1 second timeout */
	
	/* Execute memory barrier to ensure all writes are visible to FLPR */
	MEMORY_BARRIER_FULL();
	
	return BUFFER_OK;
}

/* MBOX device for BELLBOARD */
static const struct device *mbox_dev;
static const struct device *mbox_tx_dev;

/* Work queue for processing buffers outside of ISR context */
static struct k_work process_buffer_work;
static K_SEM_DEFINE(buffer_ready_sem, 0, 1);

/**
 * Process ready buffer (called from work queue)
 * 
 * This function:
 * - Acquires ready buffer (non-blocking)
 * - Reads and validates data from buffer
 * - Releases buffer and notifies FLPR
 * - Logs statistics periodically
 * 
 * Requirements: 5.1, 5.2, 5.3, 5.4, 5.5
 */
static void process_buffer_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	buffer_handle_t buf;
	int ret;
	
	/* Try to acquire a ready buffer (non-blocking) */
	ret = buffer_acquire_for_read(&buf, 0);
	
	if (ret == BUFFER_ERR_TIMEOUT) {
		/* No buffer ready - this is normal, ISR may have been spurious */
		LOG_DBG("No buffer ready to process");
		return;
	} else if (ret != BUFFER_OK) {
		LOG_ERR("Failed to acquire buffer for reading: %d", ret);
		return;
	}
	
	/* Successfully acquired buffer - validate data */
	uint32_t *data = (uint32_t *)buf.data;
	uint32_t expected_base = data[0];  /* First value is the pattern base */
	bool data_valid = true;
	
	/* Validate test pattern: should be incrementing values */
	for (size_t i = 0; i < (buf.size / sizeof(uint32_t)); i++) {
		if (data[i] != (expected_base + i)) {
			LOG_ERR("Data corruption in buffer %d at offset %zu: expected 0x%08X, got 0x%08X",
			        buf.id, i, expected_base + i, data[i]);
			data_valid = false;
			break;  /* Stop checking after first error */
		}
	}
	
	if (data_valid) {
		LOG_INF("M33: Validated buffer %d (pattern base: 0x%08X)", buf.id, expected_base);
	} else {
		LOG_ERR("M33: Buffer %d validation FAILED", buf.id);
	}
	
	/* Release buffer and notify FLPR */
	ret = buffer_release(&buf);
	
	if (ret != BUFFER_OK) {
		LOG_ERR("Failed to release buffer %d: %d", buf.id, ret);
		/* This is a critical error - buffer may be stuck in READING state */
	} else {
		LOG_DBG("M33: Released buffer %d", buf.id);
	}
}

/**
 * Notify FLPR core via VEVIF interrupt
 * 
 * This function:
 * - Executes a memory barrier to ensure all writes are visible
 * - Triggers a VEVIF interrupt to FLPR (channel 21)
 * - Handles errors from mbox_send
 * 
 * Requirements: 4.4, 7.1
 * 
 * @return BUFFER_OK on success, BUFFER_ERR_INVALID on failure
 */
static int vevif_notify_flpr(void)
{
	int ret;
	
	/* Execute memory barrier before VEVIF trigger to ensure data visibility */
	MEMORY_BARRIER_FULL();
	
	/* Trigger VEVIF interrupt to FLPR */
	ret = mbox_send_dt(&(struct mbox_dt_spec){
		.dev = mbox_tx_dev,
		.channel_id = 21
	}, NULL);
	
	if (ret < 0) {
		LOG_ERR("Failed to notify FLPR: %d", ret);
		return BUFFER_ERR_INVALID;
	}
	
	return BUFFER_OK;
}

/**
 * Acquire next ready buffer for reading (M33 read path)
 * 
 * This function:
 * - Searches for next READY buffer in FIFO order (uses timestamps)
 * - Uses atomic CAS to transition READY → READING
 * - Implements timeout using k_uptime_get() and polling
 * - Returns buffer handle with pointer to data region
 * 
 * Requirements: 5.1, 5.2, 5.5
 * 
 * @param handle Pointer to buffer_handle_t to populate
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
 * @return BUFFER_OK on success, BUFFER_ERR_TIMEOUT on timeout, BUFFER_ERR_INVALID on invalid params
 */
int buffer_acquire_for_read(buffer_handle_t *handle, uint32_t timeout_ms)
{
	uint64_t start_time;
	uint64_t current_time;
	
	/* Validate parameters */
	if (handle == NULL) {
		LOG_ERR("Invalid handle parameter");
		return BUFFER_ERR_INVALID;
	}
	
	/* Record start time for timeout */
	start_time = k_uptime_get();
	
	/* Try to acquire a READY buffer with timeout */
	while (1) {
		/* Find the oldest READY buffer (FIFO order using timestamps) */
		int8_t oldest_buffer = -1;
		uint64_t oldest_timestamp = UINT64_MAX;
		
		/* Check both buffers for READY state */
		for (uint8_t i = 0; i < 2; i++) {
			buffer_state_t state = buffer_get_state(i);
			
			if (state == BUFFER_STATE_READY) {
				/* Get timestamp for this buffer */
				uint64_t timestamp = CONTROL_BLOCK->last_write_ts[i];
				
				/* Track the oldest (earliest timestamp) READY buffer */
				if (timestamp < oldest_timestamp) {
					oldest_timestamp = timestamp;
					oldest_buffer = i;
				}
			}
		}
		
		/* If we found a READY buffer, try to acquire it */
		if (oldest_buffer >= 0) {
			/* Try to atomically transition READY → READING */
			if (atomic_cas_state(&CONTROL_BLOCK->buffer_states[oldest_buffer],
			                     BUFFER_STATE_READY,
			                     BUFFER_STATE_READING)) {
				/* Successfully acquired buffer */
				handle->id = oldest_buffer;
				handle->data = (void *)(oldest_buffer == 0 ? BUFFER_0_ADDR : BUFFER_1_ADDR);
				handle->size = BUFFER_SIZE;
				handle->state = &CONTROL_BLOCK->buffer_states[oldest_buffer];
				
				LOG_DBG("Acquired buffer %d for reading (timestamp: %llu)", 
				        oldest_buffer, oldest_timestamp);
				return BUFFER_OK;
			}
			/* CAS failed, another core may have acquired it, continue trying */
		}
		
		/* Check timeout */
		current_time = k_uptime_get();
		if ((current_time - start_time) >= timeout_ms) {
			LOG_WRN("Buffer read acquisition timeout after %u ms", timeout_ms);
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
 * Release processed buffer and notify FLPR (M33 read path)
 * 
 * This function:
 * - Uses atomic CAS to transition READING → IDLE
 * - Executes memory barrier before notification
 * - Calls vevif_notify_flpr() to trigger interrupt
 * - Increments read counter for the buffer
 * - Records timestamp using k_uptime_get()
 * 
 * Requirements: 5.4, 4.2, 7.2, 10.1, 10.2
 * 
 * @param handle Pointer to buffer_handle_t to release
 * @return BUFFER_OK on success, BUFFER_ERR_INVALID on invalid params, BUFFER_ERR_STATE on wrong state
 */
int buffer_release(buffer_handle_t *handle)
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
	
	/* Try to atomically transition READING → IDLE */
	if (!atomic_cas_state(&CONTROL_BLOCK->buffer_states[handle->id],
	                      BUFFER_STATE_READING,
	                      BUFFER_STATE_IDLE)) {
		/* CAS failed - buffer is not in READING state */
		buffer_state_t current_state = buffer_get_state(handle->id);
		LOG_ERR("Failed to release buffer %d: wrong state (expected READING, got %d)",
		        handle->id, current_state);
		return BUFFER_ERR_STATE;
	}
	
	/* Increment read counter for this buffer */
	atomic_inc((atomic_t *)&CONTROL_BLOCK->read_count[handle->id]);
	
	/* Record timestamp */
	uint64_t timestamp = k_uptime_get();
	CONTROL_BLOCK->last_read_ts[handle->id] = timestamp;
	
	/* Execute memory barrier before notification to ensure data visibility */
	MEMORY_BARRIER_FULL();
	
	/* Notify FLPR that buffer is now IDLE */
	ret = vevif_notify_flpr();
	if (ret != BUFFER_OK) {
		LOG_ERR("Failed to notify FLPR after buffer release: %d", ret);
		/* Note: Buffer state is already IDLE, so this is not a critical error */
		return ret;
	}
	
	LOG_DBG("Released buffer %d (read count: %u, timestamp: %llu)",
	        handle->id, CONTROL_BLOCK->read_count[handle->id], timestamp);
	
	return BUFFER_OK;
}

/* Callback for BELLBOARD interrupt from FLPR */
static void mbox_callback(const struct device *dev, uint32_t channel,
			  void *user_data, struct mbox_msg *msg)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);
	ARG_UNUSED(msg);

	/* Increment interrupt counter for legacy compatibility */
	ctrl->flpr_to_m33_count++;

	LOG_DBG("M33: Received interrupt #%d from FLPR", ctrl->flpr_to_m33_count);
	
	/* Schedule work queue to process buffer outside ISR context */
	k_work_submit(&process_buffer_work);
}

int main(void)
{
	int ret;

	LOG_INF("=== M33 VEVIF Interrupt Test ===");
	LOG_INF("Shared memory base: 0x%08X", (uint32_t)SHARED_MEM_BASE);
	LOG_INF("Control block: 0x%08X", (uint32_t)CONTROL_BLOCK_ADDR);

	/* Initialize ping-pong buffer control block */
	ret = control_block_init();
	if (ret != BUFFER_OK) {
		LOG_ERR("Failed to initialize control block: %d", ret);
		return ret;
	}

	LOG_INF("Ping-pong buffer control block initialized");

	/* Initialize work queue for buffer processing */
	k_work_init(&process_buffer_work, process_buffer_handler);
	LOG_INF("Work queue initialized");

	/* Initialize old control block for backward compatibility */
	ctrl->flpr_counter = 0;
	ctrl->m33_counter = 0;
	ctrl->flpr_to_m33_count = 0;
	ctrl->m33_to_flpr_count = 0;

	LOG_INF("Legacy control block initialized");

	/* Get MBOX device (VEVIF RX) */
	mbox_dev = DEVICE_DT_GET(DT_NODELABEL(cpuapp_vevif_rx));
	if (!device_is_ready(mbox_dev)) {
		LOG_ERR("MBOX RX device not ready");
		return -ENODEV;
	}

	LOG_INF("MBOX RX device ready");

	/* Configure RX channel (receive from FLPR) - Channel 20 */
	ret = mbox_set_enabled_dt(&(struct mbox_dt_spec){
		.dev = mbox_dev,
		.channel_id = 20
	}, true);
	if (ret < 0) {
		LOG_ERR("Failed to enable RX channel: %d", ret);
		return ret;
	}

	/* Register callback for interrupts from FLPR */
	ret = mbox_register_callback_dt(&(struct mbox_dt_spec){
		.dev = mbox_dev,
		.channel_id = 20
	}, mbox_callback, NULL);
	if (ret < 0) {
		LOG_ERR("Failed to register callback: %d", ret);
		return ret;
	}

	LOG_INF("RX channel configured (Channel 20: FLPR -> M33)");

	/* Get TX device */
	mbox_tx_dev = DEVICE_DT_GET(DT_NODELABEL(cpuapp_vevif_tx));
	if (!device_is_ready(mbox_tx_dev)) {
		LOG_ERR("MBOX TX device not ready");
		return -ENODEV;
	}

	LOG_INF("TX channel configured (Channel 21: M33 -> FLPR)");
	LOG_INF("Waiting for FLPR to start...");

	/* Wait a bit for FLPR to initialize */
	k_msleep(500);

	LOG_INF("Starting ping-pong buffer processing...");

	/* Main loop: log statistics periodically */
	while (1) {
		/* Log statistics every 5 seconds */
		LOG_INF("=== M33 Statistics ===");
		LOG_INF("  Buffer 0 reads: %u", CONTROL_BLOCK->read_count[0]);
		LOG_INF("  Buffer 1 reads: %u", CONTROL_BLOCK->read_count[1]);
		LOG_INF("  Interrupts received: %u", ctrl->flpr_to_m33_count);
		LOG_INF("  Overruns: %u", CONTROL_BLOCK->overrun_count);
		LOG_INF("  Timeouts: %u", CONTROL_BLOCK->timeout_count);
		
		/* Calculate and log buffer utilization */
		uint32_t total_writes = CONTROL_BLOCK->write_count[0] + CONTROL_BLOCK->write_count[1];
		uint32_t total_reads = CONTROL_BLOCK->read_count[0] + CONTROL_BLOCK->read_count[1];
		
		if (total_writes > 0) {
			uint32_t utilization = (total_reads * 100) / total_writes;
			LOG_INF("  Buffer utilization: %u%% (%u reads / %u writes)",
			        utilization, total_reads, total_writes);
		}
		
		k_msleep(5000);
	}

	return 0;
}
