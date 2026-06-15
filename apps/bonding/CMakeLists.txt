set (SRCS
    ../common/common_args.c
    ../common/common_dpdk.c
    ../common/common_stats.c
    argparse.c
    bonding.c
)

add_executable(bonding ${SRCS})

target_link_libraries(bonding rte_net_bond)
