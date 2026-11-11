/* flow_rules.c: Rule management for bf_tester, the part shared by the async and sync API.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2026
*
* Rule layout (N = --tables, 1 by default):
*   group 0:      N > 1: "src IP & (N-1) == t" -> jump to group 1+t,
*                 and a lower-priority rule sending the rest to group 1
*   group 1..N:   the matching rules (one eth/ipv4/tcp rule or one rule per
*                 flow) -> RSS over the hairpin queues, plus a lower-priority
*                 miss rule dropping everything else
* Several smaller tables keep the HW hash tables sparse with many rules.
*
* This file holds what both APIs share: generating the flows, configuring the
* flow engine, the rule building blocks and running the inserting lcores.
* API specifics are in flow_async.c (template API) and flow_sync.c.
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "flow_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_launch.h>
#include <rte_lcore.h>

void bf_flow_err_log(const char *what, const struct rte_flow_error *err)
{
    rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1,
            "ERROR: %s failed: %s (error type %d, rte_errno %d: %s)\n",
            what, err->message ? err->message : "(no message)",
            err->type, rte_errno, rte_strerror(rte_errno));
}

/* ------------------------------------------------------------------------ */
/* Flows (the 5-tuples Spirent sends)                                       */
/* ------------------------------------------------------------------------ */

static inline uint32_t field_at(const struct bf_field_gen *f, uint64_t i)
{
    return f->base + (uint32_t)(i % f->count) * f->step;
}

/* Flow idx is the idx-th packet of the Spirent stream: every modifier has
 * stepped idx times, wrapping at its count. */
void bf_flow_tuple(const struct application_settings *set, uint64_t idx,
                   uint32_t *src_ip, uint32_t *dst_ip,
                   uint16_t *src_port, uint16_t *dst_port)
{
    *src_port = (uint16_t)field_at(&set->src_port, idx);
    *dst_port = (uint16_t)field_at(&set->dst_port, idx);
    *src_ip = field_at(&set->src_ip, idx);
    *dst_ip = field_at(&set->dst_ip, idx);
}

static uint64_t gcd64(uint64_t a, uint64_t b)
{
    while (b != 0) {
        uint64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/* The stream repeats after lcm(counts) packets, that many flows are distinct. */
uint64_t bf_flow_distinct_tuples(const struct application_settings *set)
{
    const uint64_t counts[] = {
        set->src_port.count, set->dst_port.count,
        set->src_ip.count, set->dst_ip.count,
    };
    uint64_t r = 1;

    for (unsigned i = 0; i < RTE_DIM(counts); i++) {
        uint64_t c = counts[i] / gcd64(r, counts[i]);

        if (r > UINT64_MAX / c)
            return UINT64_MAX;
        r *= c;
    }
    return r;
}

void bf_fill_5tuple(const struct application_settings *set, uint64_t idx,
                    struct rte_flow_item_ipv4 *ip, struct rte_flow_item_tcp *tcp)
{
    uint32_t sip, dip;
    uint16_t sp, dp;

    bf_flow_tuple(set, idx, &sip, &dip, &sp, &dp);
    ip->hdr.src_addr = rte_cpu_to_be_32(sip);
    ip->hdr.dst_addr = rte_cpu_to_be_32(dip);
    tcp->hdr.src_port = rte_cpu_to_be_16(sp);
    tcp->hdr.dst_port = rte_cpu_to_be_16(dp);
}

/* ------------------------------------------------------------------------ */
/* Flow engine                                                              */
/* ------------------------------------------------------------------------ */

/* RSS over all hairpin queues, as "rss types ipv4-tcp end queues ..." in testpmd. */
static int rss_setup(struct bf_flow_ctx *ctx, uint16_t first_hp_queue)
{
    const struct application_settings *set = ctx->set;

    ctx->hp_queues = calloc(set->nb_hp_queues, sizeof(uint16_t));
    if (ctx->hp_queues == NULL)
        return -1;
    for (uint16_t i = 0; i < set->nb_hp_queues; i++)
        ctx->hp_queues[i] = first_hp_queue + i;

    ctx->rss = (struct rte_flow_action_rss){
        .func = RTE_ETH_HASH_FUNCTION_DEFAULT,
        .types = RTE_ETH_RSS_NONFRAG_IPV4_TCP,
        .queue_num = set->nb_hp_queues,
        .queue = ctx->hp_queues,
    };
    return 0;
}

uint16_t bf_table_of(const struct bf_flow_ctx *ctx, uint64_t idx)
{
    return (uint16_t)(field_at(&ctx->set->src_ip, idx) & (ctx->nb_tables - 1));
}

/* Split the flows over the main tables and prepare the group 0 rules. */
static void tables_setup(struct bf_flow_ctx *ctx)
{
    ctx->nb_tables = ctx->set->nb_tables;
    ctx->split_mask.hdr.src_addr = rte_cpu_to_be_32(ctx->nb_tables - 1);
    for (uint16_t t = 0; t < ctx->nb_tables; t++)
        ctx->jump_conf[t].group = BF_MAIN_GROUP + t;

    if (ctx->nb_tables == 1) {
        ctx->table_flows[0] = ctx->nb_flows;
        return;
    }
    for (uint64_t i = 0; i < ctx->nb_flows; i++)
        ctx->table_flows[bf_table_of(ctx, i)]++;
}

/*
 * edit_mac / insert modes: set the source MAC to the port's own MAC, as a router
 * would (the destination MAC stays, so Spirent still gets the packets), and push
 * VLAN BF_INSERT_VLAN_ID.
 */
static void edit_setup(struct bf_flow_ctx *ctx)
{
    rte_eth_macaddr_get(ctx->port_id, &ctx->port_mac);
    ctx->push_vlan.ethertype = RTE_BE16(RTE_ETHER_TYPE_VLAN);
    ctx->set_vlan_vid.vlan_vid = rte_cpu_to_be_16(BF_INSERT_VLAN_ID);

    ctx->set_src_mac = (struct rte_flow_action_modify_field){
        .operation = RTE_FLOW_MODIFY_SET,
        .dst = { .field = RTE_FLOW_FIELD_MAC_SRC },
        .src = { .field = RTE_FLOW_FIELD_VALUE },
        .width = 48,
    };
    /* Immediate value in the same byte order as in rte_flow_item_eth. */
    memcpy(ctx->set_src_mac.src.value, ctx->port_mac.addr_bytes, RTE_ETHER_ADDR_LEN);

    /* Async template mask: all fixed; HWS wants levels, offsets and width fully masked
     * (the PMD builds the same mask itself for sync rules). */
    memset(&ctx->set_src_mac_mask, 0xff, sizeof(ctx->set_src_mac_mask));
    ctx->set_src_mac_mask.operation = ctx->set_src_mac.operation;
    ctx->set_src_mac_mask.dst.field = ctx->set_src_mac.dst.field;
    ctx->set_src_mac_mask.src.field = ctx->set_src_mac.src.field;
}

int bf_flow_engine_configure(struct bf_flow_ctx *ctx, uint16_t port_id,
                             const struct application_settings *set,
                             uint16_t first_hp_queue, uint16_t nb_flow_queues)
{
    struct rte_flow_port_info pinfo;
    struct rte_flow_queue_info qinfo;
    struct rte_flow_error err;

    memset(ctx, 0, sizeof(*ctx));
    ctx->port_id = port_id;
    ctx->set = set;
    ctx->nb_flows = set->mode->per_flow ? set->nb_flows : 0;
    ctx->nb_flow_queues = nb_flow_queues;
    tables_setup(ctx);
    if (set->mode->set_mac || set->mode->push_vlan)
        edit_setup(ctx);

    /* rte_flow_info_get() works only with HW steering (dv_flow_en=2). */
    memset(&pinfo, 0, sizeof(pinfo));
    memset(&qinfo, 0, sizeof(qinfo));
    if (rte_flow_info_get(port_id, &pinfo, &qinfo, &err) != 0) {
        if (set->sync)
            return rss_setup(ctx, first_hp_queue);  /* SW steering, nothing to configure */
        bf_flow_err_log("rte_flow_info_get()", &err);
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1,
                "HINT: the async flow API needs HW steering, e.g. -a <pci>,dv_flow_en=2\n");
        return -1;
    }
    /* Meter capacity is only reported after rte_flow_configure(), so it is not used. */
    ctx->max_counters = pinfo.max_nb_counters;

    /* Async: one flow queue per inserting lcore. Sync on HW steering still
     * needs the engine configured, but one queue is enough. */
    uint16_t nb_cfg_queues = set->sync ? 1 : nb_flow_queues;
    if (pinfo.max_nb_queues != 0 && nb_cfg_queues > pinfo.max_nb_queues) {
        printf("WARNING: limiting async flow queues %u -> %u (PMD maximum).\n",
               nb_cfg_queues, pinfo.max_nb_queues);
        nb_cfg_queues = (uint16_t)pinfo.max_nb_queues;
        ctx->nb_flow_queues = nb_cfg_queues;
    }
    if (qinfo.max_size != 0 && set->flow_queue_size > qinfo.max_size) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1,
                "ERROR: --flow-queue-size %u exceeds PMD maximum %u.\n",
                set->flow_queue_size, qinfo.max_size);
        return -1;
    }

    /* Counters are reserved up front: miss rules (--check-miss), one per main rule (count modes). */
    uint64_t nb_counters = (set->check_miss ? ctx->nb_tables : 0) +
                           (set->mode->count ? (set->mode->per_flow ? ctx->nb_flows : 1) : 0);
    if (nb_counters > UINT32_MAX ||
        (pinfo.max_nb_counters != 0 && nb_counters > pinfo.max_nb_counters)) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1,
                "ERROR: %" PRIu64 " counters needed, PMD maximum is %u.\n",
                nb_counters, pinfo.max_nb_counters);
        return -1;
    }

    struct rte_flow_port_attr pattr = { .nb_counters = (uint32_t)nb_counters };
    struct rte_flow_queue_attr qattr = { .size = set->flow_queue_size };
    const struct rte_flow_queue_attr **qattrs = calloc(nb_cfg_queues, sizeof(*qattrs));
    if (qattrs == NULL)
        return -1;
    for (uint16_t i = 0; i < nb_cfg_queues; i++)
        qattrs[i] = &qattr;

    int ret = rte_flow_configure(port_id, &pattr, nb_cfg_queues, qattrs, &err);
    free(qattrs);
    if (ret != 0) {
        bf_flow_err_log("rte_flow_configure()", &err);
        return -1;
    }
    ctx->hws = true;
    ctx->nb_counters = pattr.nb_counters;
    ctx->nb_cfg_queues = nb_cfg_queues;

    return rss_setup(ctx, first_hp_queue);
}

/* ------------------------------------------------------------------------ */
/* Rule building blocks                                                     */
/* ------------------------------------------------------------------------ */

/* Exact 5-tuple match; the protocol is implied by the TCP item. */
const struct rte_flow_item_ipv4 bf_ipv4_5t_mask = {
    .hdr.src_addr = RTE_BE32(0xffffffff),
    .hdr.dst_addr = RTE_BE32(0xffffffff),
};
const struct rte_flow_item_tcp bf_tcp_5t_mask = {
    .hdr.src_port = RTE_BE16(0xffff),
    .hdr.dst_port = RTE_BE16(0xffff),
};

/* Items without a mask match only on the presence of the protocol. */
const struct rte_flow_item bf_any_items[] = {
    { .type = RTE_FLOW_ITEM_TYPE_ETH },
    { .type = RTE_FLOW_ITEM_TYPE_END },
};
const struct rte_flow_item bf_tcp_any_items[] = {
    { .type = RTE_FLOW_ITEM_TYPE_ETH },
    { .type = RTE_FLOW_ITEM_TYPE_IPV4 },
    { .type = RTE_FLOW_ITEM_TYPE_TCP },
    { .type = RTE_FLOW_ITEM_TYPE_END },
};
const struct rte_flow_item bf_tcp_5t_items[] = {
    { .type = RTE_FLOW_ITEM_TYPE_ETH },
    { .type = RTE_FLOW_ITEM_TYPE_IPV4, .mask = &bf_ipv4_5t_mask },
    { .type = RTE_FLOW_ITEM_TYPE_TCP,  .mask = &bf_tcp_5t_mask },
    { .type = RTE_FLOW_ITEM_TYPE_END },
};

/* No mask conf: the jump target is set per rule. */
const struct rte_flow_action bf_jump_masks[] = {
    { .type = RTE_FLOW_ACTION_TYPE_JUMP },
    { .type = RTE_FLOW_ACTION_TYPE_END },
};

void bf_jump_actions(const struct bf_flow_ctx *ctx, uint16_t table, struct rte_flow_action acts[2])
{
    acts[0] = (struct rte_flow_action){ .type = RTE_FLOW_ACTION_TYPE_JUMP, .conf = &ctx->jump_conf[table] };
    acts[1] = (struct rte_flow_action){ .type = RTE_FLOW_ACTION_TYPE_END };
}

void bf_split_items(const struct bf_flow_ctx *ctx, uint16_t table,
                    struct rte_flow_item_ipv4 *spec, struct rte_flow_item items[3])
{
    memset(spec, 0, sizeof(*spec));
    spec->hdr.src_addr = rte_cpu_to_be_32(table);
    items[0] = (struct rte_flow_item){ .type = RTE_FLOW_ITEM_TYPE_ETH };
    items[1] = (struct rte_flow_item){ .type = RTE_FLOW_ITEM_TYPE_IPV4, .spec = spec,
                                       .mask = &ctx->split_mask };
    items[2] = (struct rte_flow_item){ .type = RTE_FLOW_ITEM_TYPE_END };
}

static const struct rte_flow_action miss_count_actions[] = {
    { .type = RTE_FLOW_ACTION_TYPE_COUNT },
    { .type = RTE_FLOW_ACTION_TYPE_DROP },
    { .type = RTE_FLOW_ACTION_TYPE_END },
};
static const struct rte_flow_action miss_drop_actions[] = {
    { .type = RTE_FLOW_ACTION_TYPE_DROP },
    { .type = RTE_FLOW_ACTION_TYPE_END },
};

const struct rte_flow_action *bf_miss_actions(const struct bf_flow_ctx *ctx)
{
    return ctx->set->check_miss ? miss_count_actions : miss_drop_actions;
}

/* The mode decides the actions; RSS to the hairpin queues is always last. */
/*
 * Actions of the main rules. The list follows the order the HW applies them
 * (counter, meters, push VLAN, modify header, then the fate action), otherwise
 * the PMD rejects the template. mask = true gives the async template masks.
 */
static unsigned main_actions_build(const struct bf_flow_ctx *ctx, struct rte_flow_action *a, bool mask)
{
    const struct bf_mode_info *mode = ctx->set->mode;
    unsigned n = 0;

    if (mode->count)
        a[n++] = (struct rte_flow_action){ .type = RTE_FLOW_ACTION_TYPE_COUNT };
    for (uint8_t m = 0; m < mode->nb_meters; m++) {
        /* TODO (modes meter, all): METER_MARK action, objects reserved
         * through nb_meters in rte_flow_configure(). */
    }
    if (mode->push_vlan) {
        /* SET_VLAN_VID must follow OF_PUSH_VLAN directly. */
        a[n++] = (struct rte_flow_action){ .type = RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN,
                                           .conf = &ctx->push_vlan };
        a[n++] = (struct rte_flow_action){ .type = RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID,
                                           .conf = &ctx->set_vlan_vid };
    }
    if (mode->set_mac)
        a[n++] = (struct rte_flow_action){ .type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
                                           .conf = mask ? &ctx->set_src_mac_mask : &ctx->set_src_mac };
    a[n++] = (struct rte_flow_action){ .type = RTE_FLOW_ACTION_TYPE_RSS, .conf = &ctx->rss };
    a[n++] = (struct rte_flow_action){ .type = RTE_FLOW_ACTION_TYPE_END };
    return n;
}

unsigned bf_main_actions(const struct bf_flow_ctx *ctx, struct rte_flow_action *a)
{
    return main_actions_build(ctx, a, false);
}

unsigned bf_main_masks(const struct bf_flow_ctx *ctx, struct rte_flow_action *a)
{
    return main_actions_build(ctx, a, true);
}

/* ------------------------------------------------------------------------ */
/* Tables and fixed rules: dispatch to the chosen API                       */
/* ------------------------------------------------------------------------ */

int bf_flow_tables_create(struct bf_flow_ctx *ctx)
{
    /* Sync rules need no templates, the PMD manages its tables itself. */
    return ctx->set->sync ? 0 : bf_async_tables_create(ctx);
}

int bf_flow_fixed_rules_insert(struct bf_flow_ctx *ctx)
{
    return ctx->set->sync ? bf_sync_fixed_rules_insert(ctx) : bf_async_fixed_rules_insert(ctx);
}

/* ------------------------------------------------------------------------ */
/* Per-flow rules: parallel insertion on the worker lcores                  */
/* ------------------------------------------------------------------------ */

/* Set by each lcore when it is done; the main lcore polls it for progress. */
struct bf_worker_slot {
    struct bf_worker w;
    int finished;
};

static int worker_main(void *arg)
{
    struct bf_worker_slot *slot = arg;
    struct bf_worker *w = &slot->w;

    if (w->ctx->set->sync)
        bf_sync_worker(w);
    else
        bf_async_worker(w);
    __atomic_store_n(&slot->finished, 1, __ATOMIC_RELEASE);
    return 0;
}

/* Print "inserted X / N" once a second until all workers finish. */
static void wait_with_progress(struct bf_flow_ctx *ctx, struct bf_worker_slot *s, uint16_t nw)
{
    const uint64_t hz = rte_get_timer_hz();
    uint64_t prev_t = rte_get_timer_cycles();
    uint64_t next_print = prev_t + hz;
    uint64_t prev_done = 0;

    for (;;) {
        bool all = true;
        for (uint16_t i = 0; i < nw; i++)
            all &= __atomic_load_n(&s[i].finished, __ATOMIC_ACQUIRE) != 0;
        if (all)
            break;

        uint64_t now = rte_get_timer_cycles();
        if (now >= next_print) {
            uint64_t done = 0;
            for (uint16_t i = 0; i < nw; i++)
                done += __atomic_load_n(&s[i].w.done, __ATOMIC_RELAXED);
            printf("  inserted %10" PRIu64 " / %" PRIu64 " (%5.1f %%), %.3f Mrules/s\n",
                   done, ctx->nb_flows, 100.0 * done / ctx->nb_flows,
                   (done - prev_done) / ((double)(now - prev_t) / hz) / 1e6);
            prev_done = done;
            prev_t = now;
            next_print = now + hz;
        }
        rte_pause();
    }
    rte_eal_mp_wait_lcore();
}

int bf_flow_bulk_insert(struct bf_flow_ctx *ctx, volatile int *stop, struct bf_bulk_result *res)
{
    unsigned lcores[RTE_MAX_LCORE];
    unsigned nb_lcores = 0;
    unsigned lcore;
    int ret = 0;

    memset(res, 0, sizeof(*res));
    if (ctx->nb_flows == 0)
        return 0;

    /* One handle per flow; NULL marks a rule that was not inserted. */
    ctx->flows = calloc(ctx->nb_flows, sizeof(*ctx->flows));
    if (ctx->flows == NULL) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "ERROR: cannot allocate %" PRIu64 " flow handles.\n",
                ctx->nb_flows);
        return -1;
    }

    /* One worker per lcore (and per flow queue); without workers the main lcore inserts. */
    RTE_LCORE_FOREACH_WORKER(lcore) {
        if (nb_lcores < ctx->nb_flow_queues)
            lcores[nb_lcores++] = lcore;
    }
    uint16_t nw = nb_lcores ? nb_lcores : 1;

    struct bf_worker_slot *s = calloc(nw, sizeof(*s));
    if (s == NULL)
        return -1;

    /* Split the flows into nw contiguous ranges of (almost) equal size. */
    uint64_t chunk = ctx->nb_flows / nw;
    uint64_t rem = ctx->nb_flows % nw;
    uint64_t next = 0;
    for (uint16_t i = 0; i < nw; i++) {
        s[i].w.ctx = ctx;
        s[i].w.stop = stop;
        s[i].w.queue_id = i;
        s[i].w.first = next;
        next += chunk + (i < rem ? 1 : 0);
        s[i].w.last = next;
    }

    printf("Inserting %" PRIu64 " rules on %u lcore(s), %s...\n", ctx->nb_flows, nw,
           ctx->set->sync ? "sync rte_flow_create()" : "async flow queues");

    const uint64_t t0 = rte_get_timer_cycles();
    if (nb_lcores == 0) {
        worker_main(&s[0]);
    } else {
        for (uint16_t i = 0; i < nw; i++)
            rte_eal_remote_launch(worker_main, &s[i], lcores[i]);
        wait_with_progress(ctx, s, nw);
    }
    res->seconds = (double)(rte_get_timer_cycles() - t0) / rte_get_timer_hz();

    for (uint16_t i = 0; i < nw; i++) {
        res->ok += s[i].w.ok;
        res->failed += s[i].w.failed;
        res->aborted |= s[i].w.aborted;
        if (s[i].w.error)
            ret = -1;
    }
    free(s);

    printf("Rules inserted: %" PRIu64 " ok, %" PRIu64 " failed in %.3f s = %.3f Mrules/s%s\n",
           res->ok, res->failed, res->seconds,
           res->seconds > 0 ? res->ok / res->seconds / 1e6 : 0.0,
           res->aborted ? " (aborted)" : "");
    return ret;
}

/* ------------------------------------------------------------------------ */
/* Counters and teardown                                                    */
/* ------------------------------------------------------------------------ */

int bf_flow_counter_read(struct bf_flow_ctx *ctx, struct rte_flow *flow,
                         uint64_t *hits, uint64_t *bytes)
{
    const struct rte_flow_action count = { .type = RTE_FLOW_ACTION_TYPE_COUNT };
    struct rte_flow_query_count qc = { 0 };
    struct rte_flow_error err;

    if (flow == NULL)
        return -1;
    if (rte_flow_query(ctx->port_id, flow, &count, &qc, &err) != 0)
        return -1;
    *hits = qc.hits_set ? qc.hits : 0;
    if (bytes != NULL)
        *bytes = qc.bytes_set ? qc.bytes : 0;
    return 0;
}

int bf_flow_miss_read(struct bf_flow_ctx *ctx, uint64_t *packets)
{
    *packets = 0;
    for (uint16_t t = 0; t < ctx->nb_tables; t++) {
        uint64_t hits;

        if (bf_flow_counter_read(ctx, ctx->miss_rules[t], &hits, NULL) != 0)
            return -1;
        *packets += hits;
    }
    return 0;
}

void bf_flow_teardown(struct bf_flow_ctx *ctx)
{
    if (ctx->set->sync)
        bf_sync_teardown(ctx);
    else
        bf_async_teardown(ctx);

    free(ctx->flows);
    ctx->flows = NULL;
    free(ctx->hp_queues);
    ctx->hp_queues = NULL;
}
