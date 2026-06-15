/* create_packet.c: Packet creating for generate example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "create_packet.h"

int create_packet(char* new_packet, uint16_t packet_len) {
    // All values are here only as an example, all of them can be changed to suite your use case.
    struct rte_ether_addr src_mac;
    struct rte_ether_addr dst_mac = {{20,20,20,20,20,20}};

    memset(new_packet, 0, packet_len);

    // Gets real MAC address of used NIC.
    int ret = rte_eth_macaddr_get(0, &src_mac);
    if (ret != 0) {
        fprintf(stderr,
                   "Failed to get bond (port %u) MAC address: %s\n",
                   0, strerror(-ret));
        return -1;
    }
    // Set ethernet header.
    struct rte_ether_hdr *eth_hdr = (struct rte_ether_hdr*) new_packet;
    rte_ether_addr_copy(&src_mac, &eth_hdr->src_addr);
    rte_ether_addr_copy(&dst_mac, &eth_hdr->dst_addr);
    eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    // Set IPv4 header
    uint16_t ip_len = packet_len - sizeof(struct rte_ipv4_hdr);
    struct rte_ipv4_hdr *ipv4_hdr = (struct rte_ipv4_hdr *)(new_packet + sizeof(struct rte_ether_hdr));
    ipv4_hdr->src_addr = rte_cpu_to_be_32(3232235777);
    ipv4_hdr->dst_addr = rte_cpu_to_be_32(3232235775);
    ipv4_hdr->time_to_live = 64;
    ipv4_hdr->version_ihl = 69;
    ipv4_hdr->total_length = rte_cpu_to_be_16(ip_len);
    ipv4_hdr->hdr_checksum = rte_ipv4_cksum(ipv4_hdr);
}
