cmake_minimum_required(VERSION 3.16)
project(Ticket_System)

set(CMAKE_CXX_STANDARD 14)

include_directories(src)

add_executable(Ticket_System
        src/main.cpp
        src/system.cpp
        src/system.h
        src/train.cpp
        src/train.h
        src/user.cpp
        src/user.h
        src/utils.cpp
        src/utils.h)
