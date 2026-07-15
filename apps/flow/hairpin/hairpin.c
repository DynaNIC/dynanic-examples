/* hairpin.c: Demonstrate hardware hairpin queues and rte_flow traffic splitting.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Denis Kurka <kurka@dyna-nic.com>, May 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#define ALLOW_EXPERIMENTAL_API

#include "common_dpdk.h"
#include "argparse.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_cycles.h>
#include <rte_errno.h>

#define PORT_IN_ID 0
#define PORT_OUT_ID 1

// Structure to pass arguments to the worker core
struct worker_args {
    struct rte_mempool *mbuf_pool;
    struct application_settings *app_settings;
};

/**
 * Creates 2 flow rules:
 * 1. Odd IPs  -> Hairpin (HW forwarding)
 * 2. Even IPs -> SW (CPU processing)
 */
static int create_split_flows(uint16_t port_id, uint16_t sw_q, uint16_t hp_q)
{
    struct rte_flow_attr attr = { .ingress = 1 };
    struct rte_flow_error error;

    // Mask for the very last bit of the IP address (0x00000001)
    struct rte_flow_item_ipv4 ipv4_mask = { .hdr.dst_addr = rte_cpu_to_be_32(0x00000001) };

    printf("\n--- CREATING HW RULES ---\n");

    // --- RULE 1: Odd Destinations -> Hairpin ---
    struct rte_flow_item_ipv4 ipv4_spec_odd = { .hdr.dst_addr = rte_cpu_to_be_32(0x00000001) };
    struct rte_flow_item pattern_odd[] = {
        { .type = RTE_FLOW_ITEM_TYPE_ETH },
        { .type = RTE_FLOW_ITEM_TYPE_IPV4, .spec = &ipv4_spec_odd, .mask = &ipv4_mask },
        { .type = RTE_FLOW_ITEM_TYPE_END }
    };
    struct rte_flow_action_queue queue_odd = { .index = hp_q };
    struct rte_flow_action actions_odd[] = {
        { .type = RTE_FLOW_ACTION_TYPE_QUEUE, .conf = &queue_odd },
        { .type = RTE_FLOW_ACTION_TYPE_END }
    };

    if (!rte_flow_create(port_id, &attr, pattern_odd, actions_odd, &error)) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "RTE_FLOW ERROR (Odd IPs): %s\n", error.message);
        return -1;
    }
    printf("FLOW CREATED: Odd IPs  (e.g., 10.0.0.1) -> Hairpin queue %d\n", hp_q);

    // --- RULE 2: Even Destinations -> SW ---
    struct rte_flow_item_ipv4 ipv4_spec_even = { .hdr.dst_addr = rte_cpu_to_be_32(0x00000000) };
    struct rte_flow_item pattern_even[] = {
        { .type = RTE_FLOW_ITEM_TYPE_ETH },
        { .type = RTE_FLOW_ITEM_TYPE_IPV4, .spec = &ipv4_spec_even, .mask = &ipv4_mask },
        { .type = RTE_FLOW_ITEM_TYPE_END }
    };
    struct rte_flow_action_queue queue_even = { .index = sw_q };
    struct rte_flow_action actions_even[] = {
        { .type = RTE_FLOW_ACTION_TYPE_QUEUE, .conf = &queue_even },
        { .type = RTE_FLOW_ACTION_TYPE_END }
    };

    if (!rte_flow_create(port_id, &attr, pattern_even, actions_even, &error)) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "RTE_FLOW ERROR (Even IPs): %s\n", error.message);
        return -1;
    }
    printf("FLOW CREATED: Even IPs (e.g., 10.0.0.2) -> SW queue      %d\n", sw_q);
    printf("---------------------------------------------------\n");

    return 0;
}

// Generate a simple IPv4 packet
static struct rte_mbuf *generate_ipv4_pkt(struct rte_mempool *mp, uint32_t dst_ip)
{
    struct rte_mbuf *m = rte_pktmbuf_alloc(mp);
    if (m == NULL) rte_exit(EXIT_FAILURE, "Not enough memory for MBUF allocation!\n");

    uint16_t pkt_len = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr);
    char *data = rte_pktmbuf_append(m, pkt_len);
    if (data == NULL) rte_exit(EXIT_FAILURE, "Failed to append data to MBUF!\n");

    struct rte_ether_hdr *eth_hdr = (struct rte_ether_hdr *)data;
    memset(eth_hdr, 0, sizeof(*eth_hdr));
    memset(&eth_hdr->dst_addr, 0xFF, RTE_ETHER_ADDR_LEN); // Broadcast MAC
    memset(&eth_hdr->src_addr, 0xAA, RTE_ETHER_ADDR_LEN); // Fake Source MAC
    eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
    memset(ip_hdr, 0, sizeof(*ip_hdr));
    ip_hdr->version_ihl = 0x45;
    ip_hdr->total_length = rte_cpu_to_be_16(sizeof(struct rte_ipv4_hdr));
    ip_hdr->time_to_live = 64;
    ip_hdr->next_proto_id = IPPROTO_UDP;
    ip_hdr->dst_addr = rte_cpu_to_be_32(dst_ip);

    return m;
}

/**
 * Configure hardware ports, standard queues, hairpin queues,
 * bind them together and apply rte_flow rules.
 */
static void setup_ports(struct rte_mempool *mbuf_pool, struct application_settings *app_set)
{
    uint16_t total_queues = app_set->nb_std_queues + app_set->nb_hp_queues;
    struct rte_eth_conf port_conf = {0};

    if (rte_eth_dev_configure(PORT_IN_ID, total_queues, total_queues, &port_conf) != 0)
        rte_exit(EXIT_FAILURE, "Failed to configure Port 0\n");

    if (rte_eth_dev_configure(PORT_OUT_ID, total_queues, total_queues, &port_conf) != 0)
        rte_exit(EXIT_FAILURE, "Failed to configure Port 1\n");

    // 1. Setup Standard (SW) queues
    for (uint16_t q = 0; q < app_set->nb_std_queues; q++) {
        rte_eth_rx_queue_setup(PORT_IN_ID, q, app_set->nb_desc, rte_eth_dev_socket_id(PORT_IN_ID), NULL, mbuf_pool);
        rte_eth_tx_queue_setup(PORT_IN_ID, q, app_set->nb_desc, rte_eth_dev_socket_id(PORT_IN_ID), NULL);
        rte_eth_rx_queue_setup(PORT_OUT_ID, q, app_set->nb_desc, rte_eth_dev_socket_id(PORT_OUT_ID), NULL, mbuf_pool);
        rte_eth_tx_queue_setup(PORT_OUT_ID, q, app_set->nb_desc, rte_eth_dev_socket_id(PORT_OUT_ID), NULL);
    }

    // 2. Setup Hairpin queues
    for (uint16_t q = app_set->nb_std_queues; q < total_queues; q++) {
        struct rte_eth_hairpin_conf rx_conf = { .peer_count = 1, .manual_bind = 1, .tx_explicit = 1, .peers[0] = { PORT_OUT_ID, q } };
        struct rte_eth_hairpin_conf tx_conf = { .peer_count = 1, .manual_bind = 1, .tx_explicit = 1, .peers[0] = { PORT_IN_ID, q } };
        rte_eth_rx_hairpin_queue_setup(PORT_IN_ID, q, app_set->nb_desc, &rx_conf);
        rte_eth_tx_hairpin_queue_setup(PORT_OUT_ID, q, app_set->nb_desc, &tx_conf);
    }

    rte_eth_dev_start(PORT_IN_ID);
    rte_eth_dev_start(PORT_OUT_ID);
    rte_eth_hairpin_bind(PORT_IN_ID, PORT_OUT_ID);

    // Create flow rules - index 0 for SW, index nb_std_queues for Hairpin
    if (create_split_flows(PORT_IN_ID, 0, app_set->nb_std_queues) != 0) {
        rte_exit(EXIT_FAILURE, "FATAL: Failed to create Flow rules.\n");
    }
}

/**
 * Main worker function launched on a separate lcore.
 * Handles packet generation, sending, receiving and validation.
 */
static int lcore_main(void *arg)
{
    struct worker_args *args = (struct worker_args *)arg;
    struct rte_mempool *mbuf_pool = args->mbuf_pool;
    struct application_settings *app_set = args->app_settings;

    printf("\n=== AUTO TEST START (Running on LCORE %u) ===\n", rte_lcore_id());
    printf("Sending %d packets to PMA loopback on Port 0...\n", app_set->packets_to_send);

    struct rte_mbuf **tx_bufs = malloc(app_set->packets_to_send * sizeof(struct rte_mbuf *));
    int expected_sw = 0;
    int expected_hp = 0;
    uint16_t sent_total = 0;

    struct rte_mbuf *temp_bufs[app_set->dpdk.burst_size];

    // 1. GENERATE AND SEND IN SMALL BATCHES (CHUNKS OF 32)
    for (int i = 0; i < app_set->packets_to_send; i += app_set->dpdk.burst_size) {
        uint16_t chunk_size = RTE_MIN((uint16_t)app_set->dpdk.burst_size, (uint16_t)(app_set->packets_to_send - i));

        // Allocate only a small batch of packets (we won't deplete the mempool)
        for (uint16_t j = 0; j < chunk_size; j++) {
            int pkt_idx = i + j;
            if (pkt_idx % 2 == 0) {
                temp_bufs[j] = generate_ipv4_pkt(mbuf_pool, RTE_IPV4(10, 0, 0, 2)); // Even -> SW
                expected_sw++;
            } else {
                temp_bufs[j] = generate_ipv4_pkt(mbuf_pool, RTE_IPV4(10, 0, 0, 3)); // Odd -> Hairpin
                expected_hp++;
            }
        }

        // Sending a batch (with a retry mechanism in case the TX ring is full)
        uint16_t sent = 0;
        int retries = 0;
        while (sent < chunk_size && retries < 1000) {
            uint16_t nb_tx = rte_eth_tx_burst(PORT_IN_ID, 0, &temp_bufs[sent], chunk_size - sent);
            sent += nb_tx;
            if (sent < chunk_size) {
                rte_delay_us(10);
                retries++;
            }
        }

        // If we still weren't able to send everything, we need to free the remaining mbufs (to prevent leaks!)
        if (unlikely(sent < chunk_size)) {
            printf("WARNING: TX queue full, freeing %u unsent packets\n", chunk_size - sent);
            for (uint16_t j = sent; j < chunk_size; j++) {
                int pkt_idx = i + j;
                if (pkt_idx % 2 == 0) expected_sw--;
                else expected_hp--;
                rte_pktmbuf_free(temp_bufs[j]);
            }
        }
        sent_total += sent;
    }

    printf("Packets sent successfully: %u (Even/SW: %d, Odd/Hairpin: %d)\n", sent_total, expected_sw, expected_hp);

    rte_delay_ms(100);

    // 2. RECEIVE LOOPS (We read until we have received all expected software packets or a timeout occurs)
    struct rte_mbuf *rx_bufs[app_set->dpdk.burst_size];
    int sw_received = 0;
    int error_leaked = 0;

    uint64_t start_tsc = rte_get_tsc_cycles();
    uint64_t timeout_ticks = rte_get_timer_hz() * 2; // Timeout 2 seconds

    while (sw_received < expected_sw && (rte_get_tsc_cycles() - start_tsc) < timeout_ticks) {
        for (uint16_t q = 0; q < app_set->nb_std_queues; q++) {
            uint16_t nb_rx = rte_eth_rx_burst(PORT_IN_ID, q, rx_bufs, app_set->dpdk.burst_size);
            for (uint16_t i = 0; i < nb_rx; i++) {
                struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(rx_bufs[i], struct rte_ether_hdr *);
                struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
                uint32_t ip = rte_be_to_cpu_32(ip_hdr->dst_addr);

                if ((ip & 1) == 1) {
                    printf("ERROR: Odd packet leaked into SW queue!\n");
                    error_leaked++;
                } else {
                    sw_received++;
                }
                rte_pktmbuf_free(rx_bufs[i]);
            }
        }
        rte_delay_us(10);
    }

    printf("\n=== TEST RESULTS ===\n");
    printf("SW received valid even packets: %d / %d\n", sw_received, expected_sw);
    printf("Odd packets leaked to SW: %d (expected 0)\n", error_leaked);
    printf("Expected hairpinned packets on Port 1: %d\n", expected_hp);

    if (sw_received == expected_sw && error_leaked == 0) {
        printf("\n>>> TEST PASSED: Hairpin split is working! <<<\n");
    } else {
        printf("\n>>> TEST FAILED: Timeout or packet loss! <<<\n");
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct application_settings app_settings;

    // Initialize EAL
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) return -1;

    argc -= ret;
    argv += ret;

    // Parse Arguments using your framework
    if (parse_custom_args(argc, argv, &app_settings) < 0) {
        rte_exit(EXIT_FAILURE, "FATAL: Failed to parse arguments.\n");
    }

    if (rte_eth_dev_count_avail() < 2) {
        rte_exit(EXIT_FAILURE, "FATAL: DPDK detected less than 2 available ports!\n");
    }

    // Initialize Mempool using common_dpdk guidelines
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

    // Setup Hardware Ports & Queues
    setup_ports(mbuf_pool, &app_settings);

    // Package arguments for the worker core
    struct worker_args w_args = {
        .mbuf_pool = mbuf_pool,
        .app_settings = &app_settings
    };

    // Check and warn about unused lcores
    if (rte_lcore_count() > 1) {
        rte_log(RTE_LOG_WARNING, RTE_LOGTYPE_USER1,
            "Multiple lcores detected (count: %u). "
            "This Hairpin auto-test runs on the main lcore only. "
            "Other lcores will remain idle.\n", rte_lcore_count());
    }

    // Execute the test directly on the main lcore
    lcore_main(&w_args);

    // Hairpin queues must be unbound before the ports are stopped.
    rte_eth_hairpin_unbind(PORT_IN_ID, PORT_OUT_ID);
    rte_eth_hairpin_unbind(PORT_OUT_ID, PORT_IN_ID);

    cleanup(2);

    return 0;
}