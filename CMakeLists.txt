cmake_minimum_required(VERSION 4.1)
project(Project C)

set(CMAKE_C_STANDARD 11)

add_executable(Project main.c
        graph.h
        graph.c
        Dijkstra.c
        gui.h
        gui.c
        tester.c
        )

