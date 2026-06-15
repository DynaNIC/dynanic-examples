set (SRCS
    ../common/common_args.c
    ../common/common_dpdk.c
    ../common/common_stats.c
    argparse.c
    create_packet.c
    generate.c
)

add_executable(generate
    ${SRCS}
)
