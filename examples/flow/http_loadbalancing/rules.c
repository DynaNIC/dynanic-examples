/* rules.c: RTE Flow rules for simple http loadbalance example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, April 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "rules.h"

void print_error(struct rte_flow_error *error) {
        fprintf(stderr, "Flow creation failed on port 0 (rte_errno: %d -> %s):\n", rte_errno, rte_strerror(rte_errno));
        fprintf(stderr, "  Error type: %d\n", error->type);
        fprintf(stderr, "  Cause: %p\n", error->cause);

        if (error->message != NULL) {
            fprintf(stderr, "  Message: %s\n", error->message);
        } else {
            fprintf(stderr, "  Message: (no specific message provided)\n");
        }
}

void default_rule(uint16_t portid,
    struct rte_flow_error *error,
    enum rte_flow_action_type action_type,
    void *action_conf,
    uint32_t group
) {
    rule_factory(portid, error, NULL, NULL, RTE_FLOW_ITEM_TYPE_VOID, action_type, action_conf, group, DEFAULT_RULE_PRIORITY);
}

void rule_factory(
    uint16_t portid,
    struct rte_flow_error *error,
    void *item,
    void *mask,
    enum rte_flow_item_type pattern_type,
    enum rte_flow_action_type action_type,
    void *action_conf,
    uint32_t group,
    uint32_t priority
) {
    struct rte_flow_attr attr = {
        .ingress = 1,
        .group = group,
        .priority = priority
    };

    struct rte_flow_item pattern[MAX_PATTERN_NUM];
    struct rte_flow_action actions[MAX_ACTION_NUM];

    pattern[0].type = pattern_type;
    pattern[0].spec = item;
    pattern[0].last = NULL;
    pattern[0].mask = mask;
    pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

    actions[0].type = action_type;
    actions[0].conf  = action_conf;
    actions[1].type = RTE_FLOW_ACTION_TYPE_END;

    struct rte_flow *flow = rte_flow_create(portid, &attr, pattern, actions, error);
    if (!flow){
        print_error(error);
    }
}

void udp_tcp_jump_rule_factory(uint16_t portid, struct rte_flow_error *error, uint32_t dst_port) {
    struct rte_flow_item_udp udp_mask = {.hdr = {.dst_port= 0xffff}};
    struct rte_flow_item_tcp tcp_mask = {.hdr = {.dst_port= 0xffff}};
    struct rte_flow_action_jump jump_to = {
        .group = 3
    };
    struct rte_flow_item_udp udp = {
        .hdr = {
            .dst_port = rte_cpu_to_be_16(dst_port)
        }
    };

    struct rte_flow_item_tcp tcp = {
        .hdr = {
            .dst_port = rte_cpu_to_be_16(dst_port)
        }
    };

    rule_factory(portid, error, &udp, &udp_mask, RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ACTION_TYPE_JUMP, &jump_to, 0, 0);
    rule_factory(portid, error, &tcp, &tcp_mask, RTE_FLOW_ITEM_TYPE_TCP, RTE_FLOW_ACTION_TYPE_JUMP, &jump_to, 0, 0);
}

void create_flow(uint16_t portid, struct rte_flow_error *error) {
    uint32_t http = 80;
    uint32_t https = 443;
    uint32_t group0 = 0;
    uint32_t group3 = 3;
    uint32_t priority = 0;
    struct rte_flow_item_ipv4 ipv4 = { .hdr = {
        .dst_addr = inet_addr("0.0.0.0")
    }};
    struct rte_flow_item_ipv4 ipv4_mask = {0};
    struct rte_flow_action_queue queue = {
        .index = 0
    };

    udp_tcp_jump_rule_factory(portid, error, http);
    udp_tcp_jump_rule_factory(portid, error, https);

    ipv4_mask.hdr.dst_addr = inet_addr("0.0.0.192");
    queue.index = 0;
    rule_factory(portid, error, &ipv4, &ipv4_mask, RTE_FLOW_ITEM_TYPE_IPV4, RTE_FLOW_ACTION_TYPE_QUEUE, &queue, group3, priority);

    ipv4.hdr.dst_addr = inet_addr("0.0.0.64");
    queue.index = 1;
    rule_factory(portid, error, &ipv4, &ipv4_mask, RTE_FLOW_ITEM_TYPE_IPV4, RTE_FLOW_ACTION_TYPE_QUEUE, &queue, group3, priority);

    ipv4.hdr.dst_addr = inet_addr("0.0.0.128");
    queue.index = 2;
    rule_factory(portid, error, &ipv4, &ipv4_mask, RTE_FLOW_ITEM_TYPE_IPV4, RTE_FLOW_ACTION_TYPE_QUEUE, &queue, group3, priority);

    ipv4.hdr.dst_addr = inet_addr("0.0.0.192");
    queue.index = 3;
    rule_factory(portid, error, &ipv4, &ipv4_mask, RTE_FLOW_ITEM_TYPE_IPV4, RTE_FLOW_ACTION_TYPE_QUEUE, &queue, group3, priority);

    queue.index = 4;
    default_rule(portid, error, RTE_FLOW_ACTION_TYPE_QUEUE, &queue, group0);
}
