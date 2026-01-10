/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <string.h>

#include <zephyr/logging/log.h>

#include <zephyr/ipc/ipc_service.h>

/* FFT utilities - uncomment when needed */
/* #include "rfft_q15_simplified.h" */
/* #include "fft_utils.h" */

#ifdef CONFIG_TEST_EXTRA_STACK_SIZE
#define STACKSIZE	(1024 + CONFIG_TEST_EXTRA_STACK_SIZE)
#else
#define STACKSIZE	(1024)
#endif

K_THREAD_STACK_DEFINE(ipc0_stack, STACKSIZE);

LOG_MODULE_REGISTER(host, LOG_LEVEL_INF);

struct payload {
	unsigned long cnt;
	unsigned long size;
	uint8_t data[CONFIG_APP_IPC_SERVICE_MESSAGE_LEN - sizeof(unsigned long) * 2];
};

static struct payload payload_buffer;
static struct payload *p_payload = &payload_buffer;

static K_SEM_DEFINE(bound_sem, 0, 1);

static void ep_bound(void *priv)
{
	k_sem_give(&bound_sem);
}

static void ep_recv(const void *data, size_t len, void *priv)
{
	uint8_t received_val = *((uint8_t *)data);
	static uint8_t expected_val;


	if ((received_val != expected_val) || (len != CONFIG_APP_IPC_SERVICE_MESSAGE_LEN)) {
		printk("Unexpected message received_val: %d , expected_val: %d\n",
			received_val,
			expected_val);
	}

	expected_val++;
}

static struct ipc_ept_cfg ep_cfg = {
	.name = "ep0",
	.cb = {
		.bound    = ep_bound,
		.received = ep_recv,
	},
};

static void check_task(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	unsigned long last_cnt = p_payload->cnt;
	unsigned long delta;

	while (1) {
		k_sleep(K_MSEC(1000));

		delta = p_payload->cnt - last_cnt;

		printk("Remote Δpkt: %ld (%ld B/pkt) | throughput: %ld bit/s\n",
			delta, p_payload->size, delta * CONFIG_APP_IPC_SERVICE_MESSAGE_LEN * 8);

		last_cnt = p_payload->cnt;
	}
}

K_THREAD_DEFINE(thread_check_id, STACKSIZE, check_task, NULL, NULL, NULL,
		K_PRIO_COOP(1), 0, -1);

// 讀取 RISC-V cycle counter
static inline uint32_t read_cycle(void) {
	uint32_t cycle;
	__asm__ volatile ("rdcycle %0" : "=r"(cycle));
	return cycle;
}

// 使用新 API 測試 FFT
static void test_fft_with_api(void)
{
	printk("\n=== FFT API Test: 4096-point ===\n");
	
	static uint16_t top_bins[20];
	
	rfft_status_t status = find_fft_top_bins(
		test_signal_15_sines,
		4096,
		4096,
		top_bins,
		20
	);
	
	if (status != RFFT_SUCCESS) {
		printk("✗ FFT failed with status: %d\n", status);
		return;
	}
	
	printk("✓ FFT completed successfully!\n\n");
	printk("Top 20 frequency bins:\n");
	printk("Rank  Bin    Freq(Hz)\n");
	printk("========================\n");
	
	for (int i = 0; i < 20; i++) {
		float freq = (float)top_bins[i] * 16000.0f / 4096.0f;
		printk("%2d    %4d   %8.2f\n", 
		       i+1, top_bins[i], (double)freq);
	}
	
	printk("\n=== 4096-point Test Complete ===\n");
}

// 性能測試：4096 點 FFT
static void test_fft_performance(void)
{
	printk("\n=== FFT Performance Test: 4096-point ===\n");
	
	static uint16_t top_bins[20];
	const int iterations = 1000;
	volatile uint32_t checksum = 0;  // 防止編譯器優化
	
	printk("Running %d iterations...\n", iterations);
	
	// 使用 Zephyr 的高精度計時器
	uint64_t start_time = k_cyc_to_ns_floor64(k_cycle_get_64());
	uint64_t start_cycles = k_cycle_get_64();
	
	// 執行 100 次 FFT
	for (int i = 0; i < iterations; i++) {
		find_fft_top_bins(
			test_signal_15_sines,
			4096,
			4096,
			top_bins,
			20
		);
		// 累加結果防止優化
		checksum += top_bins[0];
	}
	
	// 記錄結束時間
	uint64_t end_time = k_cyc_to_ns_floor64(k_cycle_get_64());
	uint64_t end_cycles = k_cycle_get_64();
	
	uint64_t total_ns = end_time - start_time;
	uint64_t total_cycles = end_cycles - start_cycles;
	uint64_t avg_ns = total_ns / iterations;
	uint64_t avg_cycles = total_cycles / iterations;
	
	printk("\n性能結果:\n");
	printk("  總 cycles: %llu\n", total_cycles);
	printk("  平均 cycles/FFT: %llu\n", avg_cycles);
	printk("  總時間: %llu ns (%.2f ms)\n", 
	       total_ns, (double)(total_ns / 1000000.0));
	printk("  平均時間/FFT: %llu ns (%.2f us)\n", 
	       avg_ns, (double)(avg_ns / 1000.0));
	
	// 從 cycles 計算實際 CPU 頻率
	if (total_cycles > 0 && total_ns > 0) {
		uint64_t actual_freq_hz = (total_cycles * 1000000000ULL) / total_ns;
		printk("  實際 CPU 頻率: %llu Hz (%.2f MHz)\n", 
		       actual_freq_hz, (double)(actual_freq_hz / 1000000.0));
	}
	
	// 計算每秒可處理的 FFT 數量
	if (avg_ns > 0) {
		uint64_t ffts_per_sec = 1000000000ULL / avg_ns;
		printk("  FFT 吞吐量: %llu FFTs/秒\n", ffts_per_sec);
	}
	
	printk("  Checksum: %lu (防止優化)\n", (unsigned long)checksum);
	printk("\n=== Performance Test Complete ===\n");
}

#ifdef ENABLE_FFT_8K
// 性能測試：8192 點 FFT
static void test_fft_8192_performance(void)
{
	printk("\n=== FFT Performance Test: 8192-point ===\n");
	
	static uint16_t top_bins[20];
	const int iterations = 100;
	
	printk("Running %d iterations...\n", iterations);
	
	uint32_t start_cycle = read_cycle();
	
	for (int i = 0; i < iterations; i++) {
		find_fft_top_bins(
			test_signal_15_sines_8192,
			8192,
			8192,
			top_bins,
			20
		);
	}
	
	uint32_t end_cycle = read_cycle();
	uint32_t total_cycles = end_cycle - start_cycle;
	uint32_t avg_cycles = total_cycles / iterations;
	
	printk("\n性能結果:\n");
	printk("  總 cycles: %lu\n", (unsigned long)total_cycles);
	printk("  平均 cycles/FFT: %lu\n", (unsigned long)avg_cycles);
	
	uint32_t cpu_freq_mhz = 64;
	uint32_t avg_us = avg_cycles / cpu_freq_mhz;
	float avg_ms = (float)avg_cycles / (cpu_freq_mhz * 1000.0f);
	
	printk("  平均時間 (假設 64 MHz): %lu us (%.2f ms)\n", 
	       (unsigned long)avg_us, (double)avg_ms);
	
	if (avg_cycles > 0) {
		uint32_t ffts_per_sec = (cpu_freq_mhz * 1000000UL) / avg_cycles;
		printk("  FFT 吞吐量: %lu FFTs/秒\n", (unsigned long)ffts_per_sec);
	}
	
	printk("\n=== Performance Test Complete ===\n");
}
#endif /* ENABLE_FFT_8K */

#ifdef ENABLE_FFT_8K
// 測試 8192 點 FFT
static void test_fft_8192(void)
{
	printk("\n=== FFT API Test: 8192-point ===\n");
	
	static uint16_t top_bins[20];
	
	rfft_status_t status = find_fft_top_bins(
		test_signal_15_sines_8192,
		8192,
		8192,
		top_bins,
		20
	);
	
	if (status != RFFT_SUCCESS) {
		printk("✗ FFT failed with status: %d\n", status);
		return;
	}
	
	printk("✓ FFT completed successfully!\n\n");
	printk("Top 20 frequency bins:\n");
	printk("Rank  Bin    Freq(Hz)\n");
	printk("========================\n");
	
	for (int i = 0; i < 20; i++) {
		float freq = (float)top_bins[i] * 16000.0f / 8192.0f;
		printk("%2d    %4d   %8.2f\n", 
		       i+1, top_bins[i], (double)freq);
	}
	
	printk("\n=== 8192-point Test Complete ===\n");
}
#endif /* ENABLE_FFT_8K */

int main(void)
{
	const struct device *ipc0_instance;
	struct ipc_ept ep;
	int ret;

	memset(p_payload->data, 0xA5, sizeof(p_payload->data));

	p_payload->size = CONFIG_APP_IPC_SERVICE_MESSAGE_LEN;
	p_payload->cnt = 0;

	printk("Remote IPC-service %s demo started\n", CONFIG_BOARD_TARGET);
	
	// 性能測試 4096 點 FFT
	test_fft_performance();
	
#ifdef ENABLE_FFT_8K
	// 性能測試 8192 點 FFT
	test_fft_8192_performance();
#endif

	ipc0_instance = DEVICE_DT_GET(DT_NODELABEL(ipc0));

#if defined(CONFIG_IPC_SERVICE_BACKEND_ICBMSG)
	/* Wait a bit for the remote core to boot. */
	k_msleep(100);
#endif

	ret = ipc_service_open_instance(ipc0_instance);
	if ((ret < 0) && (ret != -EALREADY)) {
		LOG_INF("ipc_service_open_instance() failure (%d)", ret);
		return ret;
	}

	ret = ipc_service_register_endpoint(ipc0_instance, &ep, &ep_cfg);
	if (ret < 0) {
		printf("ipc_service_register_endpoint() failure (%d)", ret);
		return ret;
	}

	k_sem_take(&bound_sem, K_FOREVER);
	k_thread_start(thread_check_id);

	while (true) {
		ret = ipc_service_send(&ep, p_payload, CONFIG_APP_IPC_SERVICE_MESSAGE_LEN);
		if (ret == -ENOMEM) {
			/* No space in the buffer. Retry. */
			continue;
		} else if (ret < 0) {
			printk("send_message(%ld) failed with ret %d\n", p_payload->cnt, ret);
			break;
		}

		p_payload->cnt++;


		/* Quasi minimal busy wait time which allows to continuously send
		 * data without -ENOMEM error code. The purpose is to test max
		 * throughput. Determined experimentally.
		 */
		if (CONFIG_APP_IPC_SERVICE_SEND_INTERVAL < 1000) {
			k_busy_wait(CONFIG_APP_IPC_SERVICE_SEND_INTERVAL);
		} else {
			k_msleep(CONFIG_APP_IPC_SERVICE_SEND_INTERVAL/1000);
		}
	}

	return 0;
}
