/* lpm.c: Main for RTE Flow lpm example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, April 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

#include <signal.h>
#include <stdio.h>

#include "auxiliary.h"
#include "rules.h"
#include "common_stats.h"

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
static int lcore_main() {
    int burst_size = 32;
    struct rte_mbuf *bufs[burst_size];
    int i, j;
    uint16_t nb_rx;
    uint16_t portid = 0;
    uint16_t queueid = 0;

    rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "INFO: lcore for queue %u and port %u started.\n", queueid, portid);

    /* Main loop for packet processing. */
    /* Stop loop either when CTRL+C signal or if threshold is reached. */
    while (!stop_loop()) {
        /* Receive packets from port on given queue. */
        for (j = 0; j < QUEUES; j++) {
            nb_rx = rte_eth_rx_burst(portid, j, bufs, burst_size);

            /* Save and release read packets. */
            for (i = 0; i < nb_rx; i++) {
                printf("Packet received at queue %d\n", j);
                rte_pktmbuf_free(bufs[i]);
            }
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    struct rte_mempool *mbuf_pool;
    uint64_t mbuf_size = 1518;
    uint64_t mempool_cache_size = 512;
    uint64_t mempool_size = 4096;

    int ret;

    /* RX port configuration with RSS enabled. */
    struct rte_eth_conf port_conf = {
        .rxmode = {
            .mq_mode = RTE_ETH_MQ_RX_RSS,
            .offloads = 0,
        },
        .rx_adv_conf = {
            .rss_conf = {
                .rss_key = NULL,
                .rss_hf = RTE_ETH_RSS_IP
            },
        },
    };

    ret = init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "ERROR: init() failed.\n");
    }

    /* Register signal for loop stopping. */
    signal(SIGINT, &handle_sig);
    signal(SIGTERM, &handle_sig);

    /* Create mbuf. */
    mbuf_pool = rte_pktmbuf_pool_create(
        "pcap_pool",
        mempool_size,
        mempool_cache_size,
        0,
        mbuf_size + RTE_PKTMBUF_HEADROOM,
        rte_socket_id()
    );
    if (mbuf_pool == NULL) {
        rte_exit(-rte_errno, "ERROR: rte_pktmbuf_pool_create() failed.\n");
    }

    /* Setup device for RX. */
    ret = rx_device_setup(mbuf_pool, port_conf, QUEUES);
    if (ret != 0) {
        cleanup();
        rte_exit(EXIT_FAILURE, "ERROR: rx_device_setup() failed.\n");
    }

    struct rte_flow_error error;
    create_flow(0, &error);

    lcore_main();

    uint16_t port_id = 0;
    RTE_ETH_FOREACH_DEV(port_id){
        print_queue_xstats(port_id);
    }

    cleanup();

    return 0;
}
