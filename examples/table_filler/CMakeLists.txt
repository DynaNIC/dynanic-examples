set (SRCS
    ../common/common_args.c
    ../common/common_dpdk.c
    argparse.c
    table_filler.c
)

add_executable(table_filler
    ${SRCS}
)
