set (SRCS
    ../../common/common_args.c
    ../../common/common_dpdk.c
    argparse.c
    flow_rules.c
    flow_async.c
    flow_sync.c
    stats.c
    bf_tester.c
)

add_executable(bf_tester
    ${SRCS}
)

# Async flow / template API is still partly experimental in DPDK.
target_compile_definitions(bf_tester PRIVATE ALLOW_EXPERIMENTAL_API)
target_compile_options(bf_tester PRIVATE -Wall -Wextra)
