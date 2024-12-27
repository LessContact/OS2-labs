cmake_minimum_required(VERSION 3.29)
project(Http_proxy C)

set(CMAKE_C_STANDARD 23)

find_package(CURL REQUIRED)

add_executable(http_proxy main.c proxy/proxy.c threading/threadpool.c cache/linkedlist.c
                proxy/proxy.h threading/threadpool.h cache/linkedlist.h)

target_link_libraries(http_proxy curl)


