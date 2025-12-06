cmake_minimum_required(VERSION 3.18)

project(socket)

add_library(socket socket.c)
target_include_directories(socket
  PUBLIC "."
)
