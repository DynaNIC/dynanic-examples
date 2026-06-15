/* bonding.c: Generate traffic and send it through a Round-Robin Bonded port.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Denis Kurka <kurka@dyna-nic.com>, April 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "common_dpdk.h"
#include "common_stats.h"

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_eth_bond.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

#include <signal.h>
#include <stdio.h>

#include "argparse.h"

// The memory pool size of underlying DPDK structures for packet processing.
// Should follow 2^n - 1 for optimal memory efficiency (prevents the underlying rte_ring from doubling its allocated size).
// At least 2^19 - 1 (524287) is recommended for this bonding example.
#define MEMPOOL_SIZE 524287

// Flag variable to determine when to stop packet processing.
static volatile int __stop = 0;
static int bond_port_id = -1;

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
        uint16_t queue_id;

        for (int index = lcore_args->start_index; index < lcore_args->end_index; index++) {
            // All traffic is directed ONLY to the bonded port.
            queue_id = index % app_settings->dpdk.queues_per_port;

            // 1. GENERATE & TRANSMIT PACKETS
            if (rte_pktmbuf_alloc_bulk(lcore_args->mbuf_pool, bufs, app_settings->dpdk.burst_size) == 0) {

                for (int i = 0; i < app_settings->dpdk.burst_size; i++) {
                    char *payload = rte_pktmbuf_append(bufs[i], 64);

                    if (likely(payload != NULL)) {
                        struct rte_ether_hdr *eth = (struct rte_ether_hdr *)payload;
                        memset(&eth->dst_addr, 0xFF, RTE_ETHER_ADDR_LEN);
                        memset(&eth->src_addr, 0xAA, RTE_ETHER_ADDR_LEN);
                        eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
                    }
                }

                // Send burst into bonded port
                uint16_t nb_tx = rte_eth_tx_burst(bond_port_id, queue_id, bufs, app_settings->dpdk.burst_size);

                // Free unsent packets
                if (unlikely(nb_tx < app_settings->dpdk.burst_size)) {
                    for (uint16_t buf = nb_tx; buf < app_settings->dpdk.burst_size; buf++) {
                        rte_pktmbuf_free(bufs[buf]);
                    }
                }
            }

            // 2. RECEIVE & DROP PACKETS
            uint16_t nb_rx = rte_eth_rx_burst(bond_port_id, queue_id, bufs, app_settings->dpdk.burst_size);

            if (likely(nb_rx > 0)) {

                /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
                /* Place for your packet processing logic. */
                /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

                // Free received packets (act as a sink)
                for (uint16_t i = 0; i < nb_rx; i++) {
                    rte_pktmbuf_free(bufs[i]);
                }
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

    // Sanitize number of ports
    if (app_settings->dpdk.ports != 1) {
        rte_log(RTE_LOG_WARNING, RTE_LOGTYPE_USER1,
            "Bonding application requires exactly 1 logical application port.\n"
            "Overriding requested parameter '-p %d' to '-p 1'.\n",
            app_settings->dpdk.ports);
        app_settings->dpdk.ports = 1;
    }

    ret = check_worker_cores_cnt();
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: check_worker_cores_cnt() failed.\n");
        return ERROR_DONT_CLEANUP;
    }

    // RX port configuration
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

static int setup_bonded_port(struct application_settings *app_settings,
                             struct rte_mempool *mbuf_pool)
{
    // Hardcoded member ports for simplicity
    uint16_t port0 = 0;
    uint16_t port1 = 1;

    int bond_id = rte_eth_bond_create("net_bonding0", BONDING_MODE_ROUND_ROBIN, rte_socket_id());
    if (bond_id < 0) {
        rte_exit(EXIT_FAILURE, "ERROR: Failed to create bond port.\n");
    }

    // Add members to the bonded pool.
    if (rte_eth_bond_member_add(bond_id, port0) != 0) {
        rte_exit(EXIT_FAILURE, "ERROR: Failed to add port %d to bond.\n", port0);
    }
    if (rte_eth_bond_member_add(bond_id, port1) != 0) {
        rte_exit(EXIT_FAILURE, "ERROR: Failed to add port %d to bond.\n", port1);
    }
    if (rte_eth_bond_primary_set(bond_id, port0) != 0) {
        rte_exit(EXIT_FAILURE, "ERROR: Failed to set primary port.\n");
    }

    rte_eth_dev_configure(bond_id, app_settings->dpdk.queues_per_port,
                        app_settings->dpdk.queues_per_port, &app_settings->dpdk.port_conf);

    for (uint16_t q = 0; q < app_settings->dpdk.queues_per_port; q++) {
        rte_eth_rx_queue_setup(bond_id, q, app_settings->dpdk.descriptors, rte_socket_id(), NULL, mbuf_pool);
        rte_eth_tx_queue_setup(bond_id, q, app_settings->dpdk.descriptors, rte_socket_id(), NULL);
    }

    printf("\nPort %d is virtual bonded port. Port %d + %d\n", bond_id, port0, port1);

    rte_eth_dev_start(bond_id);

    return bond_id;
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

    if (rte_eth_dev_count_avail() < 2) {
        rte_exit(EXIT_FAILURE, "ERROR: Bonding requires atleast 2 ports!\n");
    }

    // Create mbuf.
    app_settings.dpdk.mempool_size = MEMPOOL_SIZE;
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

    // Create bonded port
    bond_port_id = setup_bonded_port(&app_settings, mbuf_pool);

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

    printf("\nApplication is ending...\n");

    rte_eth_dev_stop(bond_port_id);
    rte_eal_cleanup();
    cleanup_measurement(&measurement);

    return 0;
}
