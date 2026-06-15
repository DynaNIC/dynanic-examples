/* argparse.h: Argument parser for hairpin example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Denis Kurka <kurka@dyna-nic.com>, May 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_HAIRPIN_ARGPARSE__
#define __DYNANIC_HAIRPIN_ARGPARSE__

#include <stdint.h>
#include "common_dpdk.h"

struct application_settings
{
    // DPDK settings
    struct dpdk_settings dpdk;

    // How often to print stats
    uint64_t next_stats_delay_ms;

    // Hairpin specific settings
    uint16_t nb_std_queues;
    uint16_t nb_hp_queues;
    uint16_t nb_desc;
    uint16_t packets_to_send;
};

int parse_custom_args(int argc, char *argv[], struct application_settings *app_settings);
void print_help(void);

#endif // __DYNANIC_HAIRPIN_ARGPARSE__