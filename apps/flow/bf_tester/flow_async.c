/* flow_async.c: Rule insertion with the async template API (HW steering only).
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2026
*
* How it works:
*  1. Templates say which header fields a table matches and which actions its
*     rules use; a template table is created from them with a fixed capacity.
*  2. Every inserting lcore owns one flow queue. It enqueues rules without
*     waiting (rte_flow_async_create with "postpone"), sends them to the HW in
*     bursts (rte_flow_push) and collects the results (rte_flow_pull).
* No lcore ever waits for a single rule, which gives millions of rules/s.
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "flow_internal.h"

#include <stdio.h>
#include <string.h>

#include <rte_common.h>

/* Results collected by one rte_flow_pull() call. */
#define BF_PULL_BURST 128

/* ------------------------------------------------------------------------ */
/* Flow queue wrapper                                                       */
/* ------------------------------------------------------------------------ */

/* State of one flow queue, owned by a single lcore (no locking needed). */
struct bf_queue {
    uint16_t port_id;
    uint32_t queue_id;
    uint32_t size;      /* max operations in flight (--flow-queue-size) */
    uint32_t burst;     /* operations per rte_flow_push (--push-burst) */
    uint32_t inflight;  /* enqueued, result not pulled yet */
    uint32_t pending;   /* enqueued, not pushed yet */
    uint64_t ok;
    uint64_t failed;
    struct rte_flow_op_result res[BF_PULL_BURST];
};

static void q_init(struct bf_queue *q, uint16_t port_id, uint32_t queue_id,
                   uint32_t size, uint32_t burst)
{
    memset(q, 0, sizeof(*q));
    q->port_id = port_id;
    q->queue_id = queue_id;
    q->size = size;
    q->burst = burst;
}

/* Send the pending operations to the HW. */
static int q_push(struct bf_queue *q)
{
    struct rte_flow_error err;

    if (q->pending == 0)
        return 0;
    if (rte_flow_push(q->port_id, q->queue_id, &err) != 0) {
        bf_flow_err_log("rte_flow_push()", &err);
        return -1;
    }
    q->pending = 0;
    return 0;
}

/* Collect finished operations; a rejected rule has its handle slot cleared. */
static int q_pull(struct bf_queue *q)
{
    struct rte_flow_error err;
    int n = rte_flow_pull(q->port_id, q->queue_id, q->res, BF_PULL_BURST, &err);

    if (n < 0) {
        bf_flow_err_log("rte_flow_pull()", &err);
        return -1;
    }
    for (int i = 0; i < n; i++) {
        if (q->res[i].status == RTE_FLOW_OP_SUCCESS) {
            q->ok++;
        } else {
            q->failed++;
            /* user_data is the handle slot; the PMD already freed the rule. */
            if (q->res[i].user_data != NULL)
                *(struct rte_flow **)q->res[i].user_data = NULL;
        }
    }
    q->inflight -= n;
    return n;
}

/* Wait until one more operation fits into the queue. */
static int q_reserve(struct bf_queue *q)
{
    while (q->inflight >= q->size) {
        if (q_push(q) < 0 || q_pull(q) < 0)
            return -1;
    }
    return 0;
}

/* Count an enqueued operation; every burst push it and collect results. */
static int q_enqueued(struct bf_queue *q)
{
    q->inflight++;
    if (++q->pending >= q->burst) {
        if (q_push(q) < 0 || q_pull(q) < 0)
            return -1;
    }
    return 0;
}

/* Push everything and wait for all results. */
static int q_drain(struct bf_queue *q)
{
    if (q_push(q) < 0)
        return -1;
    while (q->inflight > 0) {
        if (q_pull(q) < 0)
            return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Templates and tables                                                     */
/* ------------------------------------------------------------------------ */

static struct rte_flow_template_table *
table_create(struct bf_flow_ctx *ctx, const char *name, uint32_t group, uint32_t priority,
             uint32_t nb_flows, struct rte_flow_pattern_template *pt,
             struct rte_flow_actions_template *at)
{
    struct rte_flow_error err;
    const struct rte_flow_template_table_attr attr = {
        .flow_attr = { .group = group, .priority = priority, .ingress = 1 },
        .nb_flows = nb_flows,
    };
    struct rte_flow_template_table *t =
        rte_flow_template_table_create(ctx->port_id, &attr, &pt, 1, &at, 1, &err);

    if (t == NULL) {
        char what[96];
        snprintf(what, sizeof(what), "template table '%s' (group %u, %u rules)", name, group, nb_flows);
        bf_flow_err_log(what, &err);
    }
    return t;
}

static struct rte_flow_pattern_template *
pattern_template(struct bf_flow_ctx *ctx, const char *name, const struct rte_flow_item *items)
{
    const struct rte_flow_pattern_template_attr attr = { .relaxed_matching = 0, .ingress = 1 };
    struct rte_flow_error err;
    struct rte_flow_pattern_template *pt =
        rte_flow_pattern_template_create(ctx->port_id, &attr, items, &err);

    if (pt == NULL)
        bf_flow_err_log(name, &err);
    return pt;
}

static struct rte_flow_actions_template *
actions_template(struct bf_flow_ctx *ctx, const char *name,
                 const struct rte_flow_action *acts, const struct rte_flow_action *masks)
{
    const struct rte_flow_actions_template_attr attr = { .ingress = 1 };
    struct rte_flow_error err;
    struct rte_flow_actions_template *at =
        rte_flow_actions_template_create(ctx->port_id, &attr, acts, masks, &err);

    if (at == NULL)
        bf_flow_err_log(name, &err);
    return at;
}

/*
 * Tables:
 *   group 0:    "split" (N > 1: N jump rules by src IP bits, priority 0)
 *               "root"  (1 jump rule to group 1 for the rest, priority 1)
 *   group 1+t:  "main"  (the matching rules of table t)
 *               "miss"  (1 drop rule, lower priority, catches the rest)
 */
int bf_async_tables_create(struct bf_flow_ctx *ctx)
{
    const bool single = !ctx->set->mode->per_flow;
    struct rte_flow_action acts[BF_MAX_ACTIONS];
    struct rte_flow_item_ipv4 split_spec;
    struct rte_flow_item split_items[3];

    /* Pattern templates: the item masks select the matched fields. */
    bf_split_items(ctx, 0, &split_spec, split_items);
    ctx->pt_any = pattern_template(ctx, "pattern template 'any'", bf_any_items);
    ctx->pt_main = pattern_template(ctx, single ? "pattern template 'eth/ipv4/tcp'" : "pattern template '5-tuple'",
                                    single ? bf_tcp_any_items : bf_tcp_5t_items);
    if (ctx->pt_any == NULL || ctx->pt_main == NULL)
        return -1;
    if (ctx->nb_tables > 1) {
        ctx->pt_split = pattern_template(ctx, "pattern template 'split'", split_items);
        if (ctx->pt_split == NULL)
            return -1;
    }

    /* Actions templates: an action passed as its own mask is the same for every
     * rule; the jump has no mask conf, so each rule sets its target group. */
    bf_jump_actions(ctx, 0, acts);
    ctx->at_jump = actions_template(ctx, "actions template 'jump'", acts, bf_jump_masks);
    bf_main_actions(ctx, acts);
    ctx->at_main = actions_template(ctx, "actions template 'main'", acts, acts);
    ctx->at_miss = actions_template(ctx, "actions template 'miss'",
                                    bf_miss_actions(ctx), bf_miss_actions(ctx));
    if (ctx->at_jump == NULL || ctx->at_main == NULL || ctx->at_miss == NULL)
        return -1;

    /* Group 0. */
    if (ctx->nb_tables > 1) {
        ctx->tbl_split = table_create(ctx, "split", 0, 0, ctx->nb_tables, ctx->pt_split, ctx->at_jump);
        if (ctx->tbl_split == NULL)
            return -1;
    }
    ctx->tbl_root = table_create(ctx, "root", 0, 1, 1, ctx->pt_any, ctx->at_jump);
    if (ctx->tbl_root == NULL)
        return -1;

    /* Main and miss table of every group. */
    for (uint16_t t = 0; t < ctx->nb_tables; t++) {
        /* Smallest power of two the rules fill to less than BF_TABLE_MAX_FILL_PCT,
         * the size the sync (BWC) matcher grows to. */
        uint64_t size = single ? 1 :
            rte_align64pow2(ctx->table_flows[t] * 100 / BF_TABLE_MAX_FILL_PCT + 1);
        if (size > UINT32_MAX) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1,
                    "ERROR: template table of %" PRIu64 " rules is too large.\n", size);
            return -1;
        }
        ctx->table_size[t] = size;
        ctx->tbl_main[t] = table_create(ctx, "main", BF_MAIN_GROUP + t, 0, (uint32_t)size,
                                        ctx->pt_main, ctx->at_main);
        ctx->tbl_miss[t] = table_create(ctx, "miss", BF_MAIN_GROUP + t, 1, 1,
                                        ctx->pt_any, ctx->at_miss);
        if (ctx->tbl_main[t] == NULL || ctx->tbl_miss[t] == NULL)
            return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Rules                                                                    */
/* ------------------------------------------------------------------------ */

/* Enqueue one rule; *slot gets the handle (cleared later if the HW rejects it). */
static int rule_enqueue(struct bf_flow_ctx *ctx, struct bf_queue *q, const char *name,
                        struct rte_flow_template_table *tbl, const struct rte_flow_item *items,
                        const struct rte_flow_action *acts, struct rte_flow **slot)
{
    const struct rte_flow_op_attr op = { .postpone = 1 };
    struct rte_flow_error err;

    if (q_reserve(q) < 0)
        return -1;
    /* The handle must be stored before a pull can report the result. */
    *slot = rte_flow_async_create(ctx->port_id, q->queue_id, &op, tbl, items, BF_TEMPLATE_IDX,
                                  acts, BF_TEMPLATE_IDX, slot, &err);
    if (*slot == NULL) {
        bf_flow_err_log(name, &err);
        return -1;
    }
    return q_enqueued(q);
}

/* Group 0 rules, miss rules and (single mode) the eth/ipv4/tcp rule, on queue 0. */
int bf_async_fixed_rules_insert(struct bf_flow_ctx *ctx)
{
    struct rte_flow_action acts[BF_MAX_ACTIONS];
    struct rte_flow_action jump[2];
    struct rte_flow_item_ipv4 split_spec;
    struct rte_flow_item split_items[3];
    struct bf_queue q;

    q_init(&q, ctx->port_id, 0, ctx->set->flow_queue_size, ctx->set->push_burst);

    /* Group 0: split by src IP bits, the rest goes to group 1. */
    for (uint16_t t = 0; ctx->nb_tables > 1 && t < ctx->nb_tables; t++) {
        bf_split_items(ctx, t, &split_spec, split_items);
        bf_jump_actions(ctx, t, jump);
        if (rule_enqueue(ctx, &q, "split rule", ctx->tbl_split, split_items, jump,
                         &ctx->split_rules[t]) < 0)
            return -1;
    }
    bf_jump_actions(ctx, 0, jump);
    if (rule_enqueue(ctx, &q, "root jump rule", ctx->tbl_root, bf_any_items, jump,
                     &ctx->root_rule) < 0)
        return -1;

    for (uint16_t t = 0; t < ctx->nb_tables; t++) {
        if (rule_enqueue(ctx, &q, "miss rule", ctx->tbl_miss[t], bf_any_items,
                         bf_miss_actions(ctx), &ctx->miss_rules[t]) < 0)
            return -1;
    }

    bf_main_actions(ctx, acts);
    if (!ctx->set->mode->per_flow &&
        rule_enqueue(ctx, &q, "eth/ipv4/tcp rule", ctx->tbl_main[0], bf_tcp_any_items,
                     acts, &ctx->single_rule) < 0)
        return -1;

    if (q_drain(&q) < 0)
        return -1;
    if (q.failed != 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1,
                "ERROR: %" PRIu64 " fixed rule(s) rejected by HW.\n", q.failed);
        return -1;
    }
    return 0;
}

/* Insert the worker's share of per-flow rules through its own flow queue. */
int bf_async_worker(struct bf_worker *w)
{
    struct bf_flow_ctx *ctx = w->ctx;
    const struct application_settings *set = ctx->set;
    struct rte_flow_action acts[BF_MAX_ACTIONS];
    struct bf_queue q;

    /* Only the spec is given: the masks come from the pattern template. */
    struct rte_flow_item_ipv4 ip = { 0 };
    struct rte_flow_item_tcp tcp = { 0 };
    const struct rte_flow_item items[] = {
        { .type = RTE_FLOW_ITEM_TYPE_ETH },
        { .type = RTE_FLOW_ITEM_TYPE_IPV4, .spec = &ip },
        { .type = RTE_FLOW_ITEM_TYPE_TCP,  .spec = &tcp },
        { .type = RTE_FLOW_ITEM_TYPE_END },
    };

    bf_main_actions(ctx, acts);
    q_init(&q, ctx->port_id, w->queue_id, set->flow_queue_size, set->push_burst);

    for (uint64_t i = w->first; i < w->last; i++) {
        if ((i & 0xfff) == 0 && *w->stop) {
            w->aborted = true;
            break;
        }
        bf_fill_5tuple(set, i, &ip, &tcp);
        /* TODO (modes edit..all): fill per-rule action values (MAC, MPLS label, meter). */

        if (rule_enqueue(ctx, &q, "rte_flow_async_create()", ctx->tbl_main[bf_table_of(ctx, i)],
                         items, acts, &ctx->flows[i]) < 0) {
            w->error = -1;
            break;
        }
        __atomic_store_n(&w->done, i - w->first + 1, __ATOMIC_RELAXED);
    }

    if (q_drain(&q) < 0)
        w->error = -1;
    w->ok = q.ok;
    w->failed = q.failed;
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Teardown                                                                 */
/* ------------------------------------------------------------------------ */

static bool flows_live(const struct bf_flow_ctx *ctx)
{
    if (ctx->flows == NULL)
        return false;
    for (uint64_t i = 0; i < ctx->nb_flows; i++)
        if (ctx->flows[i] != NULL)
            return true;
    return false;
}

static void rule_destroy(struct bf_flow_ctx *ctx, struct bf_queue *q, struct rte_flow **rule)
{
    const struct rte_flow_op_attr op = { .postpone = 1 };
    struct rte_flow_error err;

    if (*rule != NULL && q_reserve(q) == 0 &&
        rte_flow_async_destroy(ctx->port_id, q->queue_id, &op, *rule, NULL, &err) == 0)
        q_enqueued(q);
    *rule = NULL;
}

static void table_destroy(struct bf_flow_ctx *ctx, struct rte_flow_template_table **tbl)
{
    struct rte_flow_error err;

    if (*tbl != NULL && rte_flow_template_table_destroy(ctx->port_id, *tbl, &err) != 0)
        bf_flow_err_log("rte_flow_template_table_destroy()", &err);
    *tbl = NULL;
}

/* Destroy the fixed rules and, if no per-flow rules remain, tables and templates. */
void bf_async_teardown(struct bf_flow_ctx *ctx)
{
    struct rte_flow_error err;
    struct bf_queue q;

    q_init(&q, ctx->port_id, 0, ctx->set->flow_queue_size, ctx->set->push_burst);
    rule_destroy(ctx, &q, &ctx->single_rule);
    rule_destroy(ctx, &q, &ctx->root_rule);
    for (uint16_t t = 0; t < BF_MAX_TABLES; t++) {
        rule_destroy(ctx, &q, &ctx->split_rules[t]);
        rule_destroy(ctx, &q, &ctx->miss_rules[t]);
    }
    q_drain(&q);

    /* Per-flow rules are not destroyed one by one; a table still holding
     * them cannot be destroyed, closing the port frees everything. */
    if (flows_live(ctx))
        return;

    for (uint16_t t = 0; t < BF_MAX_TABLES; t++) {
        table_destroy(ctx, &ctx->tbl_main[t]);
        table_destroy(ctx, &ctx->tbl_miss[t]);
    }
    table_destroy(ctx, &ctx->tbl_split);
    table_destroy(ctx, &ctx->tbl_root);

    struct rte_flow_actions_template **ats[] = { &ctx->at_main, &ctx->at_miss, &ctx->at_jump };
    for (unsigned i = 0; i < RTE_DIM(ats); i++) {
        if (*ats[i] != NULL)
            rte_flow_actions_template_destroy(ctx->port_id, *ats[i], &err);
        *ats[i] = NULL;
    }
    struct rte_flow_pattern_template **pts[] = { &ctx->pt_main, &ctx->pt_split, &ctx->pt_any };
    for (unsigned i = 0; i < RTE_DIM(pts); i++) {
        if (*pts[i] != NULL)
            rte_flow_pattern_template_destroy(ctx->port_id, *pts[i], &err);
        *pts[i] = NULL;
    }
}
