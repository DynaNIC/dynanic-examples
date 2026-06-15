/* mark.c: Main for RTE Flow MARK example.
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
static int lcore_main(struct mark_stats *m_stats) {
    int burst_size = 32;
    struct rte_mbuf *bufs[burst_size];
    int i;
    uint16_t nb_rx;
    uint16_t portid = 0;
    uint16_t queueid = 0;

    m_stats->one = 0;
    m_stats->two = 0;
    m_stats->other = 0;

    rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "INFO: lcore for queue %u and port %u started.\n", queueid, portid);

    /* Main loop for packet processing. */
    /* Stop loop either when CTRL+C signal or if threshold is reached. */
    while (!stop_loop()) {
        /* Receive packets from port on given queue. */
        nb_rx = rte_eth_rx_burst(portid, queueid, bufs, burst_size);

        /* Save and release read packets. */
        for (i = 0; i < nb_rx; i++) {
            int mark_id = -1;

            if((bufs[i]->ol_flags & RTE_MBUF_F_RX_FDIR) != 0) {
                if ((bufs[i]->ol_flags & RTE_MBUF_F_RX_FDIR_ID) != 0) {
                    mark_id = bufs[i]->hash.fdir.hi;

                    if(mark_id == MARK_ONE) {
                        // printf("Packet is marked with ONE\n");
                        m_stats->one++;
                    } else if (mark_id == MARK_TWO) {
                        // printf("Packet is marked with TWO\n");
                        m_stats->two++;
                    } else {
                        // printf("Other MARK ID: %" PRIu32"\n", mark_id);
                        m_stats->other++;
                    }
                } else {
                    printf("Unmarked! Flag is used\n");
                }
            } else {
                printf("Unmarked!\n");
            }

            rte_pktmbuf_free(bufs[i]);
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    struct rte_mempool *mbuf_pool;
    uint64_t mbuf_size = 1518;
    uint64_t mempool_cache_size = 512;
    uint64_t mempool_size = 4096;

    uint64_t queues = 1;
    int ret;
    struct mark_stats m_stats;

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
    ret = rx_device_setup(mbuf_pool, port_conf, queues);
    if (ret != 0) {
        cleanup();
        rte_exit(EXIT_FAILURE, "ERROR: rx_device_setup() failed.\n");
    }

    struct rte_flow_error error;
    create_flow(0, &error);

    lcore_main(&m_stats);

    print_simple_stats(&m_stats);

    cleanup();

    return 0;
}
