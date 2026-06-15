/* common_stats.c: Functions for statistics printing in DynaNIC examples.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, April 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "common_stats.h"

#include <sys/time.h>

#include <rte_ethdev.h>

#define GIGA 1000000000
#define ETH_OVERHEAD 24 // CRC(4) + Preambule(8) + IFG(12)
#define BIT 8
#define CRC 4

void cleanup_measurement(struct measurement_settings *measurement)
{
    free(measurement->pre_bytes);
    free(measurement->pre_packets);
    free(measurement->pre_time);
}

void print_simple_stats(struct measurement_settings *measurement)
{
    uint64_t bytes = 0;
    uint64_t packets = 0;
    uint16_t portid = 0;
    struct rte_eth_stats stats;

    // Get data for each port.
    RTE_ETH_FOREACH_DEV(portid) {
        rte_eth_stats_get(portid, &stats);
        packets += measurement->print_rx ? stats.ipackets : stats.opackets;
        bytes +=   measurement->print_rx ? stats.ibytes   : stats.obytes;
    }

    printf("Packets: %" PRIu64 ", Bytes: %" PRIu64 "\n", packets, bytes);
}

uint64_t get_l2_bits(uint64_t bytes, uint64_t packets)
{
    return (bytes + CRC * packets) * BIT;
}

uint64_t get_l1_bits(uint64_t bytes, uint64_t packets)
{
    return (bytes + packets * ETH_OVERHEAD) * BIT;
}

uint64_t get_dma_bits(uint64_t bytes)
{
    return bytes * BIT;
}

void print_queue_xstats(uint16_t port_id)
{
    struct rte_eth_xstat_name *xstats_names;
    struct rte_eth_xstat *xstats;
    int len, i;
    int ret;

    printf("\nFetching xstats for Port %u...\n", port_id);

    // Get the number of xstats
    len = rte_eth_xstats_get_names(port_id, NULL, 0);
    if (len < 0) {
        printf("Error getting xstats count\n");
        return;
    }

    // Allocate memory for names and values
    xstats_names = malloc(sizeof(struct rte_eth_xstat_name) * len);
    xstats = malloc(sizeof(struct rte_eth_xstat) * len);
    if (xstats_names == NULL || xstats == NULL) {
        printf("Failed to allocate memory for xstats\n");
        free(xstats_names);
        free(xstats);
        return;
    }

    // Retrieve names
    ret = rte_eth_xstats_get_names(port_id, xstats_names, len);
    if (ret != len) {
        printf("Error retrieving xstats names\n");
        free(xstats_names);
        free(xstats);
        return;
    }

    // Retrieve values
    ret = rte_eth_xstats_get(port_id, xstats, len);
    if (ret != len) {
        printf("Error retrieving xstats values\n");
        free(xstats_names);
        free(xstats);
        return;
    }

    // Print  counters
    printf("%-30s | %s\n", "Counter Name", "Value");
    printf("--------------------------------------------------\n");

    int queue_stats_found = 0;
    for (i = 0; i < len; i++) {
        printf("%-30s | %"PRIu64"\n",
            xstats_names[i].name,
            xstats[i].value);
        queue_stats_found++;
    }

    if (queue_stats_found == 0) {
        printf("No queue specific xstats found. Ensure the PMD supports them.\n");
    }

    free(xstats_names);
    free(xstats);
}

void print_stats(struct measurement_settings *measurement)
{
    uint64_t bytes_per_window;
    uint64_t current_bytes;
    uint64_t current_packets;
    uint64_t current_time;
    uint64_t packets_per_window;
    uint16_t portid = 0;
    struct rte_eth_stats stats;
    uint64_t time_per_window;
    struct timeval current_time_raw;

    RTE_ETH_FOREACH_DEV(portid) {
        rte_eth_stats_get(portid, &stats);
        gettimeofday(&current_time_raw, NULL);

        current_time = current_time_raw.tv_sec * 1000 + current_time_raw.tv_usec / 1000;
        current_packets = measurement->print_rx ? stats.ipackets : stats.opackets;
        current_bytes =   measurement->print_rx ? stats.ibytes : stats.obytes;

        bytes_per_window = current_bytes - measurement->pre_bytes[portid];
        packets_per_window = current_packets - measurement->pre_packets[portid];
        time_per_window = (current_time - measurement->pre_time[portid]) / 1000;

        /* To avoid division by 0. */
        if (time_per_window == 0) {
            time_per_window = 1;
        }

        printf("~~~ PORT %" PRIu16":\n", portid);
        printf("Packets per second: \t\t%" PRIu64 "\n", packets_per_window / time_per_window);
        printf("L1 Bits per second [bps]: \t%" PRIu64 "\n", get_l1_bits(bytes_per_window, packets_per_window) / time_per_window);
        printf("L2 Bits per second [bps]: \t%" PRIu64 "\n",  get_l2_bits(bytes_per_window, packets_per_window) / time_per_window);
        printf("DMA Bits per second [bps]: \t%" PRIu64 "\n", get_dma_bits(bytes_per_window) / time_per_window);
        printf("Time [s]: \t\t\t%" PRIu64 "\n", time_per_window);

        measurement->pre_time[portid] = current_time;
        measurement->pre_packets[portid] = current_packets;
        measurement->pre_bytes[portid] = current_bytes;
    }
}

void print_stats_total(struct measurement_settings *measurement)
{
    uint64_t bytes = 0;
    uint64_t current_time;
    struct timeval current_time_raw;
    uint64_t measurement_time;
    uint64_t packets = 0;
    uint16_t portid = 0;
    struct rte_eth_stats stats;

    // Get data from each port.
    RTE_ETH_FOREACH_DEV(portid) {
        rte_eth_stats_get(portid, &stats);
        packets += measurement->print_rx ? stats.ipackets : stats.opackets;
        bytes +=   measurement->print_rx ? stats.ibytes : stats.obytes;
    }

    gettimeofday(&current_time_raw, NULL);

    current_time = current_time_raw.tv_sec * 1000 + current_time_raw.tv_usec / 1000;
    measurement_time = (current_time - measurement->start_time) / 1000;

    // To avoid division by 0.
    if (measurement_time == 0) {
        measurement_time = 1;
    }

    printf("Packets: \t\t\t%" PRIu64 "\n", packets);
    printf("L1 Bits [b]: \t\t\t%" PRIu64 "\n", get_l1_bits(bytes, packets));
    printf("L2 Bits [b]: \t\t\t%" PRIu64 "\n", get_l2_bits(bytes, packets));
    printf("DMA Bits [b]: \t\t\t%" PRIu64 "\n", get_dma_bits(bytes));
    printf("Time [s]: \t\t\t%" PRIu64 "\n", measurement_time);

    printf("Average speed (on DMA) [Gbps]: \t%0.02f\n", (float)get_dma_bits(bytes) / measurement_time / GIGA);
}

int time_variables_init(struct measurement_settings *measurement)
{
    measurement->max_ports = 10;
    measurement->next_stats_time = 0;

    // Get current time and set it as start of measurement.
    struct timeval current_time_raw;
    gettimeofday(&current_time_raw, NULL);

    measurement->pre_bytes = malloc(measurement->max_ports * sizeof(uint64_t));
    if (measurement->pre_bytes == NULL) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: malloc() failed.\n");
        return -1;
    }
    measurement->pre_packets = malloc(measurement->max_ports * sizeof(uint64_t));
    if (measurement->pre_packets == NULL) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: malloc() failed.\n");
        return -1;
    }
    measurement->pre_time = malloc(measurement->max_ports * sizeof(uint64_t));
    if (measurement->pre_time == NULL) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: malloc() failed.\n");
        return -1;
    }

    memset(measurement->pre_bytes, 0, measurement->max_ports * sizeof(uint64_t));
    memset(measurement->pre_packets, 0, measurement->max_ports * sizeof(uint64_t));
    memset(measurement->pre_time, 0, measurement->max_ports * sizeof(uint64_t));

    for (int i = 0; i < measurement->max_ports; i++) {
        measurement->pre_time[i] = current_time_raw.tv_sec * 1000 + current_time_raw.tv_usec / 1000;
    }
    measurement->start_time = current_time_raw.tv_sec * 1000 + current_time_raw.tv_usec / 1000;

    return 0;
}

void gather_stats(struct measurement_settings *measurement)
{
    struct timeval current_time_raw;
    uint64_t now;

    gettimeofday(&current_time_raw, NULL);
    now = current_time_raw.tv_sec * 1000 + current_time_raw.tv_usec / 1000;

    if (measurement->next_stats_time < now) {
        printf("---------------------------------------------------\n");
        print_stats(measurement);
        measurement->next_stats_time = now + measurement->next_stats_delay;
    }
}
