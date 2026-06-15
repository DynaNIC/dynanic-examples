/* common_pcap.c: Common PCAP functions used in DynaNIC examples.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, January 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "common_pcap.h"

int prepare_pcap_file(FILE **file, char file_name[50])
{
    // Courtesy of NDK.
    struct pcap_hdr_s hdr = {
		.magic_number   = 0xa1b23c4d,
		.version_major  = 2,
		.version_minor  = 4,
		.thiszone       = 0,
		.sigfigs        = 0,
		.snaplen        = 65535,
		.network        = 1 // LINKTYPE_ETHERNET
	};

    *file = fopen(file_name, "wb");
    if (*file == NULL) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: fopen() failed.\n");
        return -1;
    }

    if (fwrite(&hdr, sizeof(hdr), 1, *file) != 1) {
		rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "Could not write PCAP header to '%s'.", file_name);
		return -1;
	}

    return 0;
}

int save_packet(struct rte_mbuf *buf,
    FILE *file,
    int *current_packets,
    int max_packets_per_queue,
	bool check_packet_th)
{
    unsigned char *data;
    struct packet_header hdr;

    if (check_packet_th) {
        // Max packets reached.
        if (*current_packets == max_packets_per_queue) {
            return -2;
        }
        else {
            (*current_packets)++;
        }
    }

    data = rte_pktmbuf_mtod(buf, unsigned char*);

    hdr.ts_sec = 0;
    hdr.ts_nsec = 0;
    hdr.orig_len = buf->data_len;
	hdr.incl_len = buf->data_len;

    if (fwrite(&hdr, sizeof(hdr), 1, file) != 1) {
        printf("Failed to write header\n");
        return -1;
    }

    if (fwrite(data, buf->data_len, 1, file) != 1) {
        printf("Failed to write data\n");
        return -1;
    }

    return 0;
}
