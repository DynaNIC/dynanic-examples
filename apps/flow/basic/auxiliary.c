/* auxiliary.c: Implementation of helper functions for basic DPDK RTE Flow rule generation.
*               Implements RTE Flow rules to match IPv4 destination addresses based on parity:
*                   - routing packets with even destination IP addresses to a target queue
*                   - and dropping packets with odd destination IP addresses.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, August 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "auxiliary.h"

/* ---------------------------------------------------------- */
/* Send packets with even DST IPv4 address to specified queue */
/* ---------------------------------------------------------- */
void create_even_rule(uint16_t portid, uint32_t q_id, struct rte_flow_error *error) {
    /* Pattern specification. */
    struct rte_flow_item_ipv4 ipv4 = {0};
    struct rte_flow_item_ipv4 ipv4_mask = {0};
    ipv4.hdr.dst_addr = inet_addr("0.0.0.0");
    ipv4_mask.hdr.dst_addr = inet_addr("0.0.0.1");

    struct rte_flow_item pattern[] = {
        {
            .type = RTE_FLOW_ITEM_TYPE_IPV4,
            .spec = &ipv4,
            .last = NULL,
            .mask = &ipv4_mask,
        },
        {
            .type = RTE_FLOW_ITEM_TYPE_END,
        },
    };

    /* Create the queue action. */
    struct rte_flow_action_queue queue = { .index = q_id };
    struct rte_flow_action actions[] = {
        {
            .type = RTE_FLOW_ACTION_TYPE_QUEUE,
            .conf  = &queue,
        },
        {
            .type = RTE_FLOW_ACTION_TYPE_END,
        }
    };

    /* Validate and create the flow rule. */
    struct rte_flow_attr attr = { .ingress = 1, .priority = 0 };
    struct rte_flow *flow = NULL;
    flow = rte_flow_create(portid, &attr, pattern, actions, error);
}

/* -------------------------------------- */
/* Drop packets with odd dst IPv4 address */
/* -------------------------------------- */
void create_odd_rule(uint16_t portid, uint32_t q_id, struct rte_flow_error *error) {
    /* Pattern specification. */
    struct rte_flow_item_ipv4 ipv4 = {0};
    struct rte_flow_item_ipv4 ipv4_mask = {0};
    ipv4.hdr.dst_addr = inet_addr("0.0.0.1");
    ipv4_mask.hdr.dst_addr = inet_addr("0.0.0.1");

    struct rte_flow_item pattern[] = {
        {
            .type = RTE_FLOW_ITEM_TYPE_IPV4,
            .spec = &ipv4,
            .last = NULL,
            .mask = &ipv4_mask,
        },
        {
            .type = RTE_FLOW_ITEM_TYPE_END,
        },
    };

    /* Create the drop action. */
    struct rte_flow_action_queue queue = { .index = q_id };
    struct rte_flow_action actions[] = {
        {
            .type = RTE_FLOW_ACTION_TYPE_DROP,
            .conf  = NULL,
        },
        {
            .type = RTE_FLOW_ACTION_TYPE_END,
        }
    };

    /* Validate and create the flow rule. */
    struct rte_flow_attr attr = { .ingress = 1, .priority = 0 };
    struct rte_flow *flow = NULL;
    flow = rte_flow_create(portid, &attr, pattern, actions, error);
}
