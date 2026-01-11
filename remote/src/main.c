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

LOG_MODULE_REGISTER(flpr_bellboard_test, LOG_LEVEL_INF);

/* Shared memory addresses - must match overlay definitions */
#define SHARED_MEM_BASE     0x20010000
#define BUFFER_0_ADDR       (SHARED_MEM_BASE)
#define BUFFER_1_ADDR       (SHARED_MEM_BASE + 0x10000)
#define CONTROL_BLOCK_ADDR  (SHARED_MEM_BASE + 0x20000)

/* Control block structure in shared memory */
struct control_block {
	volatile uint32_t flpr_counter;
	volatile uint32_t m33_counter;
	volatile uint32_t flpr_to_m33_count;
	volatile uint32_t m33_to_flpr_count;
};

static struct control_block *ctrl = (struct control_block *)CONTROL_BLOCK_ADDR;

/* MBOX device for BELLBOARD */
static const struct device *mbox_dev;

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
	LOG_INF("Shared memory base: 0x%08X", SHARED_MEM_BASE);
	LOG_INF("Control block: 0x%08X", CONTROL_BLOCK_ADDR);

	/* Get MBOX device (VEVIF RX/TX) */
	mbox_dev = DEVICE_DT_GET(DT_NODELABEL(cpuflpr_vevif_rx));
	if (!device_is_ready(mbox_dev)) {
		LOG_ERR("MBOX RX device not ready");
		return -ENODEV;
	}

	LOG_INF("MBOX RX device ready");

	/* Get TX device */
	const struct device *mbox_tx_dev = DEVICE_DT_GET(DT_NODELABEL(cpuflpr_vevif_tx));
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
	LOG_INF("Starting interrupt test loop...");

	/* Main loop: send interrupt to M33 every 1.5 seconds */
	while (1) {
		/* Increment FLPR counter */
		ctrl->flpr_counter++;

		/* Send interrupt to M33 */
		ret = mbox_send_dt(&(struct mbox_dt_spec){
			.dev = mbox_tx_dev,
			.channel_id = 20
		}, NULL);
		
		if (ret < 0) {
			LOG_ERR("Failed to send interrupt: %d", ret);
		} else {
			ctrl->flpr_to_m33_count++;
			LOG_INF("FLPR: Sent interrupt #%d to M33 (FLPR counter: %d)",
				ctrl->flpr_to_m33_count, ctrl->flpr_counter);
		}

		/* Print statistics */
		LOG_INF("Stats - FLPR->M33: %d, M33->FLPR: %d",
			ctrl->flpr_to_m33_count, ctrl->m33_to_flpr_count);

		k_msleep(1500);
	}

	return 0;
}
