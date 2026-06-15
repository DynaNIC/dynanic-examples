set (SRCS
    ../../common/common_stats.c
    http.c
    auxiliary.c
    rules.c
)

add_executable(http
    ${SRCS}
)
