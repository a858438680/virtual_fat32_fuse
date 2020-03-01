cmake_minimum_required(VERSION 3.12)
project(OS-Project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-D_FILE_OFFSET_BITS=64")

add_executable(disk disk.cpp log.cpp u16str.cpp file.cpp dev_io.cpp)
add_executable(format format.cpp log.cpp u16str.cpp file.cpp dev_io.cpp)

set(LIBS "/usr/lib/libfuse.so")
set(INC "/usr/include")
target_link_libraries(disk PUBLIC ${LIBS})
target_include_directories(disk PUBLIC ${INC})