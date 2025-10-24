cmake_minimum_required(VERSION 3.10)
project(server)


# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 添加可执行文件
add_executable(server main.cpp util.cpp pthread.cpp epoll.cpp)

# 添加 pthread 选项
find_package(Threads REQUIRED)
target_link_libraries(server PRIVATE Threads::Threads)


# -----------------------------
# MySQL Connector/C++ 配置
# -----------------------------

# 1. 指定 MySQL Connector/C++ 的头文件路径
# 假设 Linux 默认安装路径是 /usr/include/cppconn
target_include_directories(server PRIVATE /usr/include/cppconn)

# 2. 链接 MySQL Connector/C++ 库
# Linux 下通常是 mysqlcppconn
target_link_libraries(server PRIVATE mysqlcppconn)

# 添加编译选项
# target_compile_options(server PRIVATE -Wall -O2)

