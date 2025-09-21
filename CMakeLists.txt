cmake_minimum_required(VERSION 3.10)
project(HFT_Server_Cpp17)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set compiler flags for high performance
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -mtune=native")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-loops -ffast-math")

# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

# Source files
set(SOURCES
    src/main.cpp
    src/hft_server.cpp
)

# Create HFT Server executable
add_executable(hft_server ${SOURCES})

# Link libraries
target_link_libraries(hft_server
    Threads::Threads
)

# Create Latency Test Client executable
add_executable(latency_test_client
    src/latency_test_main.cpp
    src/latency_client.cpp
)

# Link libraries for latency client
target_link_libraries(latency_test_client
    Threads::Threads
)

# Set target properties
set_target_properties(hft_server PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

set_target_properties(latency_test_client PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Installation rules
install(TARGETS hft_server latency_test_client
    RUNTIME DESTINATION bin
)

# Print build information
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

# Add compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Create HFT TCP Client executable
add_executable(hft_tcp_client
    src/hft_tcp_client_test.cpp
    src/hft_tcp_client.cpp
)

# Link libraries for HFT TCP client
target_link_libraries(hft_tcp_client
    Threads::Threads
)

# Set target properties for HFT TCP client
set_target_properties(hft_tcp_client PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Installation rules for HFT TCP client
install(TARGETS hft_tcp_client
    RUNTIME DESTINATION bin
)
