/* flow_rules.h: Rule management for bf_tester (public API, async and sync).
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_BF_TESTER_FLOW_RULES__
#define __DYNANIC_BF_TESTER_FLOW_RULES__

#include <stdint.h>
#include <stdbool.h>

#include <rte_ether.h>
#include <rte_flow.h>
#include <rte_spinlock.h>

#include "argparse.h"

/* First group with the matching rules; with --tables N they use groups 1..N. */
#define BF_MAIN_GROUP 1

/* Upper limit of --tables. */
#define BF_MAX_TABLES BF_MAX_TABLES_ARG

/*
 * Maximum fill of the per-flow template table, as the BWC matcher used by
 * sync rte_flow_create() on HW steering (MLX5DR_BWC_MATCHER_REHASH_PERCENT_TH).
 * A fuller table pushes rules to the HWS collision table, which costs lookup
 * performance (measured: 95 % fill -> 334 Gbps instead of 390 Gbps).
 */
#define BF_TABLE_MAX_FILL_PCT 70

/* Insert mode: VLAN ID pushed onto every forwarded packet; the test VLAN of the
 * switch, so the switch accepts the tagged frames on the access port. */
#define BF_INSERT_VLAN_ID 3

/*
 * Meter modes: srTCM profile shared by all meters. 1 Gbit/s is far above the
 * rate of one flow (2M flows at 400G is ~0.2 Mbit/s each), so every packet
 * stays green: the meter only marks the color, nothing is dropped, and the
 * measurement shows the cost of metering alone.
 */
#define BF_METER_PROFILE_ID 1
#define BF_METER_CIR_BYTES  (1000000000ull / 8)    /* 1 Gbit/s */
#define BF_METER_CBS_BYTES  (64 * 1024)
#define BF_METER_EBS_BYTES  (64 * 1024)

struct bf_flow_ctx {
    uint16_t port_id;
    const struct application_settings *set;

    /* One async flow queue per inserting lcore. */
    uint16_t nb_flow_queues;

    /* HW steering flow engine configured (rte_flow_configure() done). */
    bool hws;

    /* Flow engine facts for the setup report (HW steering only). */
    uint32_t max_counters;
    uint32_t nb_counters;
    uint16_t nb_cfg_queues;     /* flow queues passed to rte_flow_configure() */

    /*
     * Sync rules with a counter or a meter: rte_flow_create() is called under
     * sync_lock. mlx5 (DPDK 25.11) takes the counter of every sync rule from the
     * cache of flow queue 0, a single-producer/single-consumer ring, so parallel
     * calls corrupt it (crash or failed insertion). A sync METER_MARK takes a job
     * from the control queue without a lock (flow_hw_action_job_init()).
     */
    bool sync_serialize;
    rte_spinlock_t sync_lock;

    /* RSS action spreading matched traffic over the hairpin queues. */
    uint16_t *hp_queues;
    struct rte_flow_action_rss rss;

    /* edit_mac / insert modes: set the source MAC to the port's MAC and push a VLAN
     * (same values for all rules). */
    struct rte_ether_addr port_mac;
    struct rte_flow_action_of_push_vlan push_vlan;
    struct rte_flow_action_of_set_vlan_vid set_vlan_vid;
    struct rte_flow_action_modify_field set_src_mac;
    struct rte_flow_action_modify_field set_src_mac_mask;   /* async template mask */

    /* meter modes: METER_MARK with one shared profile; the PMD creates a meter
     * object for every rule. */
    struct rte_flow_action_meter_mark meter_mark;
    uint32_t nb_meters;         /* meters reserved in rte_flow_configure() */
    uint32_t max_meters;        /* PMD maximum, read after rte_flow_configure() */

    /*
     * Mode all (two meters per flow). A HWS rule has a single meter slot
     * (action_order_arr in mlx5dr_action.c), so each flow goes through two rules:
     *   stage 1, group BF_MAIN_GROUP + t:     counter, meter 1, jump to stage 2
     *   stage 2, group bf_stage2_group(t):    meter 2 (color aware), VLAN, MAC, RSS
     * Stage 2 has one 5-tuple rule per flow, or with --group-meter one rule
     * per source IP in a single group (the meter is shared by those flows).
     */
    bool two_stage;
    uint16_t nb_stage2_tables;  /* nb_tables, or 1 with --group-meter */
    uint32_t nb_src_groups;     /* --group-meter: stage 2 rules (source IPs) */
    struct rte_flow_action_meter_mark meter_mark2;          /* meter 2: color aware */
    struct rte_flow_action_jump stage2_jump[BF_MAX_TABLES]; /* to the stage 2 group of table t */

    /*
     * Main tables (--tables N): table t lives in group BF_MAIN_GROUP + t and
     * gets the flows whose source IP has t in its lowest log2(N) bits.
     */
    uint16_t nb_tables;
    uint64_t table_flows[BF_MAX_TABLES];    /* rules per table */
    uint64_t table_size[BF_MAX_TABLES];     /* async: template table capacity */
    struct rte_flow_action_jump jump_conf[BF_MAX_TABLES];  /* jump to table t */
    struct rte_flow_item_ipv4 split_mask;   /* src IP bits selecting the table */

    /* Async templates. */
    struct rte_flow_pattern_template *pt_any;
    struct rte_flow_pattern_template *pt_split;
    struct rte_flow_pattern_template *pt_main;
    struct rte_flow_actions_template *at_jump;
    struct rte_flow_actions_template *at_main;
    struct rte_flow_actions_template *at_miss;

    /* Async tables: group 0 split + catch-all, per group main + miss. */
    struct rte_flow_template_table *tbl_split;
    struct rte_flow_template_table *tbl_root;
    struct rte_flow_template_table *tbl_main[BF_MAX_TABLES];
    struct rte_flow_template_table *tbl_miss[BF_MAX_TABLES];

    /* Fixed rules. */
    struct rte_flow *root_rule;                     /* group 0: rest -> group 1 */
    struct rte_flow *split_rules[BF_MAX_TABLES];    /* group 0: src IP bits -> table t (N > 1) */
    struct rte_flow *miss_rules[BF_MAX_TABLES];     /* per group: drop the rest */
    struct rte_flow *single_rule;                   /* single mode only */
    struct rte_flow *egress_rules[2];               /* --egress-vlan: jump, push VLAN */
    struct rte_flow *stage2_miss_rules[BF_MAX_TABLES];  /* mode all: per stage 2 group */
    struct rte_flow **group_rules;                  /* --group-meter: one per source IP */

    /* Per-flow mode: one handle per flow, NULL if the insertion failed.
     * Mode all: flows are the stage 1 rules (with the counter), flows2 the
     * stage 2 rules (not with --group-meter). */
    struct rte_flow **flows;
    struct rte_flow **flows2;
    uint64_t nb_flows;
};

/* Group of the stage 2 rules of main table t (mode all), after the main groups. */
static inline uint32_t bf_stage2_group(const struct bf_flow_ctx *ctx, uint16_t table)
{
    return BF_MAIN_GROUP + ctx->nb_tables + table;
}

/* Result of a bulk insert run. */
struct bf_bulk_result {
    uint64_t ok;
    uint64_t failed;
    double seconds;
    bool aborted;
};

/**
 * @brief Initialise the context and configure the flow engine.
 *
 * Must be called after rte_eth_dev_configure() and before rte_eth_dev_start().
 * With HW steering it reserves the flow queues and counters; with SW steering
 * (only allowed for --sync) there is nothing to configure.
 */
int bf_flow_engine_configure(struct bf_flow_ctx *ctx, uint16_t port_id,
                             const struct application_settings *set,
                             uint16_t first_hp_queue, uint16_t nb_flow_queues);

/**
 * @brief Async: create the templates and template tables. Sync: nothing to do.
 *
 * Called after the port is started and hairpin queues are bound.
 */
int bf_flow_tables_create(struct bf_flow_ctx *ctx);

/**
 * @brief Insert the fixed rules: group 0 jump, miss rule and, in single
 * mode, the eth/ipv4/tcp rule.
 */
int bf_flow_fixed_rules_insert(struct bf_flow_ctx *ctx);

/**
 * @brief Insert the per-flow rules in parallel on all worker lcores.
 *
 * Progress is printed every second. Stops early when *stop becomes non-zero.
 */
int bf_flow_bulk_insert(struct bf_flow_ctx *ctx, volatile int *stop, struct bf_bulk_result *res);

/**
 * @brief Destroy fixed rules, tables and templates.
 *
 * Per-flow rules are left to rte_eth_dev_close(), which frees them much faster.
 */
void bf_flow_teardown(struct bf_flow_ctx *ctx);

/**
 * @brief Sum of packets dropped by the miss rules of all tables (--check-miss).
 */
int bf_flow_miss_read(struct bf_flow_ctx *ctx, uint64_t *packets);

/**
 * @brief Read the counter of a rule created with a COUNT action.
 */
int bf_flow_counter_read(struct bf_flow_ctx *ctx, struct rte_flow *flow,
                         uint64_t *hits, uint64_t *bytes);

/**
 * @brief Make the counter of a rule count from now on (later reads return the delta).
 */
int bf_flow_counter_reset(struct bf_flow_ctx *ctx, struct rte_flow *flow);

/*
 * mlx5 HWS refreshes the counter values from the HW in the background, every
 * svc_cycle_time (500 ms by default), and a query returns the last refreshed
 * value. Waiting this long after the last rule change makes the values current.
 */
#define BF_CNT_SETTLE_MS 1000

/**
 * @brief Number of distinct flows in the stream (lcm of the field counts).
 */
uint64_t bf_flow_distinct_tuples(const struct application_settings *set);

/**
 * @brief Compute the 5-tuple of flow number idx (host byte order).
 */
void bf_flow_tuple(const struct application_settings *set, uint64_t idx,
                   uint32_t *src_ip, uint32_t *dst_ip,
                   uint16_t *src_port, uint16_t *dst_port);

#endif // __DYNANIC_BF_TESTER_FLOW_RULES__
