/* common_stats.h: Functions for statistics printing in DynaNIC examples.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, April 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_EXAMPLE_COMMON_STATS__
#define __DYNANIC_EXAMPLE_COMMON_STATS__

#include <stdint.h>
#include <stdbool.h>

struct measurement_settings {
    uint16_t max_ports;
    uint64_t next_stats_delay;
    uint64_t next_stats_time;
    uint64_t *pre_bytes;
    uint64_t *pre_packets;
    uint64_t *pre_time;
    uint64_t start_time;
    bool print_rx; // otherwise print tx
};

/**
 * @brief Free dynamically allocated statistics resources.
 *
 * @param measurement Pointer to measurements data.
 */
void cleanup_measurement(struct measurement_settings *measurement);

/**
 * @brief Print simple stats in format for easy
 * parsing by measurement script.
 *
 * @param measurement Pointer to measurements data.
 */
void print_simple_stats(struct measurement_settings *measurement);

/**
 * @brief Print xstats for given port.
 * @param port_id Port ID.
 */
void print_queue_xstats(uint16_t port_id);

/**
 * @brief Print stats from all queues/lcores.
 *
 * NOTE: Stats are counted for each port separately.
 *
 * @param measurement Pointer to measurements data.
 */
void print_stats(struct measurement_settings *measurement);

/**
 * @brief Print stats at the end of the measurement.
 *
 * @param measurement Pointer to measurements data.
 */
void print_stats_total(struct measurement_settings *measurement);

/**
 * @brief Init variables for time management.
 *
 * @param measurement Pointer to measurements data.
 *
 * @return 0 on success, a negative error code (-1) in case of a failure.
 */
int time_variables_init(struct measurement_settings *measurement);

/**
 * @brief Print current statistics in given time periods.
 *
 * @param measurement Pointer to measurements data.
 */
void gather_stats(struct measurement_settings *measurement);

#endif //__DYNANIC_EXAMPLE_COMMON_STATS__
