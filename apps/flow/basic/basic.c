/* basic.c: Main entry point for the basic DPDK RTE Flow offloading example.
*
*           Demonstrates essential DPDK application setup, including EAL initialization,
*           mbuf pool creation, RX queue configuration, and application of basic hardware
*           flow rules (even/odd packet matching). Includes signal handling for graceful shutdown
*           and basic throughput stats logging.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, August 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/


#include "auxiliary.h"

#include <signal.h>

#define MIN_MEMPOOL_SIZE 8196

/* Flag variable to determine when to stop packet processing. */
static volatile int __stop = 0;

/* Handle CTRL+C and kill signals. */
static void handle_sig(int sig) {
    switch (sig) {
    case SIGINT:
    case SIGTERM:
        __stop = 1;
        break;
    }
}

/* Return whether to stop main loop or not. */
static int stop_loop(void) {
    return __stop;
}

/* Main function for worker lcore process. */
static int lcore_main(uint32_t burst_size, uint32_t rx_queues) {
    struct rte_mbuf *bufs[burst_size];
    int port_id = 0;
    int queue_id;
    int i, j;
    uint16_t nb_rx;

    /* Main loop for packet processing. */
    while (!stop_loop()) {
        for (queue_id = 0; queue_id < rx_queues; queue_id++) {
            /* Receive packets from port on given queue. */
            nb_rx = rte_eth_rx_burst(port_id, queue_id, bufs, burst_size);

            for(i = 0; i < nb_rx; i++) {
                rte_pktmbuf_free(bufs[i]);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    // Parse EAL args
    int ret;
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eal_init() failed.\n");
        rte_exit(EXIT_FAILURE, "ERROR: init() failed.\n");
    }
    argc -= ret;
    argv += ret;

    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    /* Here you can add your own arg parsing.   */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

    uint32_t rx_queues = 1;
    uint32_t tx_queues = 0;
    uint16_t ports = 1;
    uint64_t descriptors = 1024;
    uint32_t burst_size = 64;
    uint64_t mempool_cache_size = 512;

    uint64_t mempool_size = RTE_MAX(
        ports * rx_queues * descriptors + \
        ports * rte_lcore_count() * burst_size + \
        ports * tx_queues * descriptors + \
        rte_lcore_count() * mempool_cache_size, MIN_MEMPOOL_SIZE
    );

    // Register signal for loop stopping.
    signal(SIGINT, &handle_sig);
    signal(SIGTERM, &handle_sig);

    // Create mbuf.
    struct rte_mempool *mbuf_pool;
    uint64_t mbuf_size = 1518;
    mbuf_pool = rte_pktmbuf_pool_create(
        "pcap_pool",
        mempool_size,
        mempool_cache_size,
        0,
        mbuf_size,
        rte_socket_id()
    );
    if (mbuf_pool == NULL) {
        rte_exit(-rte_errno, "ERROR: rte_pktmbuf_pool_create() failed.\n");
    }

    // Port configuration.
    struct rte_eth_conf port_conf = {0};
    port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
    port_conf.rxmode.offloads = 0;
    port_conf.rx_adv_conf.rss_conf.rss_key = NULL;
    port_conf.rx_adv_conf.rss_conf.rss_hf = RTE_ETH_RSS_IP;

    uint16_t port_id = 0;
    if (!rte_eth_dev_is_valid_port(port_id))
        rte_exit(EXIT_FAILURE, "ERROR: rte_eth_dev_is_valid_port() failed.\n");

    ret = rte_eth_dev_configure(port_id, rx_queues, tx_queues, &port_conf);
    if (ret != 0)
        rte_exit(EXIT_FAILURE, "ERROR: rte_eth_dev_configure() failed.\n");

    // Setup RX queues.
    for (int queue_id = 0; queue_id < rx_queues; queue_id++) {
        ret = rte_eth_rx_queue_setup(
            port_id,
            queue_id,
            descriptors,
            rte_eth_dev_socket_id(port_id),
            NULL,
            mbuf_pool
        );
        if (ret < 0) {
            rte_exit(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eth_rx_queue_setup() for queue %d and port %d failed\n", queue_id, port_id);
        }
    }

    // Start port.
    ret = rte_eth_dev_start(port_id);
    if (ret < 0) {
        rte_exit(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eth_dev_start() failed for port %" PRIu16 "\n", port_id);
    }

    rte_eth_promiscuous_enable(port_id);

    struct rte_flow_error error;
    uint32_t q_id = 0;
    create_even_rule(port_id, q_id, &error);
    create_odd_rule(port_id, q_id, &error);

    lcore_main(burst_size, rx_queues);

    // Get stats.
    struct rte_eth_stats stats;
    rte_eth_stats_get(port_id, &stats);
    uint64_t packets = stats.ipackets;
    uint64_t bytes = stats.ibytes;

    printf("\nPackets: %" PRIu64 ", Bytes: %" PRIu64 "\n", packets, bytes);

    rte_eth_dev_stop(port_id);
    rte_eal_cleanup();

    return 0;
}
