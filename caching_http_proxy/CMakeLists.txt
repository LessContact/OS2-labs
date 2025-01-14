cmake_minimum_required(VERSION 3.29)
project(Http_proxy C)

set(CMAKE_C_STANDARD 23)

find_package(CURL REQUIRED)

add_library(logc STATIC third_party/log.c third_party/log.h)
target_compile_options(logc PRIVATE -DLOG_USE_COLOR)


add_executable(http_proxy main.c proxy/proxy.c threading/threadpool.c caching/linkedlist.c
                proxy/proxy.h threading/threadpool.h caching/linkedlist.h)

target_link_libraries(http_proxy curl logc)


