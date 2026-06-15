set (SRCS
    ../common/common_args.c
    ../common/common_dpdk.c
    ../common/common_stats.c
    ../common/common_pcap.c
    argparse.c
    receive.c
)

add_executable(receive
    ${SRCS}
)
