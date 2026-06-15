/* rules.c: RTE Flow rules for MARK example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, April 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "rules.h"

/* ------------------------------------------- */
/* MARK packets with DST IP *.*.*.1 using ID 1*/
/* ----------------------------------------- */
void create_rule1(uint16_t portid, struct rte_flow_error *error) {
    struct rte_flow_attr attr = { .ingress = 1, .priority = 0 };
    struct rte_flow_item pattern[MAX_PATTERN_NUM];
    struct rte_flow_action actions[MAX_ACTION_NUM];
    struct rte_flow_item_ipv4 ipv4 = {0};
    struct rte_flow_item_ipv4 ipv4_mask = {0};
    struct rte_flow *flow = NULL;

    /* Parameter for queue action. */
    struct rte_flow_action_queue queue = { .index = 0 };
    struct rte_flow_action_mark mark = { .id = MARK_ONE };

    /* Pattern specification. */
    ipv4.hdr.dst_addr = inet_addr("0.0.0.1");
    ipv4_mask.hdr.dst_addr = inet_addr("0.0.0.255");

    /* Match IP header. */
    pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
    pattern[0].spec = &ipv4;
    pattern[0].last = NULL;
    pattern[0].mask = &ipv4_mask;

    /* End the pattern array. */
    pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

    /* Create the queue action. */
    actions[0].type = RTE_FLOW_ACTION_TYPE_MARK;
    actions[0].conf  = &mark;

    actions[1].type = RTE_FLOW_ACTION_TYPE_QUEUE;
    actions[1].conf  = &queue;

    actions[2].type = RTE_FLOW_ACTION_TYPE_END;

    /* Validate and create the flow rule. */
    flow = rte_flow_create(portid, &attr, pattern, actions, error);
}

/* ------------------------------------------- */
/* MARK packets with DST IP *.*.*.2 using ID 2*/
/* ----------------------------------------- */
void create_rule2(uint16_t portid, struct rte_flow_error *error) {
    struct rte_flow_attr attr = { .ingress = 1, .priority = 0 };
    struct rte_flow_item pattern[MAX_PATTERN_NUM];
    struct rte_flow_action actions[MAX_ACTION_NUM];
    struct rte_flow_item_ipv4 ipv4 = {0};
    struct rte_flow_item_ipv4 ipv4_mask = {0};
    struct rte_flow *flow = NULL;

    /* Parameter for queue action. */
    struct rte_flow_action_queue queue = { .index = 0 };
    struct rte_flow_action_mark mark = { .id = MARK_TWO };

    /* Pattern specification. */
    ipv4.hdr.dst_addr = inet_addr("0.0.0.2");
    ipv4_mask.hdr.dst_addr = inet_addr("0.0.0.255");

    /* Match IP header. */
    pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
    pattern[0].spec = &ipv4;
    pattern[0].last = NULL;
    pattern[0].mask = &ipv4_mask;

    /* End the pattern array. */
    pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

    /* Create the queue action. */
    actions[0].type = RTE_FLOW_ACTION_TYPE_MARK;
    actions[0].conf  = &mark;

    actions[1].type = RTE_FLOW_ACTION_TYPE_QUEUE;
    actions[1].conf  = &queue;

    actions[2].type = RTE_FLOW_ACTION_TYPE_END;

    /* Validate and create the flow rule. */
    flow = rte_flow_create(portid, &attr, pattern, actions, error);
}

/* --------------------------------------- */
/* Default rule to MARK packets using ID 3*/
/* ------------------------------------- */
void create_rule3(uint16_t portid, struct rte_flow_error *error) {
    struct rte_flow_attr attr = { .ingress = 1, .priority = 5 };
    struct rte_flow_item pattern[MAX_PATTERN_NUM];
    struct rte_flow_action actions[MAX_ACTION_NUM];
    struct rte_flow_item_ipv4 ipv4 = {0};
    struct rte_flow_item_ipv4 ipv4_mask = {0};
    struct rte_flow *flow = NULL;

    /* Parameter for queue action. */
    struct rte_flow_action_queue queue = { .index = 0 };
    struct rte_flow_action_mark mark = { .id = MARK_OTHER };

    /* Pattern specification. */
    ipv4.hdr.dst_addr = inet_addr("0.0.0.0");
    ipv4_mask.hdr.dst_addr = inet_addr("0.0.0.0");

    /* Match IP header. */
    pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
    pattern[0].spec = &ipv4;
    pattern[0].last = NULL;
    pattern[0].mask = &ipv4_mask;

    /* End the pattern array. */
    pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

    /* Create the queue action. */
    actions[0].type = RTE_FLOW_ACTION_TYPE_MARK;
    actions[0].conf  = &mark;

    actions[1].type = RTE_FLOW_ACTION_TYPE_QUEUE;
    actions[1].conf  = &queue;

    actions[2].type = RTE_FLOW_ACTION_TYPE_END;

    /* Validate and create the flow rule. */
    flow = rte_flow_create(portid, &attr, pattern, actions, error);
}

void create_flow(uint16_t portid, struct rte_flow_error *error) {
    create_rule1(portid, error);
    create_rule2(portid, error);
    create_rule3(portid, error);
}
