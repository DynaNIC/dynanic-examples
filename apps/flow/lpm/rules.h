/* rules.c: RTE Flow rules for lpm example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, April 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_LPM_RUL__
#define __DYNANIC_LPM_RUL__

#include <rte_ethdev.h>

#define MAX_PATTERN_NUM	3
#define MAX_ACTION_NUM 3

#define DEFAULT_RULE_PRIORITY 5

void create_flow(uint16_t portid, struct rte_flow_error *error);
void rule_factory(
    uint16_t portid,
    struct rte_flow_error *error,
    void *item,
    void *mask,
    enum rte_flow_item_type pattern_type,
    enum rte_flow_action_type action_type,
    void *action_conf,
    uint32_t group,
    uint32_t priority
);
void print_error(struct rte_flow_error *error);

#endif //__DYNANIC_LPM_RUL__
