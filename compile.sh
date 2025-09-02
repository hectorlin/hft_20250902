#!/bin/bash

# HFT Server Compilation Script
# 高频交易服务器编译脚本

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project configuration
PROJECT_NAME="hft_server"
SRC_DIR="src"
INC_DIR="inc"
BIN_DIR="bin"
LIB_DIR="lib"

# Compiler settings
CXX="g++"
CXXFLAGS="-std=c++17 -O3 -Wall -Wextra -Wpedantic -march=native -mtune=native"
INCLUDES="-I${INC_DIR}"
LIBS="-lpthread"

# Debug/Release mode
MODE="release"
if [[ "$1" == "debug" ]]; then
    MODE="debug"
    CXXFLAGS="-std=c++17 -g -O0 -Wall -Wextra -Wpedantic -DDEBUG"
    echo -e "${YELLOW}Building in DEBUG mode${NC}"
else
    echo -e "${GREEN}Building in RELEASE mode${NC}"
fi

# Create directories if they don't exist
mkdir -p "${BIN_DIR}"
mkdir -p "${LIB_DIR}"

echo -e "${BLUE}=== HFT Server Compilation ===${NC}"
echo -e "Project: ${PROJECT_NAME}"
echo -e "Mode: ${MODE}"
echo -e "Compiler: ${CXX}"
echo -e "Flags: ${CXXFLAGS}"
echo -e "================================"

# Source files
SOURCES=(
    "${SRC_DIR}/main.cpp"
    "${SRC_DIR}/hft_server.cpp"
)

# Object files
OBJECTS=()
for src in "${SOURCES[@]}"; do
    obj_name=$(basename "$src" .cpp)
    OBJECTS+=("${LIB_DIR}/${obj_name}.o")
done

echo -e "${YELLOW}Compiling source files...${NC}"

# Compile each source file
for i in "${!SOURCES[@]}"; do
    src="${SOURCES[$i]}"
    obj="${OBJECTS[$i]}"
    
    echo -e "  Compiling: ${src} -> ${obj}"
    
    if ! ${CXX} ${CXXFLAGS} ${INCLUDES} -c "${src}" -o "${obj}"; then
        echo -e "${RED}Error: Failed to compile ${src}${NC}"
        exit 1
    fi
done

echo -e "${GREEN}Source files compiled successfully${NC}"

# Link the executable
EXECUTABLE="${BIN_DIR}/${PROJECT_NAME}"
echo -e "${YELLOW}Linking executable: ${EXECUTABLE}${NC}"

if ! ${CXX} ${CXXFLAGS} "${OBJECTS[@]}" -o "${EXECUTABLE}" ${LIBS}; then
    echo -e "${RED}Error: Failed to link executable${NC}"
    exit 1
fi

# Make executable
chmod +x "${EXECUTABLE}"

echo -e "${GREEN}=== Compilation Successful ===${NC}"
echo -e "Executable: ${EXECUTABLE}"
echo -e "Size: $(du -h "${EXECUTABLE}" | cut -f1)"

# Display usage information
echo -e "\n${BLUE}Usage:${NC}"
echo -e "  ${EXECUTABLE} [options]"
echo -e "  ${EXECUTABLE} --help"

echo -e "\n${BLUE}Options:${NC}"
echo -e "  --ip <ip>        Server IP address (default: 127.0.0.1)"
echo -e "  --port <port>    Server port (default: 8888)"
echo -e "  --threads <n>    Number of worker threads (default: 4)"
echo -e "  --help           Show help message"

# Clean up object files if in release mode
if [[ "$MODE" == "release" ]]; then
    echo -e "\n${YELLOW}Cleaning up object files...${NC}"
    rm -f "${OBJECTS[@]}"
    echo -e "${GREEN}Cleanup complete${NC}"
fi

echo -e "\n${GREEN}Build completed successfully!${NC}"
