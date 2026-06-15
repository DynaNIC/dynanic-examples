/* statistics.c: Functions necessary for stats printing for read example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, December 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "statistics.h"

/* Simple stats in format for easy parsing by measurement script. */
void print_simple_stats() {
    uint64_t bytes = 0;
    uint64_t packets = 0;
    uint16_t portid = 0;
    struct rte_eth_stats stats;

    /* Get data from each port. */
    RTE_ETH_FOREACH_DEV(portid) {
        rte_eth_stats_get(portid, &stats);
        packets += stats.ipackets;
        bytes += stats.ibytes;
    }

    printf("Packets: %" PRIu64 ", Bytes: %" PRIu64 "\n", packets, bytes);
}
