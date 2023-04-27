cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


project(rtp_proxy VERSION 1.0)

# add multithreading
find_package(Threads)
# add source files (.cpp)
file(GLOB_RECURSE SRC_FILES src/*.cpp)
add_executable(
  main ${SRC_FILES}
)

# include header files (.h):
target_include_directories(main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(BEFORE SYSTEM "/usr/local/include")
 
target_link_libraries(main Threads::Threads)

set_target_properties(main PROPERTIES OUTPUT_NAME "rtp_proxy")