/* loopback.c: Read packets from NIC port and send it to same port (port X -> port X).
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "common_dpdk.h"
#include "common_stats.h"

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

#include <signal.h>
#include <stdio.h>

#include "argparse.h"

// Flag variable to determine when to stop packet processing.
static volatile int __stop = 0;

// Handle CTRL+C and kill signals.
static void handle_sig(int sig)
{
    switch (sig) {
    case SIGINT:
    case SIGTERM:
        __stop = 1;
        break;
    }
}

// Return whether to stop main loop or not.
static int stop_loop(void)
{
    return __stop;
}

// Main function for worker lcore process.
static int lcore_main(void *arg)
{
    struct lcore_arguments *lcore_args = arg;
    struct application_settings *app_settings = (struct application_settings *)lcore_args->app_settings;
    struct rte_mbuf *bufs[app_settings->dpdk.burst_size];

    // Main loop for packet processing.
    while (!stop_loop()) {
        uint16_t nb_rx;
        uint16_t nb_tx;
        uint16_t port_id;
        uint16_t queue_id;

        for (int index = lcore_args->start_index; index < lcore_args->end_index; index++) {
            port_id = index / app_settings->dpdk.queues_per_port;
            queue_id = index % app_settings->dpdk.queues_per_port;
            nb_tx = 0;

            // Recieve packets from port on given queue.
            nb_rx = rte_eth_rx_burst(port_id, queue_id, bufs, app_settings->dpdk.burst_size);

            /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
            /* Place for your packet processing logic. */
            /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

            // Send burst of TX packets back where it came from.
            while (nb_rx != nb_tx && !stop_loop()) {
                nb_tx += rte_eth_tx_burst(port_id, queue_id, bufs, nb_rx);
            }
        }
    }

    return 0;
}

/* Init all necessary components. */
int init(int argc, char *argv[],
    struct application_settings *app_settings,
    struct measurement_settings *measurement)
{
    // DPDK enviroment init.
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eal_init() failed.\n");
        return ERROR_DONT_CLEANUP;
    }
    argc -= ret;
    argv += ret;

    // Parse custom command line arguments.
    ret = parse_custom_args(argc, argv, app_settings);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: parse_custom_args() failed.\n");
        return ERROR_DONT_CLEANUP;
    }

    ret = check_worker_cores_cnt();
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: check_worker_cores_cnt() failed.\n");
        return ERROR_DONT_CLEANUP;
    }

    // RX port configuration with RSS enabled.
    app_settings->dpdk.port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
    app_settings->dpdk.port_conf.rxmode.offloads = 0;
    app_settings->dpdk.port_conf.rx_adv_conf.rss_conf.rss_key = NULL;
    app_settings->dpdk.port_conf.rx_adv_conf.rss_conf.rss_hf = RTE_ETH_RSS_IP;

    measurement->print_rx = false;
    measurement->next_stats_delay = app_settings->next_stats_delay_ms;

    ret = time_variables_init(measurement);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: time_variables_init() failed.\n");
        return ERROR_DO_CLEANUP;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct application_settings app_settings;
    struct measurement_settings measurement;

    int ret = init(argc, argv, &app_settings, &measurement);
    if (ret < 0) {
        if (ret == ERROR_DO_CLEANUP) {
            cleanup_measurement(&measurement);
        }
        rte_exit(EXIT_FAILURE, "ERROR: init() failed.\n");
    }

    // Register signal for loop stopping.
    signal(SIGINT, &handle_sig);
    signal(SIGTERM, &handle_sig);

    // Create mbuf.
    struct rte_mempool *mbuf_pool;
    mbuf_pool = rte_pktmbuf_pool_create(
        "pcap_pool",
        app_settings.dpdk.mempool_size,
        app_settings.dpdk.mempool_cache_size,
        0,
        app_settings.dpdk.mbuf_size + RTE_PKTMBUF_HEADROOM,
        rte_socket_id()
    );
    if (mbuf_pool == NULL) {
        cleanup_measurement(&measurement);
        rte_exit(-rte_errno, "ERROR: rte_pktmbuf_pool_create() failed.\n");
    }

    // Setup device for RX and TX.
    ret = device_setup(mbuf_pool, &app_settings.dpdk, true, true);
    if (ret != 0) {
        cleanup(app_settings.dpdk.ports);
        cleanup_measurement(&measurement);
        rte_exit(EXIT_FAILURE, "ERROR: device_setup() failed.\n");
    }

    // Each lcore needs to have its own instance of lcore_arguments otherwise race condition may occure.
    struct lcore_arguments lcore_args[get_worker_cores_cnt()];

    // Start main loop for all worker lcores.
    unsigned i = 0;
    unsigned lcore_id; // can be an arbitrary value (does not have to start from 0 or 1)
    workers_job_init(&app_settings.dpdk, &app_settings, mbuf_pool);
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        //printf("Launching worker %u, LCORE_ID: %u\n", i, lcore_id);
        if (workers_job_assign(lcore_id, &lcore_args[i])) {
            rte_eal_remote_launch(lcore_main, &lcore_args[i], lcore_id);
        }
        i++;
    }

    // Main core start.
    rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "INFO: Start main core.\n");
    while (!stop_loop()) {
        gather_stats(&measurement);
    }

    // Wait for all worker lcores to finish.
    rte_eal_mp_wait_lcore();

    // Print stats for the last time.
    printf("\nApplication is ending...\nStats:\n");
    printf("=====================================================\n");
    print_stats_total(&measurement);
    printf("=====================================================\n");

    print_simple_stats(&measurement);

    cleanup(app_settings.dpdk.ports);
    cleanup_measurement(&measurement);

    return 0;
}
