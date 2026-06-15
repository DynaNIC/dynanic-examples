/* flow_control.h: Define flow rules for flow example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, December 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_FLOW_CONTROL__
#define __DYNANIC_FLOW_CONTROL__

#include <rte_flow.h>

#include "argparse.h"
#include "flow_rules/part1.h"
#include "flow_rules/part2.h"
#include "flow_rules/part3.h"

#define PART1 1
#define PART2 2
#define PART3 3

/* Create flow rule. */
int create_flow(uint16_t portid, struct application_settings *app_settings);
struct rte_flow* create_flow_pass_even(uint16_t portid, struct rte_flow_error *error);
int destroy_all_flow_rules(uint16_t portid);

#endif // __DYNANIC_FLOW_CONTROL__
