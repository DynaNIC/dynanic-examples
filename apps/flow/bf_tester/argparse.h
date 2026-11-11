/* argparse.h: Argument parser for the bf_tester application.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_BF_TESTER_ARGPARSE__
#define __DYNANIC_BF_TESTER_ARGPARSE__

#include <stdint.h>
#include <stdbool.h>

#include "common_dpdk.h"

/*
 * Measurement modes. They are incremental: every mode contains everything
 * from the previous one. The features of each mode are in bf_mode_table.
 */
enum bf_mode {
    BF_MODE_SINGLE = 0, /* baseline: one eth/ipv4/tcp rule -> hairpin */
    BF_MODE_PER_FLOW,   /* 1:  one exact 5-tuple rule per flow */
    BF_MODE_EDIT_MAC,   /* 2a: + set the source MAC */
    BF_MODE_INSERT,     /* 2b: + push VLAN (header insertion) */
    BF_MODE_COUNT,      /* 3a: + counter per rule */
    BF_MODE_METER,      /* 3b: + meter per rule */
    BF_MODE_ALL,        /* 4:  + counter and two meters per rule */
    BF_MODE_MAX
};

struct bf_mode_info {
    enum bf_mode id;
    const char *name;
    const char *desc;
    bool implemented;
    bool per_flow;      /* one rule per flow instead of one catch-all rule */
    bool set_mac;       /* set the source MAC to the port's MAC */
    bool push_vlan;     /* push a VLAN header */
    bool count;         /* counter per rule */
    uint8_t nb_meters;  /* meters per rule */
};

/*
 * One varying header field, like a Spirent modifier: values base, base+step,
 * ... (count values). As in Spirent, all fields step with every flow and each
 * wraps at its own count.
 */
struct bf_field_gen {
    uint32_t base;
    uint32_t count;
    uint32_t step;
};

/* Upper limit of --tables. */
#define BF_MAX_TABLES_ARG 64

struct application_settings
{
    /* Mempool / descriptor defaults for the (single) standard queue. */
    struct dpdk_settings dpdk;

    const struct bf_mode_info *mode;

    /* Number of hairpin queues the matched traffic is spread over. */
    uint16_t nb_hp_queues;

    /* Per-flow rule generation. */
    uint64_t nb_flows;      /* 0 = all distinct flows of the stream */
    struct bf_field_gen src_ip;
    struct bf_field_gen dst_ip;
    struct bf_field_gen src_port;
    struct bf_field_gen dst_port;

    /* Spread the per-flow rules over this many tables (power of two, 1 = one table). */
    uint16_t nb_tables;

    /* Count packets hitting the miss rule and report them while running. */
    bool check_miss;

    /* Async flow engine tuning (rule insertion speed). */
    uint32_t flow_queue_size;
    uint32_t push_burst;

    /* Create rules with synchronous rte_flow_create() instead of the async API. */
    bool sync;

    /* Run control. */
    uint64_t stats_interval_ms;
    bool insert_only;
    bool no_stats;      /* do not read any statistics or counters */
};

int parse_custom_args(int argc, char *argv[], struct application_settings *app_settings);
void print_help(void);

#endif // __DYNANIC_BF_TESTER_ARGPARSE__
