set (SRCS
    argparse.c
    flow_control.c
    common.c
    configure.c
    statistics.c
    flow_rules/part1.c
    flow_rules/part2.c
    flow_rules/part3.c
    flow.c
)

add_executable(basic
    ${SRCS}
)
