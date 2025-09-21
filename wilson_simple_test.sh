#!/bin/bash

# Wilson Simple HFT Test
# 威尔逊简单高频交易测试

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== Wilson Simple HFT Test ===${NC}"
echo "Date: $(date)"
echo ""

# Cleanup
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    pkill -f hft_server 2>/dev/null || true
    pkill -f simple_latency_test 2>/dev/null || true
    sleep 1
}

trap cleanup EXIT

# Build
echo -e "${YELLOW}Building components...${NC}"
mkdir -p build
cd build

g++ -std=c++17 -O3 -Wall -Wextra -I../inc -o hft_server ../src/main.cpp ../src/hft_server.cpp -lpthread
g++ -std=c++17 -O3 -Wall -Wextra -I../inc -o simple_latency_test ../src/simple_latency_test.cpp -lpthread

echo -e "${GREEN}✓ Build completed${NC}"

# Start server
echo -e "${YELLOW}Starting server...${NC}"
pkill -f hft_server 2>/dev/null || true
sleep 1

./hft_server > server.log 2>&1 &
SERVER_PID=$!
sleep 2

if kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${GREEN}✓ Server started (PID: $SERVER_PID)${NC}"
else
    echo -e "${RED}✗ Server failed to start${NC}"
    cat server.log
    exit 1
fi

# Run test
echo -e "${YELLOW}Running latency test (100 messages)...${NC}"
timeout 30s ./simple_latency_test 100 > test.log 2>&1
TEST_RESULT=$?

if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}✓ Test completed successfully${NC}"
    echo -e "\n${BLUE}=== Test Results ===${NC}"
    if grep -q "LATENCY STATISTICS" test.log; then
        grep -A 10 "LATENCY STATISTICS" test.log
    else
        echo "Test completed - check test.log for details"
    fi
else
    echo -e "${RED}✗ Test failed or timed out${NC}"
    echo "Last 10 lines of test.log:"
    tail -10 test.log
fi

# Show server stats
echo -e "\n${YELLOW}=== Server Statistics ===${NC}"
tail -5 server.log | grep -E "(Total Messages|Active Connections)" || echo "No statistics available"

# Final summary
echo -e "\n${BLUE}=== Summary ===${NC}"
echo "✓ Build: SUCCESS"
echo "✓ Server: SUCCESS"
echo "✓ Test: $([ $TEST_RESULT -eq 0 ] && echo "SUCCESS" || echo "FAILED")"
echo "✓ Files: $(ls -1 hft_server simple_latency_test 2>/dev/null | wc -l) binaries created"

echo -e "\n${GREEN}Wilson Simple HFT Test completed!${NC}"

# Cleanup
kill $SERVER_PID 2>/dev/null || true
