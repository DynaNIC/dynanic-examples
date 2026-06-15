/* common_dpdk.h: Common argument functions used in DynaNIC examples.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Jan Privara <privara@dyna-nic.com>, November 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_EXAMPLE_COMMON_ARGS__
#define __DYNANIC_EXAMPLE_COMMON_ARGS__

#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>

#include "common_dpdk.h"

/**
 * @brief Set default values for DPDK settings.
 *
 * @param dpdk Pointer to the DPDK settings structure.
 *
 * @return 0 on success strerror otherwise.
 */
int set_default_dpdk_setts(struct dpdk_settings *dpdk);

/**
 * @brief Set application-specific command line arguments.
 *
 * @param short_args  Pointer to structure with short arguments.
 * @param long_args   Pointer to structure with long arguments.
 */
void set_app_args(char *short_args, struct option *long_args);

/**
 * @brief Get short command line arguments.
 *
 * This function combines common DPDK and application-specific
 * (set using the set_app_args function) short options.
 *
 * @return Pointer to the structure with short arguments.
 */
char * get_short_args();

/**
 * @brief Get long command line arguments.
 *
 * This function combines common DPDK and application-specific
 * (set using the set_app_args function) long options.
 *
 * @return Pointer to the structure with long arguments.
 */
struct option * get_long_args();

/**
 * @brief Parse DPDK command line arguments.
 *
 * Parse (short) options that configure the common set of DPDK settings.
 * is_dpdk_arg(opt) should be checked before using this function to ensure
 * the option is valid in this context.
 *
 * @param opt      Short option to process.
 * @param optarg   Option argument.
 * @param dpdk     Pointer to the DPDK settings structure.
 */
void parse_dpdk_args(int opt, char *optarg, struct dpdk_settings *dpdk);

/**
 * @brief Check whether the short option configures DPDK settings.
 *
 * @return true if the option configures DPDK settins, false otherwise.
 */
bool is_dpdk_arg(int opt);

/**
 * @brief Print help message for common DPDK settings options.
 */
void print_dpdk_args_help();

#endif //__DYNANIC_EXAMPLE_COMMON_ARGS__
