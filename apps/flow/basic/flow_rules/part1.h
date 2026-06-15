/* part1.h: Define flow rules for flow example part1.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, December 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_FLOW_RULE_PART1__
#define __DYNANIC_FLOW_RULE_PART1__

#include <rte_flow.h>

struct rte_flow* create_flow_drop_odd(uint16_t portid, struct rte_flow_error *error);

#endif // __DYNANIC_FLOW_RULE_PART1__
