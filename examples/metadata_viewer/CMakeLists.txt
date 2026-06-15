set (SRCS
    ../common/common_args.c
    ../common/common_dpdk.c
    ../common/common_stats.c
    argparse.c
    metadata_viewer.c
)

add_executable(meta_viewer
    ${SRCS}
)
