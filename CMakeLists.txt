cmake_minimum_required(VERSION 3.16)
project(Project)

find_package(MKL CONFIG REQUIRED PATHS $ENV{MKLROOT})

add_executable(solve_matrix_market solve_matrix_market.cpp)
target_link_libraries(solve_matrix_market PUBLIC MKL::MKL)
