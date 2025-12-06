cmake_minimum_required(VERSION 3.18)

project(dc-powermon-client)

add_library(dc-powermon-client dc_powermon_client.c)
target_include_directories(dc-powermon-client
  PUBLIC "."
)
