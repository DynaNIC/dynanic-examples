/* minimal_generate.c: Minimal example for generatng packets.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, October 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include <rte_ethdev.h>

#include <signal.h>

#define MIN_MEMPOOL_SIZE 8196

struct lcore_arguments
{
    int worker_id;
    uint32_t total_queues;
    uint64_t burst_size;
    struct rte_mempool *mbuf_pool;
};

/* Flag variable to determine when to stop packet processing. */
static volatile int __stop = 0;

/* Handle CTRL+C and kill signals. */
static void handle_sig(int sig)
{
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            __stop = 1;
            break;
    }
}

// Return whether to stop main loop or not.
static int stop_loop(void)
{
    return __stop;
}

// Main function for worker lcore process.
int lcore_main(void *arg)
{
    struct lcore_arguments *lcore_args = arg;

    int worker_id = lcore_args->worker_id;
    int total_queues = lcore_args->total_queues;
    int burst_size = lcore_args->burst_size;
    struct rte_mempool *mbuf_pool = lcore_args->mbuf_pool;

    uint16_t packet_len = 512;

    uint16_t nb_tx = 0;
    char *pkt;
    uint16_t port_id = 0;
    uint16_t queue_id;
    int ret;
    struct rte_mbuf *tx_packets[burst_size];

    if (worker_id != 0) {
        printf("Lcore %d leaving, only one lcore is needed!\n", rte_lcore_id());
        return 0;
    } else {
        printf("Lcore started packet generating.\n");
    }

    // Start packet transmition.
    while (!stop_loop()) {
        for (queue_id = 0; queue_id < total_queues; queue_id++) {
            ret = rte_pktmbuf_alloc_bulk(mbuf_pool, tx_packets, burst_size);
            if (ret != 0) {
                rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_pktmbuf_alloc_bulk() failed.\n");
                return -rte_errno;
            }

            for (int i = 0; i < burst_size; i++) {
                pkt = rte_pktmbuf_append(tx_packets[i], packet_len);

                // Here you can alter packets
                memset(pkt, 0, packet_len);
                struct rte_ether_hdr *eth_hdr = (struct rte_ether_hdr*) pkt;
                struct rte_ether_addr src_mac = {{10,10,10,10,10,10}};
                rte_ether_addr_copy(&src_mac, &eth_hdr->src_addr);
                eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

                struct rte_ipv4_hdr *ipv4_hdr = (struct rte_ipv4_hdr *)(pkt + sizeof(struct rte_ether_hdr));
                ipv4_hdr->src_addr = inet_addr("192.168.1.1");
                ipv4_hdr->version_ihl = 0x45;
                ipv4_hdr->next_proto_id = IPPROTO_UDP;
                ipv4_hdr->hdr_checksum = rte_ipv4_cksum(ipv4_hdr);
            }

            nb_tx = rte_eth_tx_burst(port_id, queue_id, tx_packets, burst_size);

            // Free unsend packets.
            if (nb_tx != burst_size) {
                for (int i = nb_tx; i < burst_size; i++) {
                    rte_pktmbuf_free(tx_packets[i]);
                }
            }
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int ret;
    struct rte_mempool *mbuf_pool;
    struct rte_eth_conf port_conf = {0};
    struct rte_eth_txconf txconf;

    uint64_t burst_size = 32;
    uint64_t descriptors = 32;
    uint64_t mempool_cache_size = 512;
    uint64_t mbuf_size = 1518 + RTE_PKTMBUF_HEADROOM;
    uint16_t ports = 1;
    uint16_t port_id = 0;
    uint32_t rx_queues = 0;
    uint32_t tx_queues = 8;

    /* NOTE: optimal mempool size can be counted as:
        nports * nb_rx_queue * nb_rxd +
	    nports * nb_lcores * MAX_PKT_BURST +
	    nports * n_tx_queue * nb_txd +
	    nb_lcores * MEMPOOL_CACHE_SIZE

        But min should be 8192.

        This info is taken from l3fwd example's source code.
    */
    uint64_t mempool_size = RTE_MAX(
        ports * rx_queues * descriptors + \
        ports * rte_lcore_count() * burst_size + \
        ports * tx_queues * descriptors + \
        rte_lcore_count() * mempool_cache_size, MIN_MEMPOOL_SIZE
    );

    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eal_init() failed.\n");
        rte_exit(EXIT_FAILURE, "ERROR: init() failed.\n");
    }
    argc -= ret;
    argv += ret;

    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    /* Here you can add you own arg parsing.   */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

    // Register signal for loop stopping.
    signal(SIGINT, &handle_sig);
    signal(SIGTERM, &handle_sig);

    // Create mbuf.
    mbuf_pool = rte_pktmbuf_pool_create(
        "pcap_pool",
        mempool_size,
        mempool_cache_size,
        0,
        mbuf_size,
        rte_socket_id()
    );
    if (mbuf_pool == NULL) {
        rte_exit(-rte_errno, "ERROR: rte_pktmbuf_pool_create() failed.\n");
    }

    struct rte_eth_dev_info dev_info;
    // Get informations about device.
    ret = rte_eth_dev_info_get(port_id, &dev_info);
    if (ret != 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eth_dev_info_get() faild for port %u: %s\n",
                port_id,
                strerror(-ret)
        );

        return ret;
    }

    if (!rte_eth_dev_is_valid_port(port_id))
        rte_exit(EXIT_FAILURE, "ERROR: rte_eth_dev_is_valid_port() failed.\n");

    ret = rte_eth_dev_configure(port_id, rx_queues, tx_queues, &port_conf);
    if (ret != 0)
        rte_exit(EXIT_FAILURE, "ERROR: rte_eth_dev_configure() failed.\n");


    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;

    // Setup TX queues.
    for (int queue_id = 0; queue_id < tx_queues; queue_id++) {
        ret = rte_eth_tx_queue_setup(
            port_id,
            queue_id,
            descriptors,
            rte_eth_dev_socket_id(port_id),
            &txconf
        );
        if (ret < 0) {
            rte_exit(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eth_tx_queue_setup() for queue %d and port %d failed\n", queue_id, port_id);
        }
    }

    // Start port.
    ret = rte_eth_dev_start(port_id);
    if (ret < 0) {
        rte_exit(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eth_dev_start() failed for port %" PRIu16 "\n", port_id);
    }

    rte_log(RTE_LOG_INFO , RTE_LOGTYPE_EAL, "INFO: Start port: %" PRIu16 "\n", port_id);

    struct lcore_arguments lcore_args = {
        .worker_id = 0,
        .total_queues = tx_queues,
        .burst_size = burst_size,
        .mbuf_pool = mbuf_pool,
    };

    // This way you can start lcore_main function for each worker core.
    // RTE_LCORE_FOREACH_WORKER(lcore_id) {
    //     lcore_args.worker_id = worker_id++;
    //     rte_eal_remote_launch(lcore_main, &lcore_args, lcore_id);
    // }

    // But in this example only main lcore will do all the work
    lcore_main(&lcore_args);

    // Wait for all worker lcores to finish.
    rte_eal_mp_wait_lcore();

    // Get stats.
    struct rte_eth_stats stats;
    rte_eth_stats_get(port_id, &stats);
    uint64_t packets = stats.opackets;
    uint64_t bytes = stats.obytes;

    printf("\nPackets: %" PRIu64 ", Bytes: %" PRIu64 "\n", packets, bytes);

    rte_eth_dev_stop(port_id);
    rte_eal_cleanup();

    return 0;
}
