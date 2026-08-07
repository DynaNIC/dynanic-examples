/* auxiliary.h: Header file declaring configuration structures, command-line argument parsing,
*               and helper prototypes for DPDK RTE Flow rule management (MARK and FLAG actions).
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, August 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_FLAGGING_AUX__
#define __DYNANIC_FLAGGING_AUX__

#include <rte_ethdev.h>

#include <getopt.h>

struct application_settings {
    bool use_mark; // If true use mark else use flag

    uint32_t q_id;
    uint32_t rx_queues;
    uint16_t port_id;

	bool debug;
};

int parse_custom_args(
    int argc,
    char *argv[],
    struct application_settings *app_settings
);

void create_mark_rule(uint16_t portid, uint32_t q_id, struct rte_flow_error *error);
void create_flag_rule(uint16_t portid, uint32_t q_id, struct rte_flow_error *error);

#endif //__DYNANIC_FLAGGING_AUX__
