set (SRCS
    ../../common/common_args.c
    ../../common/common_dpdk.c
    ../../common/common_stats.c
    argparse.c
    hairpin.c
)

add_executable(hairpin
    ${SRCS}
)
