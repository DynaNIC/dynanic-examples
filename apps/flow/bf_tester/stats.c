/* stats.c: Port statistics for bf_tester (hairpin traffic is HW-only, so xstats are used).
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2026
*
* Hairpinned packets never reach the SW queues, so the basic ethdev counters
* (ipackets/opackets) do not see them. The mlx5 physical port xstats do.
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "stats.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_cycles.h>

static const char *const xstat_names[BF_XS_MAX] = {
    [BF_XS_RX_PKTS]    = "rx_phy_packets",
    [BF_XS_RX_BYTES]   = "rx_phy_bytes",
    [BF_XS_TX_PKTS]    = "tx_phy_packets",
    [BF_XS_TX_BYTES]   = "tx_phy_bytes",
    [BF_XS_RX_DISCARD] = "rx_phy_discard_packets",
    [BF_XS_TX_DISCARD] = "tx_phy_discard_packets",
    [BF_XS_RX_OOB]     = "rx_out_of_buffer",
    [BF_XS_HP_OOB]     = "hairpin_out_of_buffer",
};

static void read_selected(struct bf_stats *st, uint64_t *out)
{
    for (int i = 0; i < BF_XS_MAX; i++) {
        out[i] = 0;
        if (st->have[i])
            rte_eth_xstats_get_by_id(st->port_id, &st->ids[i], &out[i], 1);
    }
}

int bf_stats_init(struct bf_stats *st, uint16_t port_id,
                  uint64_t match_pkts, uint64_t miss_pkts)
{
    memset(st, 0, sizeof(*st));
    st->port_id = port_id;

    for (int i = 0; i < BF_XS_MAX; i++) {
        st->have[i] = rte_eth_xstats_get_id_by_name(port_id, xstat_names[i], &st->ids[i]) == 0;
        if (!st->have[i])
            printf("NOTE: xstat '%s' not available on this port.\n", xstat_names[i]);
    }
    read_selected(st, st->first);
    memcpy(st->prev, st->first, sizeof(st->prev));

    st->first_match = st->prev_match = match_pkts;
    st->first_miss = st->prev_miss = miss_pkts;
    st->start_tsc = st->prev_tsc = rte_get_timer_cycles();
    return 0;
}

static double mpps(uint64_t pkts, double dt)
{
    return dt > 0 ? pkts / dt / 1e6 : 0.0;
}

static double gbps(uint64_t bytes, double dt)
{
    return dt > 0 ? bytes * 8.0 / dt / 1e9 : 0.0;
}

static void print_counter_rate(const char *name, uint64_t cur, uint64_t prev, double dt)
{
    if (cur == BF_STATS_NA || prev == BF_STATS_NA)
        printf(" | %s n/a", name);
    else
        printf(" | %s %7.2f Mpps", name, mpps(cur - prev, dt));
}

void bf_stats_print(struct bf_stats *st, uint64_t match_pkts, uint64_t miss_pkts)
{
    uint64_t cur[BF_XS_MAX], d[BF_XS_MAX];
    uint64_t now = rte_get_timer_cycles();
    double dt = (double)(now - st->prev_tsc) / rte_get_timer_hz();
    double t = (double)(now - st->start_tsc) / rte_get_timer_hz();

    read_selected(st, cur);
    for (int i = 0; i < BF_XS_MAX; i++)
        d[i] = cur[i] - st->prev[i];

    printf("[%7.1fs] RX %7.2f Mpps %7.2f Gbps | TX %7.2f Mpps %7.2f Gbps"
           " | rx_disc %" PRIu64 " tx_disc %" PRIu64 " rx_oob %" PRIu64 " hp_oob %" PRIu64,
           t,
           mpps(d[BF_XS_RX_PKTS], dt), gbps(d[BF_XS_RX_BYTES], dt),
           mpps(d[BF_XS_TX_PKTS], dt), gbps(d[BF_XS_TX_BYTES], dt),
           d[BF_XS_RX_DISCARD], d[BF_XS_TX_DISCARD], d[BF_XS_RX_OOB], d[BF_XS_HP_OOB]);
    if (match_pkts != BF_STATS_NA)
        print_counter_rate("match", match_pkts, st->prev_match, dt);
    if (miss_pkts != BF_STATS_NA && st->prev_miss != BF_STATS_NA) {
        uint64_t missed = miss_pkts - st->prev_miss;
        if (missed == 0)
            printf(" | miss 0");
        else
            printf(" | MISS %" PRIu64 " pkts (%.2f Mpps) !!", missed, mpps(missed, dt));
    }
    printf("\n");

    memcpy(st->prev, cur, sizeof(cur));
    st->prev_tsc = now;
    st->prev_match = match_pkts;
    st->prev_miss = miss_pkts;
    fflush(stdout);
}

void bf_stats_summary(struct bf_stats *st, uint64_t match_pkts, uint64_t miss_pkts)
{
    uint64_t cur[BF_XS_MAX];
    double t = (double)(rte_get_timer_cycles() - st->start_tsc) / rte_get_timer_hz();

    read_selected(st, cur);
    printf("\n=== Traffic summary (%.1f s) ===\n", t);
    if (st->have[BF_XS_RX_PKTS] && st->have[BF_XS_RX_BYTES])
        printf("  %-24s %12.2f Mpps %10.2f Gbps\n", "RX average",
               mpps(cur[BF_XS_RX_PKTS] - st->first[BF_XS_RX_PKTS], t),
               gbps(cur[BF_XS_RX_BYTES] - st->first[BF_XS_RX_BYTES], t));
    if (st->have[BF_XS_TX_PKTS] && st->have[BF_XS_TX_BYTES])
        printf("  %-24s %12.2f Mpps %10.2f Gbps\n", "TX average",
               mpps(cur[BF_XS_TX_PKTS] - st->first[BF_XS_TX_PKTS], t),
               gbps(cur[BF_XS_TX_BYTES] - st->first[BF_XS_TX_BYTES], t));
    for (int i = 0; i < BF_XS_MAX; i++) {
        if (st->have[i])
            printf("  %-24s %20" PRIu64 "\n", xstat_names[i], cur[i] - st->first[i]);
    }
    if (match_pkts != BF_STATS_NA && st->first_match != BF_STATS_NA)
        printf("  %-24s %20" PRIu64 "\n", "rule match packets", match_pkts - st->first_match);
    if (miss_pkts != BF_STATS_NA && st->first_miss != BF_STATS_NA) {
        uint64_t missed = miss_pkts - st->first_miss;
        uint64_t rx = cur[BF_XS_RX_PKTS] - st->first[BF_XS_RX_PKTS];
        printf("  %-24s %20" PRIu64, "miss (dropped) packets", missed);
        if (st->have[BF_XS_RX_PKTS] && rx > 0)
            printf("  (%.4f %% of RX)", 100.0 * missed / rx);
        printf("\n");
    }
    if (st->have[BF_XS_RX_PKTS] && st->have[BF_XS_TX_PKTS]) {
        uint64_t rx = cur[BF_XS_RX_PKTS] - st->first[BF_XS_RX_PKTS];
        uint64_t tx = cur[BF_XS_TX_PKTS] - st->first[BF_XS_TX_PKTS];
        printf("  %-24s %20" PRId64 "\n", "rx - tx", (int64_t)(rx - tx));
    }
    if (miss_pkts != BF_STATS_NA && st->first_miss != BF_STATS_NA) {
        uint64_t missed = miss_pkts - st->first_miss;
        if (missed == 0)
            printf("Miss check: OK, every packet matched a rule.\n");
        else
            printf("Miss check: FAILED, %" PRIu64 " packets matched no rule (dropped).\n", missed);
    }
}
