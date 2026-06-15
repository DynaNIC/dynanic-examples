/* common_dpdk.h: Common DPDK functions used in DynaNIC examples.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Jan Privara <privara@dyna-nic.com>, November 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_EXAMPLE_COMMON_DPDK__
#define __DYNANIC_EXAMPLE_COMMON_DPDK__

#include <stdint.h>
#include <stdbool.h>

#include <rte_ethdev.h>

#define ERROR_DO_CLEANUP   -2
#define ERROR_DONT_CLEANUP -1

struct dpdk_settings
{
    // Variables for mempool setup.
    uint64_t burst_size;
    uint64_t descriptors;
    uint64_t mempool_cache_size;
    uint64_t mempool_size;
    uint64_t mbuf_size;

    // Number of ports and queues.
    uint16_t ports;
    uint32_t queues_per_port;

    // Port configuration.
    struct rte_eth_conf port_conf;
};

struct lcore_arguments
{
    void *app_settings;
    int worker_id;
    int start_index;
    int end_index;
    struct rte_mempool *mbuf_pool;
};

/**
 * @brief Configure device for RX/TX.
 *
 * Initialize all ports and setup RX and/or TX queues as requested in
 * the dpdk_settings argument.
 *
 * @param mbuf_pool     Pointer to mbuf pool.
 * @param dpdk_settings Pointer to DPDK settings structure.
 * @param rx_en         Enable RX / receive direction.
 * @param tx_en         Enable TX / transmit direction.
 *
 * @return 0 on success, a negative error code in case of failure.
 */
int device_setup(
    struct rte_mempool *mbuf_pool,
    struct dpdk_settings *dpdk_settings,
    bool rx_en,
    bool tx_en
);

/**
 * @brief Get number of worker cores.
 *
 * @return Number of worker lcores.
 */
int get_worker_cores_cnt();

/**
 * @brief Check if there is enought worker cores.
 *
 * @return 0 if there is enought worker cores -1 otherwise.
 */
int check_worker_cores_cnt();

/**
 * @brief Get number of DPDK queues per one worker lcore.
 *
 * @param dpdk_setts Pointer to the DPDK settings structure.
 *
 * @return Number of DPDK queues.
 */
int get_queues_per_lcore(struct dpdk_settings *dpdk_setts);

/**
 * @brief Get number of remaining DPDK queues for the last worker lcore.
 *
 * @param dpdk_setts Pointer to the DPDK settings structure.
 *
 * @return @return Number of DPDK queues.
 */
int get_remaining_queues(struct dpdk_settings *dpdk_setts);

/**
 * @brief Initialize resources for workers job assignment.
 *
 * Use this function before calling workers_job_assign.
 *
 * @param dpdk_setts  Pointer to DPDK settings structure.
 * @param lcore_param Pointer to parameters structure for the
 *                    particular worker lcore.
 * @param mbuf_pool   Pointer to common mbuf pool.
 */
void workers_job_init(struct dpdk_settings *dpdk_setts, void *lcore_param, struct rte_mempool *mbuf_pool);

/**
 * @brief Assign work (range of DPDK queues) for next worker lcore.
 *
 * This function is expected to be used after
 * initializing the resources by workers_job_init.
 *
 * Should be used iterratively to assign work to all available worker lcores.
 *
 * @param lcore_id  LCore ID.
 * @param lcore_args Pointer to parameters structure for the
 *                    particular worker lcore.
 *
 * @return true if some work was assigned to the worker,
 *         false otherwise - such lcore does not need to launched.
 */
bool workers_job_assign(unsigned int lcore_id, struct lcore_arguments *lcore_args);

/**
 * @brief Free resources associated with allocated ports.
 *
 * @param started_ports Number of started ports.
 */
void cleanup(uint16_t started_ports);

#endif //__DYNANIC_EXAMPLE_COMMON_DPDK__
