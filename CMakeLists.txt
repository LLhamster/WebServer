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

# 添加编译选项
# target_compile_options(server PRIVATE -Wall -O2)