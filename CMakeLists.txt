cmake_minimum_required(VERSION 3.29)
project(OS2_labs C)

set(CMAKE_C_STANDARD 23)
add_compile_options(${CMAKE_C_FLAGS})
add_executable(test 1/1.2/test.c)

add_executable(1.1.a 1/1.1/a.c)
add_executable(1.1.b 1/1.1/b.c)
add_executable(1.1.c 1/1.1/c.c)
add_executable(1.1.d 1/1.1/d.c)

add_executable(1.2.b 1/1.2/b.c)
add_executable(1.2.c 1/1.2/c.c)
add_executable(1.2.d 1/1.2/d.c)
add_executable(1.2.e 1/1.2/e.c)
add_executable(1.2.f 1/1.2/f.c)

add_executable(1.3.a 1/1.3/a.c)
add_executable(1.3.b 1/1.3/b.c)




