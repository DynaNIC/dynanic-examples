/* flagging.c: DPDK RTE Flow example for hardware-based packet FLAG and MARK actions.
*              Demonstrates rte_flow rule creation, inspecting mbuf offload flags (FDIR)
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, August 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/


#include "auxiliary.h"
#include "set.h"

#include <signal.h>

#define MIN_MEMPOOL_SIZE 8196

struct viewer_stats {
    struct mark_set *mark;
    uint64_t flag;
};

int count_mark(uint16_t pkt_cnt, uint16_t mark_id, struct viewer_stats *stats) {
    return set_add(stats->mark, mark_id, pkt_cnt);
}

void count_flag(uint16_t pkt_cnt, struct viewer_stats *stats) {
    stats->flag += pkt_cnt;
}

int count_packet(struct application_settings *app_settings, struct rte_mbuf *buf, struct viewer_stats *stats) {
    int nb_rx = 1;

    if((buf->ol_flags & RTE_MBUF_F_RX_FDIR) != 0) {
        if ((buf->ol_flags & RTE_MBUF_F_RX_FDIR_ID) != 0) {
            int ret = count_mark(nb_rx, buf->hash.fdir.hi, stats);
            // Something is wrong with the set, leave.
            if (ret < 0) {
                return ret;
            }
            if (app_settings->debug) {
                printf("Packet is using MARK with ID: %" PRIu32 ", ol flags: %lu\n", buf->hash.fdir.hi, buf->ol_flags);
            }
        } else {
            count_flag(nb_rx, stats);
            if (app_settings->debug) {
                printf("Packet is using FLAG, ol flags: %lu\n", buf->ol_flags);
            }
        }
    }

    return 0;
}

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
static int lcore_main(struct application_settings *app_settings, struct viewer_stats *stats, uint32_t burst_size, uint32_t rx_queues) {
    struct rte_mbuf *bufs[burst_size];
    int port_id;
    int queue_id;
    int i, j;
    uint16_t nb_rx;

    /* Main loop for packet processing. */
    while (!stop_loop()) {
        port_id = app_settings->port_id;
        for (queue_id = 0; queue_id < rx_queues; queue_id++) {
            /* Receive packets from port on given queue. */
            nb_rx = rte_eth_rx_burst(port_id, queue_id, bufs, burst_size);

            for(i = 0; i < nb_rx; i++) {
                if (app_settings->debug) {
                    printf("Packet received at port_id %" PRIu16 " queue_id %" PRIu32 "\n", port_id, queue_id);
                }
                int ret = count_packet(app_settings, bufs[i], stats);
                // Something is wrong with the set, leave.
                rte_pktmbuf_free(bufs[i]);
                if (ret < 0) {
                    break;
                }
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

    // Parse custom args
    struct application_settings app_settings;

    ret = parse_custom_args(argc, argv, &app_settings);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: parse_custom_args() failed.\n");
        return ret;
    }

    uint16_t port_id = app_settings.port_id;
    uint32_t q_id = app_settings.q_id;
    uint32_t rx_queues = app_settings.rx_queues;

    if (q_id > rx_queues) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: cannot use queue %u, maximum is %u: %s\n",
                q_id,
                rx_queues,
                strerror(-ret)
        );

        return ret;
    }

    uint16_t ports = 1;
    uint64_t descriptors = 1024;
    uint32_t burst_size = 64;
    uint64_t mempool_cache_size = 512;
    uint32_t tx_queues = 0;

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

    struct viewer_stats viewer_stats = {
        .mark = set_create(),
        .flag = 0,
    };

    if (viewer_stats.mark == NULL) {
        rte_eth_dev_stop(port_id);
        rte_exit(EXIT_FAILURE, "ERROR: set_create() failed.\n");
    }

    struct rte_flow_error error;
    if (app_settings.use_mark)
        create_mark_rule(port_id, q_id, &error);
    else
        create_flag_rule(port_id, q_id, &error);

    lcore_main(&app_settings, &viewer_stats, burst_size, rx_queues);

    // Get stats.
    struct rte_eth_stats stats;
    rte_eth_stats_get(port_id, &stats);
    uint64_t packets = stats.ipackets;
    uint64_t bytes = stats.ibytes;

    printf("\n");
    if (app_settings.use_mark){
        printf("Marked packets: ");
        print_set(viewer_stats.mark);
    } else {
        printf("Flagged packets: %d\n", viewer_stats.flag);
    }
    printf("\nPackets: %" PRIu64 ", Bytes: %" PRIu64 "\n", packets, bytes);

    set_destroy(viewer_stats.mark);
    rte_eth_dev_stop(port_id);
    rte_eal_cleanup();

    return 0;
}
