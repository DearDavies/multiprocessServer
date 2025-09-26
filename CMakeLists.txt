cmake_minimum_required(VERSION 3.31)
project(multiprocessServer C)

set(CMAKE_C_STANDARD 11)

# 自动查找 client 文件夹下所有的 .cpp 文件
file(GLOB CLIENT_SOURCES "client/*.c")
# 自动查找 server 文件夹下所有的 .cpp 文件
file(GLOB SERVER_SOURCES "server/*.c")

# 使用这些源文件创建可执行文件
add_executable(client_app ${CLIENT_SOURCES})
add_executable(server_app ${SERVER_SOURCES})

# add_executable(client client/client.c)


