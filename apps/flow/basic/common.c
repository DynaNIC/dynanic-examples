/* common.c: Common functions used in the flow example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, December 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "common.h"
#include "flow_control.h"

void cleanup(uint16_t portid) {
    destroy_all_flow_rules(portid);
    rte_eth_dev_stop(portid);
    rte_eal_cleanup();
}

/* Init all necessary components. */
int init(
    int argc,
    char *argv[],
    struct application_settings *app_settings
) {
    int ret;

    /* DPDK enviroment init. */
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eal_init() failed.\n");
        return ret;
    }
    argc -= ret;
    argv += ret;

    /* Parse custom command line arguments. */
    ret = parse_custom_args(argc, argv, app_settings);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: parse_custom_args() failed.\n");
        return ret;
    }

    return 0;
}
