cmake_minimum_required(VERSION 3.16)
project(Ticket_System)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_executable(code
        main.cpp
        include/train.h
        include/user.h
        include/utils.h
        include/bpt.h
        include/vector.h
        include/map.h
        include/order.h
        include/memory_river.h
        include/utility.h
        include/exceptions.h)
