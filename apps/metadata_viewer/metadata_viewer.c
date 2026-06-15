/* metadata_viewer.c: Dump metadata and data of received packet.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, April 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "common_dpdk.h"
#include "common_stats.h"

#include "argparse.h"

#include <ctype.h>
#include <signal.h>
#include <stdio.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

/* Flag variable to determine when to stop packet processing. */
static volatile int __stop = 0;

/* Counter for printing packets. */
static int __packet_counter = 0;

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

struct rte_flow *
generate_flow(uint16_t port_id, struct rte_flow_error *error)
{
    /* *************************************************************************** */
    /* Change the content of this function if you want to use your own RTE Flow rule */
    /* ************************************************************************* */

    uint32_t mark_id = 5;
    uint16_t queue_id = 5;

    // Atributes (Ingress, Group, Priority)
    struct rte_flow_attr attr = {
        .ingress = 1,
        .group = 0,
        .priority = 0
    };

    // Patern
    struct rte_flow_item pattern[] = {
        {
            .type = RTE_FLOW_ITEM_TYPE_END
        }
    };

    // Actions
    struct rte_flow_action_mark mark_conf = {
        .id = mark_id
    };

    struct rte_flow_action_queue queue_conf = {
        .index = queue_id
    };

    struct rte_flow_action actions[] = {
        {
            .type = RTE_FLOW_ACTION_TYPE_MARK,
            .conf = &mark_conf
        },
        {
            .type = RTE_FLOW_ACTION_TYPE_QUEUE,
            .conf = &queue_conf
        },
        {
            .type = RTE_FLOW_ACTION_TYPE_END
        }
    };

    // Create rule
    return rte_flow_create(port_id, &attr, pattern, actions, error);
}

void print_packet_info(struct rte_mbuf *buf, uint16_t queueid)
{
    printf("============================================================================================\n");
    printf("Packet: %d\n", __packet_counter++);
    printf("Metadata:\n");
    printf("\tQUEUE: %d\n", queueid);

    if((buf->ol_flags & RTE_MBUF_F_RX_FDIR) != 0) {
        if ((buf->ol_flags & RTE_MBUF_F_RX_FDIR_ID) != 0) {
            printf("Packet is using MARK with ID: %" PRIu32 ", ol flags: %lu\n", buf->hash.fdir.hi, buf->ol_flags);
        } else {
            printf("Packet is using FLAG, ol flags: %lu\n", buf->ol_flags);
        }
    }
    // Packet lenght = Total pkt len: sum of all segments ; Data lenght = Amount of data in segment buffer.
    printf("Packet length: %" PRIu32 ", Data lenght: %" PRIu16 "\n", buf->pkt_len, buf->data_len);
    printf("--------------------------------------------------------------------------------------------\n");

    printf("\n");

    printf("Data:\n");

    // Parse L2
    struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(buf, struct rte_ether_hdr *);
    uint32_t l2_len = sizeof(struct rte_ether_hdr);

    printf("\tEthernet: \n");
    printf("\t\tDST MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", RTE_ETHER_ADDR_BYTES(&eth_hdr->dst_addr));
    printf("\t\tSRC MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", RTE_ETHER_ADDR_BYTES(&eth_hdr->src_addr));
    printf("\t\tETH type: 0x%02X\n", rte_be_to_cpu_16(eth_hdr->ether_type));

    uint16_t ether_type = rte_be_to_cpu_16(eth_hdr->ether_type);

    // Parse VLAN
    int vlan_cnt = 0;
    uint32_t vlan_len;
    while (ether_type == RTE_ETHER_TYPE_VLAN) {
        struct rte_vlan_hdr *vlan_hdr = rte_pktmbuf_mtod_offset(buf, struct rte_vlan_hdr *, l2_len);
        printf("\tVLAN%d\n", vlan_cnt++);
        printf("\t\tvlan_tci: %d\n", rte_be_to_cpu_16(vlan_hdr->vlan_tci));
        printf("\t\teth_proto: 0x%02X\n", rte_be_to_cpu_16(vlan_hdr->eth_proto));
        ether_type = rte_be_to_cpu_16(vlan_hdr->eth_proto);

        l2_len += sizeof(struct rte_vlan_hdr);
    }

    // Parse L3
    struct rte_ipv4_hdr *ipv4_hdr;
    uint32_t l3_len = sizeof(struct rte_ipv4_hdr);

    if (ether_type == RTE_ETHER_TYPE_IPV4) {
        printf("\tIPv4\n");
        ipv4_hdr = rte_pktmbuf_mtod_offset(buf, struct rte_ipv4_hdr *, l2_len);
        printf("\t\tDST IP: %d.%d.%d.%d\n",
            ipv4_hdr->dst_addr & 0xFF,
            (ipv4_hdr->dst_addr >> 8) & 0xFF,
            (ipv4_hdr->dst_addr >> 16) & 0xFF,
            (ipv4_hdr->dst_addr >> 24) & 0xFF
        );
        printf("\t\tSRC IP: %d.%d.%d.%d\n",
            ipv4_hdr->src_addr & 0xFF,
            (ipv4_hdr->src_addr >> 8) & 0xFF,
            (ipv4_hdr->src_addr >> 16) & 0xFF,
            (ipv4_hdr->src_addr >> 24) & 0xFF
        );

        printf("\t\tTotal length: %d\n", rte_be_to_cpu_16(ipv4_hdr->total_length));

        // Parse L4
        uint32_t l4_offset = l2_len + l3_len;
        if (ipv4_hdr->next_proto_id == IPPROTO_UDP) {
            struct rte_udp_hdr *l4 = rte_pktmbuf_mtod_offset(buf, struct rte_udp_hdr *, l4_offset);
            printf("UDP: SRC: %d DST: %d\n", rte_be_to_cpu_16(l4->src_port), rte_be_to_cpu_16(l4->dst_port));
        }

        if (ipv4_hdr->next_proto_id == IPPROTO_TCP) {
            struct rte_tcp_hdr *l4 = rte_pktmbuf_mtod_offset(buf, struct rte_tcp_hdr *, l4_offset);
            printf("TCP: SRC: %d DST: %d\n", rte_be_to_cpu_16(l4->src_port), rte_be_to_cpu_16(l4->dst_port));
        }

        if (ipv4_hdr->next_proto_id == IPPROTO_SCTP) {
            struct rte_sctp_hdr *l4 = rte_pktmbuf_mtod_offset(buf, struct rte_sctp_hdr *, l4_offset);
            printf("SCTP: SRC: %d DST: %d\n", rte_be_to_cpu_16(l4->src_port), rte_be_to_cpu_16(l4->dst_port));
        }
    } else {
        printf("Unsupported eth type: 0x%02X\n", ether_type);
    }

    // Raw
    unsigned char *packet;
    packet = rte_pktmbuf_mtod(buf, unsigned char*);
    const size_t BYTES_PER_LINE = 16;
    char ascii_buf[BYTES_PER_LINE + 1];
    int w, j;
    printf("Raw:\n");
    memset(ascii_buf, 0, sizeof(ascii_buf));

    for (w = 0; w <  buf->data_len; w += BYTES_PER_LINE) {
        // Print offset at line start.
        printf("%08zx  ", w);

        // Print hexa values of bytes.
        for (j = 0; j < BYTES_PER_LINE; ++j) {
            // Add space in the middle for better read-ability.
            if (j == BYTES_PER_LINE / 2) {
                printf(" ");
            }

            if (w + j < buf->data_len) {
                // Print byte as two hexa numbers.
                printf("%02x ", packet[w + j]);
                // Save char for ASCII part or add '.' for unprintable chars.
                ascii_buf[j] = isprint(packet[w + j]) ? packet[w + j] : '.';
            } else {
                // Print space to aligne to line length.
                printf("   "); // 3 spaces as "%02x "
                ascii_buf[j] = ' ';
            }
        }
        ascii_buf[BYTES_PER_LINE] = '\0';

        // Print ASCII for current line.
        printf(" |%s|\n", ascii_buf);
    }

    printf("============================================================================================\n");
}

/* Main function for worker lcore process. */
static int lcore_main(void *arg)
{
    struct lcore_arguments *lcore_args = arg;
    struct application_settings *app_settings = (struct application_settings *)lcore_args->app_settings;
    struct rte_mbuf *bufs[app_settings->dpdk.burst_size];

    // Main loop for packet processing.
    while (!stop_loop()) {
        uint16_t nb_rx;
        uint16_t port_id;
        uint16_t queue_id;

        for (int index = lcore_args->start_index; index < lcore_args->end_index; index++) {
            port_id = index / app_settings->dpdk.queues_per_port;
            queue_id = index % app_settings->dpdk.queues_per_port;

            // Recieve packets from port on given queue.
            nb_rx = rte_eth_rx_burst(port_id, queue_id, bufs, app_settings->dpdk.burst_size);

            for(int i = 0; i < nb_rx; i++) {
                print_packet_info(bufs[i], queue_id);
            }

            // Release read packets.
            for (int i = 0; i < nb_rx; i++) {
                rte_pktmbuf_free(bufs[i]);
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

    measurement->print_rx = true;

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

    // Setup device for RX.
    ret = device_setup(mbuf_pool, &app_settings.dpdk, true, false);
    if (ret != 0) {
        cleanup(app_settings.dpdk.ports);
        cleanup_measurement(&measurement);
        rte_exit(EXIT_FAILURE, "ERROR: device_setup() failed.\n");
    }

    // Setup RTE Flow rules
    if (app_settings.enable_flow) {
        struct rte_flow_error error;
        struct rte_flow *flow;
        flow = generate_flow(0, &error);

        if (!flow) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: generate_flow() failed: %s\n",
                   error.message ? error.message : "Unknown error");
        } else {
            printf("The flow rule was successfully created!\n");
        }
    }

    // Start single worker loop to process all queues
    // (parallel processing in multiple lcores would require
    // output (print) synchronization)
    struct lcore_arguments lcore_args;

    lcore_args.app_settings = &app_settings;
    lcore_args.mbuf_pool = mbuf_pool;
    lcore_args.worker_id = 0;
    lcore_args.start_index = 0;
    lcore_args.end_index = app_settings.dpdk.queues_per_port * app_settings.dpdk.ports;

    lcore_main(&lcore_args);

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
