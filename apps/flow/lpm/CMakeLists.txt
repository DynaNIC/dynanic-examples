set (SRCS
    ../../common/common_stats.c
    lpm.c
    auxiliary.c
    rules.c
)

add_executable(lpm
    ${SRCS}
)
