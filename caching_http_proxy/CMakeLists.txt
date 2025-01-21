cmake_minimum_required(VERSION 3.29)
project(Http_proxy C)

set(CMAKE_C_STANDARD 23)

add_library(logc STATIC third_party/log.c third_party/log.h)
target_compile_options(logc PRIVATE -DLOG_USE_COLOR)
add_library(parser STATIC third_party/picohttpparser.h third_party/picohttpparser.c)

add_executable(http_proxy main.c proxy/proxy.c threading/threadpool.c caching/httpcache.c
                proxy/proxy.h threading/threadpool.h caching/httpcache.h)

# for debugging
target_compile_options(http_proxy PRIVATE -Og -O0 -fsanitize=address -fsanitize=leak -fsanitize=signed-integer-overflow -fsanitize=bounds-strict)
# target_compile_options(http_proxy PRIVATE -fsanitize=thread -fno-omit-frame-pointer)

target_link_libraries(http_proxy asan ubsan logc parser)
#target_link_libraries(http_proxy logc parser tsan)
#target_link_libraries(http_proxy logc parser)


