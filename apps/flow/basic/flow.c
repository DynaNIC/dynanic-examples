/* flow.c: Simple read with RTE Flow rules enabled.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, December 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

#include <signal.h>
#include <stdio.h>

#include "argparse.h"
#include "common.h"
#include "configure.h"
#include "flow_control.h"
#include "statistics.h"

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
static int lcore_main(void *arg) {
    struct application_settings *app_settings = arg;
    struct rte_mbuf *bufs[app_settings->burst_size];
    int i;
    struct rte_ipv4_hdr *ipv4_hdr;
    uint16_t nb_rx;
    char* packet;
    uint16_t portid = 0;
    uint16_t queueid = 0;
    int ret;

    /* Main loop for packet processing. */
    while (!stop_loop()) {
        /* Recieve packets from port on given queue. */
        nb_rx = rte_eth_rx_burst(portid, queueid, bufs, app_settings->burst_size);

        /* Print destination IP address of the first recieved packet. */
        if (!app_settings->measurement) {
            if (nb_rx > 0) {
                for (int p = 0; p < nb_rx; p++){
                    packet = rte_pktmbuf_mtod(bufs[p], char*);
                    ipv4_hdr = (struct rte_ipv4_hdr *)(packet + sizeof(struct rte_ether_hdr));
                    printf("DST IP: %d.%d.%d.%d\n",
                        ipv4_hdr->dst_addr & 0xFF,
                        (ipv4_hdr->dst_addr >> 8) & 0xFF,
                        (ipv4_hdr->dst_addr >> 16) & 0xFF,
                        (ipv4_hdr->dst_addr >> 24) & 0xFF
                    );
                }
            }
        }

        /* Release read packets. */
        for (i = 0; i < nb_rx; i++) {
            rte_pktmbuf_free(bufs[i]);
        }
    }
}

int main(int argc, char *argv[]) {
    struct application_settings app_settings;
    int i;
    unsigned lcoreid;
    uint64_t mempool_size;
    struct rte_eth_conf port_conf = {
        .rxmode = {
			.offloads = 0
		},
        .txmode = {
			.offloads = 0
		},
    };
    uint16_t portid;
    uint16_t queues = 1;
    int ret;
    struct rte_mempool *mbuf_pool;
    struct rte_eth_stats stats;

    ret = init(argc, argv, &app_settings);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "ERROR: init() failed.\n");
    }

    /* Register signal for loop stopping. */
    signal(SIGINT, &handle_sig);
    signal(SIGTERM, &handle_sig);

    uint64_t lcore_cnt = rte_lcore_count();
    if (lcore_cnt > 1) {
        if (!app_settings.measurement) {
            rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "INFO: %" PRIu64 " lcores allocated, but only one will be used.\n", lcore_cnt);
        }
    }

    mempool_size = queues * app_settings.mbuf_cnt_per_queue;

    /* Create mempool. */
    mbuf_pool = rte_pktmbuf_pool_create(
        "pcap_pool",
        mempool_size,
        app_settings.mempool_cache_size,
        0,
        app_settings.mbuf_size + RTE_PKTMBUF_HEADROOM,
        rte_socket_id()
    );
    if (mbuf_pool == NULL) {
        rte_exit(-rte_errno, "ERROR: rte_pktmbuf_pool_create() failed.\n");
    }

    /* Setup device for RX. */
    RTE_ETH_FOREACH_DEV(portid){

        ret = rx_device_setup(mbuf_pool, &port_conf, queues, &app_settings, portid);
        if (ret != 0) {
            rte_exit(EXIT_FAILURE, "ERROR: rx_device_setup(%d) failed.\n", portid);
        }
    }

    portid = 0;
    if (!app_settings.disable_rte_flow) {
        ret = create_flow(portid, &app_settings);
        if (ret != 0) {
            rte_exit(EXIT_FAILURE, "ERROR: create_flow_rules() failed.\n");
        }
        if (!app_settings.measurement) {
            rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "INFO: Flow rules created for port %" PRIu16 ".\n", portid);
        }
    }

    lcore_main(&app_settings);

    print_simple_stats();

    cleanup(portid);

    return 0;
}
