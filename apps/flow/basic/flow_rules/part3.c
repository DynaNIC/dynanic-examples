/* part3.c: Define flow rules for flow example part3.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, December 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "part3.h"

#define MAX_PATTERN_NUM	3
#define MAX_ACTION_NUM 3

struct rte_flow* create_part3_example(uint16_t portid, struct rte_flow_error *error) {
    struct rte_flow *flow = NULL;
    struct rte_flow *flow_match_tag;

    flow = create_tag_jump(portid, error);
    if (!flow) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: create_tag_jump() failed: %s (%d).\n", error->message, error->type);
        return NULL;
    }

    flow = create_match_tag(portid, error);
    if (!flow) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: create_match_tag() failed: %s (%d).\n", error->message, error->type);
        return NULL;
    }

    return flow;
}

/* Match packet with odd IPv4 destination address, tag it and jump to the group 3. */
struct rte_flow* create_tag_jump(uint16_t portid, struct rte_flow_error *error) {
    struct rte_flow_action actions[MAX_ACTION_NUM];
    struct rte_flow_attr attr = {
        .ingress = 1,
    };
    struct rte_flow *flow = NULL;
    struct rte_flow_item_ipv4 ipv4 = {0};
    struct rte_flow_item_ipv4 ipv4_mask = {0};
    struct rte_flow_action_jump jump_to = {
        .group = 3
    };
    struct rte_flow_item pattern[MAX_PATTERN_NUM];
    int ret;
    struct rte_flow_action_set_tag tag = {
        .index = 0,
        .data = 123
    };

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

    /* Create the action. */
    actions[0].type = RTE_FLOW_ACTION_TYPE_SET_TAG;
    actions[0].conf  = &tag;

    /* Jump to different group. */
    actions[1].type = RTE_FLOW_ACTION_TYPE_JUMP;
    actions[1].conf  = &jump_to;

    /* End action array. */
    actions[2].type = RTE_FLOW_ACTION_TYPE_END;

    /* Validate and create the flow rule. */
    flow = rte_flow_create(portid, &attr, pattern, actions, error);

    return flow;
}

struct rte_flow* create_match_tag(uint16_t portid, struct rte_flow_error *error) {
    struct rte_flow_action actions[MAX_ACTION_NUM];
    struct rte_flow_attr attr = {
        .ingress = 1,
        .transfer = 1,
        .group = 3,
    };
    struct rte_flow_action_ethdev eth_dev = {
        .port_id = 1
    };
    struct rte_flow *flow = NULL;
    struct rte_flow_item pattern[MAX_PATTERN_NUM];
    int ret;
    struct rte_flow_item_tag tag = {
        .index = 0,
        .data = 123,
    };

    /* Match tag. */
    pattern[0].type = RTE_FLOW_ITEM_TYPE_TAG;
    pattern[0].spec = &tag;
    pattern[0].last = NULL;

    /* End the pattern array. */
    pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

    /* Create the action. */
    actions[0].type = RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR;
    actions[0].conf  = &eth_dev;

    /* End action array. */
    actions[1].type = RTE_FLOW_ACTION_TYPE_END;

    /* Validate and create the flow rule. */
    flow = rte_flow_create(portid, &attr, pattern, actions, error);

    return flow;
}
