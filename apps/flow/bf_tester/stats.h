/* stats.h: Port statistics for bf_tester (hairpin traffic is HW-only, so xstats are used).
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_BF_TESTER_STATS__
#define __DYNANIC_BF_TESTER_STATS__

#include <stdint.h>
#include <stdbool.h>

#include <rte_ethdev.h>

/* Value passed for a flow counter that is not available. */
#define BF_STATS_NA UINT64_MAX

enum bf_xstat {
    BF_XS_RX_PKTS = 0,
    BF_XS_RX_BYTES,
    BF_XS_TX_PKTS,
    BF_XS_TX_BYTES,
    BF_XS_RX_DISCARD,
    BF_XS_TX_DISCARD,
    BF_XS_RX_OOB,
    BF_XS_HP_OOB,
    BF_XS_MAX
};

struct bf_stats {
    uint16_t port_id;

    uint64_t ids[BF_XS_MAX];
    bool have[BF_XS_MAX];
    uint64_t first[BF_XS_MAX];
    uint64_t prev[BF_XS_MAX];

    uint64_t start_tsc;
    uint64_t prev_tsc;
    uint64_t prev_match;
    uint64_t prev_miss;
    uint64_t first_match;
    uint64_t first_miss;
};

/**
 * @brief Resolve xstat ids and take the initial snapshot.
 */
int bf_stats_init(struct bf_stats *st, uint16_t port_id,
                  uint64_t match_pkts, uint64_t miss_pkts);

/**
 * @brief Print one line of rates since the previous call.
 *
 * @param match_pkts Cumulative hits of the single-mode rule or BF_STATS_NA.
 * @param miss_pkts  Cumulative hits of the miss rule or BF_STATS_NA.
 */
void bf_stats_print(struct bf_stats *st, uint64_t match_pkts, uint64_t miss_pkts);

/**
 * @brief Print totals since bf_stats_init().
 */
void bf_stats_summary(struct bf_stats *st, uint64_t match_pkts, uint64_t miss_pkts);

#endif // __DYNANIC_BF_TESTER_STATS__
