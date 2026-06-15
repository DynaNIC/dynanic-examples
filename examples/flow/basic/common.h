/* common.h: Common functions used in the flow example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, December 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_FLOW_COMMON__
#define __DYNANIC_FLOW_COMMON__

#include <rte_ethdev.h>

#include "argparse.h"
#include "statistics.h"

void cleanup(uint16_t portid);
int init(
    int argc,
    char *argv[],
    struct application_settings *app_settings
);

#endif //__DYNANIC_FLOW_COMMON__
