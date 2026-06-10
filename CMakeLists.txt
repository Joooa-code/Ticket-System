cmake_minimum_required(VERSION 3.16)
project(Ticket_System)

set(CMAKE_CXX_STANDARD 14)

include_directories(src)

add_executable(Ticket_System
        src/main.cpp
        src/system.cpp
        include/system.h
        src/train.cpp
        include/train.h
        src/user.cpp
        include/user.h
        src/utils.cpp
        include/utils.h
        include/bpt.h
        include/vector.h
        include/map.h)
