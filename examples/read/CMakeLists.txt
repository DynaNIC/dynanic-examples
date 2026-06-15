set (SRCS
    ../common/common_args.c
    ../common/common_dpdk.c
    ../common/common_stats.c
    argparse.c
    read.c
)

add_executable(read
    ${SRCS}
)
