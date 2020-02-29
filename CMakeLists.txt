cmake_minimum_required(VERSION 3.12)
project(OS-Project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(disk disk.cpp file.cpp dev_io.cpp)
add_executable(format format.cpp file.cpp dev_io.cpp)

set(LIBS "/usr/lib/libfuse.so")
set(INC "/usr/include")
target_link_libraries(dokan_disk PUBLIC ${LIBS})
target_include_directories(dokan_disk PUBLIC ${INC})