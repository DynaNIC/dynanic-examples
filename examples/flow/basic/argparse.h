/* argparse.h: Argument parser for flow example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, December 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_FLOW_ARGPARSE__
#define __DYNANIC_FLOW_ARGPARSE__

#include <rte_ethdev.h>

#include <getopt.h>

struct application_settings {
    /* Flag for mode of printing. */
    bool measurement;

    /* flag for RTE Flow. */
    bool disable_rte_flow;

    /* Variables for mempool setup. */
    uint64_t burst_size;
    uint64_t descriptors;
    uint64_t mempool_cache_size;
    uint64_t mbuf_cnt_per_queue;
    uint64_t mbuf_size;

    /* Number of started ports. */
    uint16_t started_ports;

    /* Which example to use. */
    uint8_t example_part_number;
};

int parse_custom_args(
    int argc,
    char *argv[],
    struct application_settings *app_settings
);
void print_help();

#endif // __DYNANIC_FLOW_ARGPARSE__
