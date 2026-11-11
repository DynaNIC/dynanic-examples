/* flow_internal.h: Internals shared by the rule code (flow_rules.c, flow_async.c, flow_sync.c).
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2026
*
* Not part of the public API, include only from the flow_*.c files.
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_BF_TESTER_FLOW_INTERNAL__
#define __DYNANIC_BF_TESTER_FLOW_INTERNAL__

#include <stdint.h>
#include <stdbool.h>

#include <rte_flow.h>

#include "flow_rules.h"

/* Each template table has exactly one pattern and one actions template. */
#define BF_TEMPLATE_IDX 0

/* Room for the actions of one rule including END (future modes add more). */
#define BF_MAX_ACTIONS 8

/* --- Rule building blocks (defined in flow_rules.c) --------------------- */

/* Pattern of a single rule: any packet / any eth-ipv4-tcp / exact 5-tuple. */
extern const struct rte_flow_item bf_any_items[];
extern const struct rte_flow_item bf_tcp_any_items[];
extern const struct rte_flow_item bf_tcp_5t_items[];

/* Masks of the exact 5-tuple match. */
extern const struct rte_flow_item_ipv4 bf_ipv4_5t_mask;
extern const struct rte_flow_item_tcp bf_tcp_5t_mask;

/* Group 0 rules jump to a main table; the target group is given per rule. */
extern const struct rte_flow_action bf_jump_masks[];
void bf_jump_actions(const struct bf_flow_ctx *ctx, uint16_t table, struct rte_flow_action acts[2]);

/* Group 0 split rule "eth / ipv4 src & (N-1) == table" (spec and mask filled). */
void bf_split_items(const struct bf_flow_ctx *ctx, uint16_t table,
                    struct rte_flow_item_ipv4 *spec, struct rte_flow_item items[3]);

/* Main table (0..N-1) of flow idx, from the lowest bits of its source IP. */
uint16_t bf_table_of(const struct bf_flow_ctx *ctx, uint64_t idx);

/* Actions of the main rules, given by the mode. Returns the count incl. END. */
unsigned bf_main_actions(const struct bf_flow_ctx *ctx, struct rte_flow_action *acts);

/* Mode all: actions of the stage 1 rule (counter, meter 1, jump to stage 2 group of
 * the given stage 2 table). bf_main_actions() then gives the stage 2 actions. */
unsigned bf_stage1_actions(const struct bf_flow_ctx *ctx, uint16_t stage2_table,
                           struct rte_flow_action *acts);

/* The same actions with the masks for the async actions template (all values fixed). */
unsigned bf_main_masks(const struct bf_flow_ctx *ctx, struct rte_flow_action *masks);

/* Actions of the miss rule: drop, with a counter when --check-miss is on. */
const struct rte_flow_action *bf_miss_actions(const struct bf_flow_ctx *ctx);

/* Fill the ipv4/tcp item specs with the 5-tuple of flow idx. */
void bf_fill_5tuple(const struct application_settings *set, uint64_t idx,
                    struct rte_flow_item_ipv4 *ip, struct rte_flow_item_tcp *tcp);

void bf_flow_err_log(const char *what, const struct rte_flow_error *err);

/* --- Parallel insertion ------------------------------------------------- */

/* One inserting lcore and its share of the flows: indexes [first, last). */
struct bf_worker {
    struct bf_flow_ctx *ctx;
    volatile int *stop;     /* set by Ctrl+C, checked now and then */
    uint32_t queue_id;      /* async: this lcore's flow queue */
    uint64_t first;
    uint64_t last;

    uint64_t done;          /* rules handed to the PMD so far, for the progress line */
    uint64_t ok;            /* rules confirmed by the HW */
    uint64_t failed;        /* rules rejected by the HW */
    int error;              /* API error, the run is not usable */
    bool aborted;           /* stopped by Ctrl+C */
};

/* --- Async template API (flow_async.c) ---------------------------------- */

int bf_async_tables_create(struct bf_flow_ctx *ctx);
int bf_async_fixed_rules_insert(struct bf_flow_ctx *ctx);
int bf_async_worker(struct bf_worker *w);
void bf_async_teardown(struct bf_flow_ctx *ctx);

/* --- Sync rte_flow_create() (flow_sync.c) ------------------------------- */

int bf_sync_fixed_rules_insert(struct bf_flow_ctx *ctx);
int bf_sync_worker(struct bf_worker *w);
void bf_sync_teardown(struct bf_flow_ctx *ctx);

#endif // __DYNANIC_BF_TESTER_FLOW_INTERNAL__
