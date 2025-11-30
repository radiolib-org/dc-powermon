cmake_minimum_required(VERSION 3.18)

project(ina219)

add_library(ina219 ina219.c)
target_include_directories(ina219
  PUBLIC "."
)
