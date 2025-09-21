#!/bin/bash

# Quick HFT Latency Test
# 快速高频交易延迟测试

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== Quick HFT Latency Test ===${NC}"
echo "Date: $(date)"
echo ""

# Default message count
MESSAGE_COUNT=${1:-10000}

echo -e "${YELLOW}Testing with $MESSAGE_COUNT messages${NC}"
echo ""

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    pkill -f hft_server 2>/dev/null || true
    pkill -f simple_latency_test 2>/dev/null || true
    sleep 1
}

trap cleanup EXIT

# Start server
echo -e "${YELLOW}Starting HFT server...${NC}"
cd build
./hft_server > server.log 2>&1 &
SERVER_PID=$!
sleep 2

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}✗ Server failed to start${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Server started (PID: $SERVER_PID)${NC}"

# Run latency test
echo -e "\n${YELLOW}Running latency test with $MESSAGE_COUNT messages...${NC}"
echo "This may take a few moments..."
echo ""

timeout 120s ./simple_latency_test $MESSAGE_COUNT

TEST_RESULT=$?

# Stop server
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

if [ $TEST_RESULT -eq 0 ]; then
    echo -e "\n${GREEN}✓ Latency test completed successfully!${NC}"
else
    echo -e "\n${RED}✗ Latency test failed${NC}"
fi

cd ..

echo -e "\n${BLUE}Usage examples:${NC}"
echo "  ./quick_latency_test.sh 1000    # Test with 1K messages"
echo "  ./quick_latency_test.sh 10000   # Test with 10K messages (default)"
echo "  ./quick_latency_test.sh 100000  # Test with 100K messages"
