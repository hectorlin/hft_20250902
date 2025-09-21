#!/bin/bash

# Wilson HFT Test Script - Simple and Reliable
# 威尔逊高频交易测试脚本 - 简单可靠

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== Wilson HFT Test Script ===${NC}"
echo "Date: $(date)"
echo "Author: Wilson"
echo "Purpose: Simple and reliable HFT testing"
echo ""

# Configuration
BUILD_DIR="build"
TEST_MESSAGES=1000

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    pkill -f hft_server 2>/dev/null || true
    pkill -f simple_latency_test 2>/dev/null || true
    sleep 1
}

trap cleanup EXIT

# Function to print results
print_result() {
    local test="$1"
    local status="$2"
    if [ "$status" = "PASS" ]; then
        echo -e "${GREEN}✓ $test: PASSED${NC}"
    else
        echo -e "${RED}✗ $test: FAILED${NC}"
    fi
}

# Step 1: Build components
echo -e "${YELLOW}=== Step 1: Building Components ===${NC}"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Build HFT Server
echo "Building HFT Server..."
g++ -std=c++17 -O3 -Wall -Wextra -I../inc -o hft_server ../src/main.cpp ../src/hft_server.cpp -lpthread
if [ -f "hft_server" ]; then
    print_result "HFT Server Build" "PASS"
else
    print_result "HFT Server Build" "FAIL"
    exit 1
fi

# Build Simple Latency Test
echo "Building Simple Latency Test..."
g++ -std=c++17 -O3 -Wall -Wextra -I../inc -o simple_latency_test ../src/simple_latency_test.cpp -lpthread
if [ -f "simple_latency_test" ]; then
    print_result "Simple Latency Test Build" "PASS"
else
    print_result "Simple Latency Test Build" "FAIL"
    exit 1
fi

echo -e "${GREEN}All components built successfully!${NC}"

# Step 2: Test server startup
echo -e "\n${YELLOW}=== Step 2: Testing Server ===${NC}"

# Kill any existing server
pkill -f hft_server 2>/dev/null || true
sleep 2

# Start server
echo "Starting HFT Server..."
./hft_server > server.log 2>&1 &
SERVER_PID=$!
sleep 3

# Check if server is running
if kill -0 $SERVER_PID 2>/dev/null; then
    print_result "Server Startup" "PASS"
    echo "Server PID: $SERVER_PID"
else
    print_result "Server Startup" "FAIL"
    echo "Server log:"
    cat server.log
    exit 1
fi

# Step 3: Run latency test
echo -e "\n${YELLOW}=== Step 3: Running Latency Test ===${NC}"
echo "Testing with $TEST_MESSAGES messages..."

# Run the test
timeout 60s ./simple_latency_test $TEST_MESSAGES > test.log 2>&1
TEST_RESULT=$?

if [ $TEST_RESULT -eq 0 ]; then
    print_result "Latency Test" "PASS"
    echo -e "\n${CYAN}=== Test Results ===${NC}"
    if grep -q "LATENCY STATISTICS" test.log; then
        grep -A 15 "LATENCY STATISTICS" test.log
    else
        echo "Test completed successfully"
        tail -10 test.log
    fi
else
    print_result "Latency Test" "FAIL"
    echo "Test log:"
    tail -10 test.log
fi

# Step 4: Show server statistics
echo -e "\n${YELLOW}=== Step 4: Server Statistics ===${NC}"
if [ -f "server.log" ]; then
    echo "Server Statistics:"
    tail -5 server.log | grep -E "(Total Messages|Active Connections|Latency target)" || echo "No statistics available"
fi

# Step 5: Final summary
echo -e "\n${BLUE}=== Final Summary ===${NC}"
echo "Date: $(date)"
echo "Test Messages: $TEST_MESSAGES"
echo "Server PID: $SERVER_PID"
echo "Build Directory: $BUILD_DIR"
echo ""
echo "Files created:"
ls -la hft_server simple_latency_test 2>/dev/null | while read line; do
    echo "  $line"
done

echo ""
echo "Log files:"
ls -la *.log 2>/dev/null | while read line; do
    echo "  $line"
done

echo -e "\n${GREEN}=== Wilson HFT Test Completed ===${NC}"

# Cleanup
kill $SERVER_PID 2>/dev/null || true
