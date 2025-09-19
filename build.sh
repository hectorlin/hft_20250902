#!/bin/bash

# HFT Server CMake Build Script
# 高频交易服务器CMake构建脚本

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project configuration
PROJECT_NAME="hft_server"
BUILD_DIR="build"

echo -e "${BLUE}=== HFT Server CMake Build ===${NC}"

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Creating build directory...${NC}"
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"
if ! cmake ..; then
    echo -e "${RED}Error: CMake configuration failed${NC}"
    exit 1
fi

# Build the project
echo -e "${YELLOW}Building project...${NC}"
if ! make -j$(nproc); then
    echo -e "${RED}Error: Build failed${NC}"
    exit 1
fi

echo -e "${GREEN}=== Build Successful ===${NC}"
echo -e "Executable: ${BUILD_DIR}/${PROJECT_NAME}"
echo -e "Size: $(du -h "${PROJECT_NAME}" | cut -f1)"

# Display usage information
echo -e "\n${BLUE}Usage:${NC}"
echo -e "  ./${PROJECT_NAME} [options]"
echo -e "  ./${PROJECT_NAME} --help"

echo -e "\n${BLUE}Options:${NC}"
echo -e "  --ip <ip>        Server IP address (default: 127.0.0.1)"
echo -e "  --port <port>    Server port (default: 8888)"
echo -e "  --threads <n>    Number of worker threads (default: 4)"
echo -e "  --help           Show help message"

echo -e "\n${GREEN}Build completed successfully!${NC}"
