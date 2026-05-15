/*
 * ================================================================
 * DW3000 SPI / API Benchmark
 * ================================================================
 *
 * Ziel:
 * -----
 * Profiling der dwt_api:
 *
 *  - Anzahl echter SPI-Transfers
 *  - Laufzeit der dwt_* APIs
 *  - Durchschnittswerte
 *  - SPI Overhead sichtbar machen
 *
 * Einsatz:
 * --------
 * Kann einmal beim Device-Start ausgeführt werden.
 *
 * Voraussetzungen:
 * ----------------
 * In der SPI-Lowlevel-Schicht zählen:
 *
 *      static volatile uint32_t g_spi_access_counter;
 *
 *      readfromspi(...)
 *      writetospi(...)
 *
 * jeweils:
 *
 *      g_spi_access_counter++;
 *
 * API:
 *
 *      uint32_t dwt_get_spi_access_count(void);
 *
 * ================================================================
 */

#include <stdio.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_timer.h"

#include "dw3000_device_api.h"
#include "dw3000_regs.h"
#include "dw3000_port.h"

#define BENCH_ITERATIONS     1000
#define BENCH_WARMUP         100

typedef struct
{
    const char *name;

    uint32_t total_spi;
    uint64_t total_time_us;

    double avg_spi;
    double avg_time_us;
    double avg_time_per_spi_us;

} benchmark_result_t;

/* ================================================================
 * Empty loop benchmark
 * ================================================================ */

static uint64_t benchmark_empty_loop(void)
{
    volatile int dummy = 0;

    int64_t t1 = esp_timer_get_time();

    for (int i = 0; i < BENCH_ITERATIONS; i++)
    {
        dummy++;
    }

    int64_t t2 = esp_timer_get_time();

    return (uint64_t)(t2 - t1);
}

/* ================================================================
 * Print helper
 * ================================================================ */

static void print_result(benchmark_result_t *r)
{
    printf("\n");
    printf("====================================================\n");
    printf("%s\n", r->name);
    printf("====================================================\n");

    printf("Total time         : %" PRIu64 " us\n", r->total_time_us);
    printf("Total SPI accesses : %" PRIu32 "\n", r->total_spi);

    printf("Avg time/call      : %.3f us\n", r->avg_time_us);
    printf("Avg SPI/call       : %.3f\n", r->avg_spi);

    if (r->total_spi > 0)
    {
        printf("Avg time/SPI       : %.3f us\n",
               r->avg_time_per_spi_us);
    }
}

/* ================================================================
 * Generic benchmark macro
 * ================================================================ */

#define RUN_BENCHMARK(NAME, CODE_BLOCK)                         \
do                                                              \
{                                                               \
    benchmark_result_t r = {0};                                 \
                                                                \
    r.name = NAME;                                              \
                                                                \
    /* Warmup */                                                \
    for (int i = 0; i < BENCH_WARMUP; i++)                      \
    {                                                           \
        CODE_BLOCK;                                             \
    }                                                           \
                                                                \
    uint32_t spi_before = spiAccessCount();                     \
                                                                \
    int64_t t1 = esp_timer_get_time();                          \
                                                                \
    for (int i = 0; i < BENCH_ITERATIONS; i++)                  \
    {                                                           \
        CODE_BLOCK;                                             \
    }                                                           \
                                                                \
    int64_t t2 = esp_timer_get_time();                          \
                                                                \
    uint32_t spi_after = spiAccessCount();                      \
                                                                \
    r.total_time_us = (t2 - t1) - g_empty_loop_time_us;         \
    r.total_spi = spi_after - spi_before;                       \
                                                                \
    r.avg_time_us =                                             \
        (double)r.total_time_us / BENCH_ITERATIONS;             \
                                                                \
    r.avg_spi =                                                 \
        (double)r.total_spi / BENCH_ITERATIONS;                 \
                                                                \
    if (r.total_spi > 0)                                        \
    {                                                           \
        r.avg_time_per_spi_us =                                 \
            (double)r.total_time_us / r.total_spi;              \
    }                                                           \
                                                                \
    print_result(&r);                                           \
    total_time_us += r.total_time_us;                           \
} while(0)

/* ================================================================
 * Global empty loop calibration
 * ================================================================ */

static uint64_t g_empty_loop_time_us = 0;

/* ================================================================
 * Benchmarks
 * ================================================================ */

uint32_t dw3000_benchmark() {
    printf("\n");
    printf("#############################################\n");
    printf("# DW3000 SPI/API BENCHMARK\n");
    printf("#############################################\n");

    printf("\n");
    printf("Iterations : %d\n", BENCH_ITERATIONS);
    printf("Warmup     : %d\n", BENCH_WARMUP);

    /*
     * Empty loop calibration
     */

    g_empty_loop_time_us = benchmark_empty_loop();

    printf("Empty loop : %" PRIu64 " us\n",
           g_empty_loop_time_us);

    /*
     * Buffers
     */

    uint8_t txbuf[19];
    uint8_t rxbuf[32];
    uint64_t total_time_us = 0;

    for (int i = 0; i < sizeof(txbuf); i++)
    {
        txbuf[i] = i;
    }

    /*
     * Benchmarks
     */

    RUN_BENCHMARK(
        "dwt_readdevid()",
        dwt_readdevid()
    );

    RUN_BENCHMARK(
        "dwt_read32bitreg(SYS_STATUS_ID)",
        dwt_read32bitreg(SYS_STATUS_ID)
    );

    RUN_BENCHMARK(
        "dwt_write32bitreg(SYS_STATUS_ID)",
        dwt_write32bitreg(SYS_STATUS_ID, 0)
    );

#if 0
    RUN_BENCHMARK(
        "dwt_readtempvbat()",
        {
            uint8_t t;
            uint8_t v;
            dwt_readtempvbat(&t, &v);
        }
    );
#endif

    RUN_BENCHMARK(
        "dwt_writetxdata(19B)",
        dwt_writetxdata(sizeof(txbuf), txbuf, 0)
    );

    RUN_BENCHMARK(
        "dwt_writetxfctrl()",
        dwt_writetxfctrl(sizeof(txbuf), 0, 1)
    );

    RUN_BENCHMARK(
        "dwt_starttx(IMMEDIATE)",
        dwt_starttx(DWT_START_TX_IMMEDIATE)
    );

    RUN_BENCHMARK(
        "dwt_rxenable(IMMEDIATE)",
        dwt_rxenable(DWT_START_RX_IMMEDIATE)
    );

    RUN_BENCHMARK(
        "dwt_readrxdata(32B)",
        dwt_readrxdata(rxbuf, sizeof(rxbuf), 0)
    );

    /*
     * Full TX sequence
     */

    RUN_BENCHMARK(
        "FULL TX sequence",
        {
            dwt_writetxdata(sizeof(txbuf), txbuf, 0);
            dwt_writetxfctrl(sizeof(txbuf), 0, 1);
            dwt_starttx(DWT_START_TX_IMMEDIATE);
        }
    );

    /*
     * Typical status polling
     */

    RUN_BENCHMARK(
        "SYS_STATUS polling",
        {
            volatile uint32_t s =
                dwt_read32bitreg(SYS_STATUS_ID);

            (void)s;
        }
    );

    printf("\n");
    printf("====================================================\n");
    printf("Totals\n");
    printf("====================================================\n");
    printf("Total time         : %" PRIu64 " us\n", total_time_us);

    printf("\n");
    printf("#############################################\n");
    printf("# BENCHMARK FINISHED\n");
    printf("#############################################\n");

    return total_time_us;
}

