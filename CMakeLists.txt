cmake_minimum_required(VERSION 3.29)
project(OS2_labs C)

set(CMAKE_C_STANDARD 23)
add_compile_options(${CMAKE_C_FLAGS})
#was doing some testing
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

add_executable(1.4.a 1/1.4/a.c)
add_executable(1.4.b 1/1.4/b.c)
add_executable(1.4.c 1/1.4/c.c)

add_executable(1.5.a 1/1.5/a.c)


add_library(mythreadlib STATIC 1/1.6/mythread_create.c 1/1.6/mythread_create.h)
add_executable(mythread 1/1.6/main.c)
target_link_libraries(mythread mythreadlib)

#==========================================================================================================

add_executable(q-example 2/2.1/queue-example.c 2/2.1/queue.c)
add_executable(q-th 2/2.1/queue-threads.c 2/2.1/queue.c)
add_executable(q-hz 2/2.1/queue2.c)

add_executable(q-spin 2/2.2/spinlock/queue.c 2/2.2/spinlock/queue-threads.c)
add_executable(q-mutex 2/2.2/mutex/queue-threads.c 2/2.2/mutex/queue.c)
add_executable(q-condvar 2/2.2/condvar/queue-threads.c 2/2.2/condvar/queue.c)
add_executable(q-sem 2/2.2/semaphor/queue.c 2/2.2/semaphor/queue-threads.c)

add_executable(list-mutex 2/2.3/mutex/storage.c 2/2.3/mutex/main.c)



