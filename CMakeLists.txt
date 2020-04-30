cmake_minimum_required(VERSION 3.16)
project(fsm1)

set(CMAKE_CXX_STANDARD 17)
SET (CMAKE_CXX_COMPILER             "/usr/bin/clang++")
SET (CMAKE_CXX_FLAGS                "-Wall")
SET (CMAKE_CXX_FLAGS_DEBUG          "-g")
SET (CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
SET (CMAKE_CXX_FLAGS_RELEASE        "-O4 -DNDEBUG")
SET (CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

include_directories(. /home/rfriedma/try/sml/include/boost)


add_executable(fsm1
        inplace.h
        /home/rfriedma/try/sml/include/boost/sml.hpp
        sml.cc scrubber.h fsm_impl.h scrub_fsm.h scrub_fsm.cpp globals.h plant_uml.h)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(fmt REQUIRED)

target_link_libraries(fsm1 Threads::Threads fmt::fmt)

target_compile_options(fsm1 PUBLIC "-pthread")

