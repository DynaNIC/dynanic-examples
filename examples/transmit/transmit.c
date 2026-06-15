/* transmit.c: Transmit packets loaded from PCAP file.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, January 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "common_dpdk.h"
#include "common_stats.h"
#include "common_pcap.h"

#include "argparse.h"

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

#include <signal.h>
#include <stdio.h>

/* Flag variable to determine when to stop packet processing. */
static volatile int __stop = 0;

/* Handle CTRL+C and kill signals. */
static void handle_sig(int sig)
{
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            __stop = 1;
            break;
    }
}

/* Return whether to stop main loop or not. */
static int stop_loop(void)
{
    return __stop;
}

/* Main function for worker lcore process. */
int lcore_main(void *arg)
{
    struct lcore_arguments *lcore_args = arg;
    struct application_settings *app_settings = (struct application_settings *)lcore_args->app_settings;
    struct rte_mempool *mbuf_pool = lcore_args->mbuf_pool;
    struct rte_mbuf *tx_packet;

    struct pcap_hdr_s hdr = {
		.magic_number   = 0xa1b23c4d,
		.version_major  = 2,
		.version_minor  = 4,
		.thiszone       = 0,
		.sigfigs        = 0,
		.snaplen        = 65535,
		.network        = 1 // LINKTYPE_ETHERNET
	};

    struct packet_header phdr;
    char *pcap_file = app_settings->pcap_file;
    char *pkt_data,*pkt;

    FILE *f = fopen(pcap_file, "r");
    if (f == NULL) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: could not open file: %s\n", pcap_file);
		return -1;
	}

    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: could not read PCAP header from %s\n", pcap_file);
		return -1;
	}

    while (!stop_loop()) {
        uint16_t nb_tx = 0;
        uint16_t port_id;
        uint16_t queue_id;

        // Extact packet header.
        int ret = fread(&phdr, sizeof(phdr), 1, f);
        if (ret == 0) {
            // Next data could not be read. Check if it was due to EOF or other error.
            if (feof(f)) {
                // Got EOF, therefore no more packets are available. End transmiting successfully.
                return 0;
            }
            else {
                rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: could not read packet header\n");
                return -1;
            }
        }

        if (phdr.incl_len > app_settings->dpdk.mbuf_size) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "Selected packet length is larger then mbuf size (%d > %d).\n", phdr.incl_len, app_settings->dpdk.mbuf_size);
            return -1;
        }

        // Allocate space for packet data
        pkt_data = malloc(phdr.incl_len);
        if (pkt_data == NULL) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: malloc() failed.\n");
            return -1;
        }

        // Read packet data and add them to the allocated space.
        ret = fread(pkt_data, phdr.incl_len, 1, f);
        if (ret == 0) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: could not read packet data\n");
            return -1;
        }

        int something_send = 0;
        for (int index = lcore_args->start_index; index < lcore_args->end_index; index++) {
            port_id = index / app_settings->dpdk.queues_per_port;
            queue_id = index % app_settings->dpdk.queues_per_port;

            if (app_settings->transmit_single) {
                // if defined send packets only to specific queue
                if (queue_id != app_settings->transmit_queue || port_id != app_settings->transmit_port) {
                    continue;
                }
            }

            something_send = 1;

            // Allocate space for 1 packet.
            ret = rte_pktmbuf_alloc_bulk(mbuf_pool, &tx_packet, 1);
            if (ret != 0) {
                rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_pktmbuf_alloc_bulk() failed.\n");
                return -rte_errno;
            }

            // Copy packet data
            pkt = rte_pktmbuf_append(tx_packet, phdr.incl_len);
            rte_memcpy(pkt, pkt_data, phdr.incl_len);

            nb_tx = rte_eth_tx_burst(port_id, queue_id, &tx_packet, 1);
            if (nb_tx != 1) {
                rte_log(RTE_LOG_WARNING, RTE_LOGTYPE_EAL, "WARNING: could not send the packet\n");
                rte_pktmbuf_free(tx_packet);
            }
        }

        if (!something_send) {
            rte_log(RTE_LOG_WARNING, RTE_LOGTYPE_EAL, "WARNING: lcore %u not sending packets due to active transmit_queue setting\n", rte_lcore_id());
            return 0;
        }
    }

    free(pkt_data);

    return 0;
}

/* Init all necessary components. */
int init(
    int argc, char *argv[],
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

    measurement->print_rx = false;

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
        rte_exit(-rte_errno, "ERROR: rte_pktmbuf_pool_create() failed.\n");
    }

    // Setup device for TX.
    ret = device_setup(mbuf_pool, &app_settings.dpdk, false, true);
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

    // Wait for all worker lcores to finish.
    rte_eal_mp_wait_lcore();

    print_simple_stats(&measurement);

    cleanup(app_settings.dpdk.ports);
    cleanup_measurement(&measurement);

    return 0;
}
