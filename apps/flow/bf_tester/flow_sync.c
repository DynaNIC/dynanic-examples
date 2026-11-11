/* flow_sync.c: Rule insertion with synchronous rte_flow_create() (as testpmd).
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2026
*
* Every rte_flow_create() call waits until the rule is in the HW. No templates
* are needed: with HW steering the PMD manages the tables itself (and grows
* them as rules are added), with SW steering it works as in testpmd.
* The mlx5 flow API is thread-safe, so the lcores still insert in parallel.
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "flow_internal.h"

#include <rte_common.h>

/* Create one rule in the given group; priority 0 is matched first. */
static int rule_create(struct bf_flow_ctx *ctx, const char *name, uint32_t group,
                       uint32_t priority, const struct rte_flow_item *items,
                       const struct rte_flow_action *acts, struct rte_flow **slot)
{
    const struct rte_flow_attr attr = { .group = group, .priority = priority, .ingress = 1 };
    struct rte_flow_error err;

    *slot = rte_flow_create(ctx->port_id, &attr, items, acts, &err);
    if (*slot == NULL) {
        bf_flow_err_log(name, &err);
        return -1;
    }
    return 0;
}

#if BF_TMP_EGRESS_VLAN
/*
 * TEMPORARY: push the VLAN on egress, i.e. on everything leaving the port
 * (the hairpin traffic). The egress root table cannot push VLAN, so egress
 * group 0 jumps to egress group 1 where the VLAN is pushed.
 */
static int tmp_egress_vlan_insert(struct bf_flow_ctx *ctx)
{
    const struct rte_flow_attr root = { .group = 0, .egress = 1 };
    const struct rte_flow_attr grp1 = { .group = 1, .egress = 1 };
    const struct rte_flow_action_jump jump_conf = { .group = 1 };
    const struct rte_flow_action jump[] = {
        { .type = RTE_FLOW_ACTION_TYPE_JUMP, .conf = &jump_conf },
        { .type = RTE_FLOW_ACTION_TYPE_END },
    };
    const struct rte_flow_action push[] = {
        { .type = RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN, .conf = &ctx->push_vlan },
        { .type = RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID, .conf = &ctx->set_vlan_vid },
        { .type = RTE_FLOW_ACTION_TYPE_END },
    };
    struct rte_flow_error err;

    ctx->tmp_egress_rules[0] = rte_flow_create(ctx->port_id, &root, bf_any_items, jump, &err);
    if (ctx->tmp_egress_rules[0] == NULL) {
        bf_flow_err_log("TEMPORARY egress jump rule", &err);
        return -1;
    }
    ctx->tmp_egress_rules[1] = rte_flow_create(ctx->port_id, &grp1, bf_any_items, push, &err);
    if (ctx->tmp_egress_rules[1] == NULL) {
        bf_flow_err_log("TEMPORARY egress push VLAN rule", &err);
        return -1;
    }
    return 0;
}
#endif

/* Group 0 rules, miss rules (lower priority) and, in single mode, the eth/ipv4/tcp rule. */
int bf_sync_fixed_rules_insert(struct bf_flow_ctx *ctx)
{
    struct rte_flow_action acts[BF_MAX_ACTIONS];
    struct rte_flow_action jump[2];
    struct rte_flow_item_ipv4 split_spec;
    struct rte_flow_item split_items[3];

    /* Group 0: split by src IP bits (N > 1), the rest goes to group 1. */
    for (uint16_t t = 0; ctx->nb_tables > 1 && t < ctx->nb_tables; t++) {
        bf_split_items(ctx, t, &split_spec, split_items);
        bf_jump_actions(ctx, t, jump);
        if (rule_create(ctx, "split rule", 0, 0, split_items, jump, &ctx->split_rules[t]) < 0)
            return -1;
    }
    bf_jump_actions(ctx, 0, jump);
    if (rule_create(ctx, "root jump rule", 0, 1, bf_any_items, jump, &ctx->root_rule) < 0)
        return -1;

    for (uint16_t t = 0; t < ctx->nb_tables; t++) {
        if (rule_create(ctx, "miss rule", BF_MAIN_GROUP + t, 1, bf_any_items,
                        bf_miss_actions(ctx), &ctx->miss_rules[t]) < 0)
            return -1;
    }

    bf_main_actions(ctx, acts);
    if (!ctx->set->mode->per_flow &&
        rule_create(ctx, "eth/ipv4/tcp rule", BF_MAIN_GROUP, 0, bf_tcp_any_items,
                    acts, &ctx->single_rule) < 0)
        return -1;

#if BF_TMP_EGRESS_VLAN
    if (ctx->set->mode->push_vlan && tmp_egress_vlan_insert(ctx) < 0)
        return -1;
#endif
    return 0;
}

/* Insert the worker's share of per-flow rules, one call per rule. */
int bf_sync_worker(struct bf_worker *w)
{
    struct bf_flow_ctx *ctx = w->ctx;
    const struct application_settings *set = ctx->set;
    struct rte_flow_action acts[BF_MAX_ACTIONS];

    /* Without templates the masks must be given in every rule. */
    struct rte_flow_item_ipv4 ip = { 0 };
    struct rte_flow_item_tcp tcp = { 0 };
    const struct rte_flow_item items[] = {
        { .type = RTE_FLOW_ITEM_TYPE_ETH },
        { .type = RTE_FLOW_ITEM_TYPE_IPV4, .spec = &ip,  .mask = &bf_ipv4_5t_mask },
        { .type = RTE_FLOW_ITEM_TYPE_TCP,  .spec = &tcp, .mask = &bf_tcp_5t_mask },
        { .type = RTE_FLOW_ITEM_TYPE_END },
    };

    bf_main_actions(ctx, acts);

    for (uint64_t i = w->first; i < w->last; i++) {
        if ((i & 0xfff) == 0 && *w->stop) {
            w->aborted = true;
            break;
        }
        bf_fill_5tuple(set, i, &ip, &tcp);
        /* TODO (modes edit_mac..all): fill per-rule action values (MAC, MPLS label, meter). */

        if (rule_create(ctx, "rte_flow_create()", BF_MAIN_GROUP + bf_table_of(ctx, i), 0,
                        items, acts, &ctx->flows[i]) < 0) {
            /* Most likely a resource limit: stop this lcore, the others go on. */
            w->failed++;
            break;
        }
        w->ok++;
        __atomic_store_n(&w->done, i - w->first + 1, __ATOMIC_RELAXED);
    }
    return 0;
}

static void rule_destroy(struct bf_flow_ctx *ctx, struct rte_flow **rule)
{
    struct rte_flow_error err;

    if (*rule != NULL)
        rte_flow_destroy(ctx->port_id, *rule, &err);
    *rule = NULL;
}

/* Destroy the fixed rules; per-flow rules are freed by closing the port. */
void bf_sync_teardown(struct bf_flow_ctx *ctx)
{
    rule_destroy(ctx, &ctx->single_rule);
    rule_destroy(ctx, &ctx->root_rule);
    rule_destroy(ctx, &ctx->tmp_egress_rules[1]);   /* TEMPORARY: BF_TMP_EGRESS_VLAN */
    rule_destroy(ctx, &ctx->tmp_egress_rules[0]);
    for (uint16_t t = 0; t < BF_MAX_TABLES; t++) {
        rule_destroy(ctx, &ctx->split_rules[t]);
        rule_destroy(ctx, &ctx->miss_rules[t]);
    }
}
