/* part1.c: Define flow rules for flow example part1.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, December 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "part1.h"

#define MAX_PATTERN_NUM	3
#define MAX_ACTION_NUM 3

/* Create flow rule to drop packets with odd dst IP address. */
struct rte_flow* create_flow_drop_odd(uint16_t portid, struct rte_flow_error *error) {
    struct rte_flow_action actions[MAX_ACTION_NUM];
    struct rte_flow_attr attr = { .ingress = 1 };
    struct rte_flow *flow = NULL;
    struct rte_flow_item_ipv4 ipv4 = {0};
    struct rte_flow_item_ipv4 ipv4_mask = {0};
    struct rte_flow_item pattern[MAX_PATTERN_NUM];

    /* Pattern specification. */
    ipv4.hdr.dst_addr = htonl(0x00000001); // 0.0.0.1
    ipv4_mask.hdr.dst_addr = htonl(0x00000001); // 0.0.0.1

    /* Match IP header. */
    pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
    pattern[0].spec = &ipv4;
    pattern[0].last = NULL;
    pattern[0].mask = &ipv4_mask;

    /* End the pattern array. */
    pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

    /* Create the drop action. */
    actions[0].type = RTE_FLOW_ACTION_TYPE_DROP;

    /* End action array. */
    actions[1].type = RTE_FLOW_ACTION_TYPE_END;

    /* Validate and create the flow rule. */
    flow = rte_flow_create(portid, &attr, pattern, actions, error);

    return flow;
}
