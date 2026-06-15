/* flow_control.c: Define flow rules for flow example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, December 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "flow_control.h"

#define MAX_PATTERN_NUM	3
#define MAX_ACTION_NUM 3

int create_flow(uint16_t portid, struct application_settings *app_settings) {
    struct rte_flow *flow = NULL;
    struct rte_flow *flow_pass;
    struct rte_flow_error error;

    if (app_settings->example_part_number == PART1) {
        flow = create_flow_drop_odd(portid, &error);
        if (!app_settings->measurement) {
            rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "INFO: Packets with odd IPv4 dst address will dropped.\n");
        }
    }
    else if (app_settings->example_part_number == PART2) {
        flow = create_forward_odd(portid, &error);
        if (!app_settings->measurement) {
            rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "INFO: Packets with odd IPv4 dst address will be forwarded to the port 1.\n");
        }
    }
    else if (app_settings->example_part_number == PART3) {
        flow = create_part3_example(portid, &error);
        if (!app_settings->measurement) {
            rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "INFO: Packets with odd IPv4 dst address will tagged, processed by group 1 and forwarded to the port 1.\n");
        }
    }

    if (!flow) {
        rte_log(
            RTE_LOG_ERR, RTE_LOGTYPE_EAL,
            "ERROR: Creating rule for example %d failed: %s (%d).\n",
            app_settings->example_part_number,
            error.message,
            error.type
        );
        return -1;
    }

    if (!app_settings->measurement) {
        rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "INFO: Packets with even IPv4 dst address will be send to queue 0.\n");
    }
    flow_pass = create_flow_pass_even(portid, &error);
    if (!flow_pass) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: create_flow_pass_even() failed: %s (%d).\n", error.message, error.type);
        return -1;
    }

    return 0;
}

/* Create flow rule to forward packets with even dst IP address. */
struct rte_flow* create_flow_pass_even(uint16_t portid, struct rte_flow_error *error) {
    struct rte_flow_attr attr = { .ingress = 1 };
    struct rte_flow_item pattern[MAX_PATTERN_NUM];
    struct rte_flow_action actions[MAX_ACTION_NUM];
    struct rte_flow_item_ipv4 ipv4 = {0};
    struct rte_flow_item_ipv4 ipv4_mask = {0};
    struct rte_flow *flow = NULL;

    /* Parameter for queue action. */
    struct rte_flow_action_queue queue = { .index = 0 };

    /* Pattern specification. */
    ipv4.hdr.dst_addr = htonl(0x00000000); // 0.0.0.0
    ipv4_mask.hdr.dst_addr = htonl(0x00000001); // 0.0.0.1

    /* Match IP header. */
    pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
    pattern[0].spec = &ipv4;
    pattern[0].last = NULL;
    pattern[0].mask = &ipv4_mask;

    /* End the pattern array. */
    pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

    /* Create the queue action. */
    actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
    actions[0].conf  = &queue;

    actions[1].type = RTE_FLOW_ACTION_TYPE_END;

    /* Validate and create the flow rule. */
    flow = rte_flow_create(portid, &attr, pattern, actions, error);

    return flow;
}

int destroy_all_flow_rules(uint16_t portid) {
    struct rte_flow_error error;
    int ret = 0;

    ret = rte_flow_flush(portid, &error);
    if (ret != 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_flow_flush() failed: %s (%d).\n", error.message, error.type);
        return -1;
    }

    return 0;
}
