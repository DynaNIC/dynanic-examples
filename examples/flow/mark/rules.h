/* rules.c: RTE Flow rules for MARK example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, April 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_MARK_RUL__
#define __DYNANIC_MARK_RUL__

#include <rte_ethdev.h>

#define MAX_PATTERN_NUM	3
#define MAX_ACTION_NUM 3

#define MARK_ONE 1
#define MARK_TWO 2
#define MARK_OTHER 3

void create_rule1(uint16_t portid, struct rte_flow_error *error);
void create_rule2(uint16_t portid, struct rte_flow_error *error);
void create_rule3(uint16_t portid, struct rte_flow_error *error);
void create_flow(uint16_t portid, struct rte_flow_error *error);

#endif //__DYNANIC_MARK_RUL__
