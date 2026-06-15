/* common_pcap.h: Common PCAP functions used in DynaNIC examples.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, January 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_RECEIVE_COMMON__
#define __DYNANIC_RECEIVE_COMMON__

#include <rte_ethdev.h>

struct pcap_hdr_s
{
	uint32_t magic_number;  // magic number
	uint16_t version_major; // major version number
	uint16_t version_minor; // minor version number
	int32_t thiszone;       // GMT to local correction
	uint32_t sigfigs;       // accuracy of timestamps
	uint32_t snaplen;       // max length of captured packets, in octets
	uint32_t network;       // data link type
} __attribute__ ((packed));

struct packet_header
{
	uint32_t ts_sec;        // timestamp in seconds
	uint32_t ts_nsec;       // timestamp in microseconds
	uint32_t incl_len;      // number of octets of packet saved in file
	uint32_t orig_len;      // actual length of packet
} __attribute__ ((packed));

/**
 * @brief Prepare / create a new PCAP file.
 *
 * Creates new PCAP file incl. valid PCAP header,
 * so that the file is ready to store packets.
 *
 * @param file       Pointer to FILE pointer, where the new
 *                   handler will be written.
 * @param file_name  Filename of the new PCAP file.
 *
 * @return 0 on success, a negative value (-1) in case of failure.
 */
int prepare_pcap_file(FILE **file, char file_name[50]);

/**
 * @brief Store packet to PCAP file.
 *
 * Saves packet to given PCAP file.
 * The file (handler) must be an open, valid PCAP file.
 * Use prepare_pcap_file to create new PCAP file.
 *
 * It is possible to limit the total number of packets in the file.
 *
 * @param file                   Pointer to target FILE handler.
 * @param current_packets        Pointer to number of written packets.
 * @param max_packets_per_queue  Packet count threshold.
 * @param check_packet_th        Whether to enable packet count check.
 *
 * @return  0 on success.
 *         -1 in case of failure.
 *         -2 when the threshold packet cound is reached.
 */
int save_packet(struct rte_mbuf *buf,
    FILE *file,
    int *current_packets,
	int max_packets_per_queue,
	bool check_packet_th
);

#endif //__DYNANIC_RECEIVE_COMMON__
