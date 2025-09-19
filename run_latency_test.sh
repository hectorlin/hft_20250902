#!/bin/bash

# HFT Latency Test Script
# 高频交易延迟测试脚本

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== HFT Latency Test ===" << std::endl;

# Kill any existing server processes
echo -e "${YELLOW}Cleaning up existing processes...${NC}"
pkill -f hft_server 2>/dev/null || true
sleep 1

# Start server in background
echo -e "${YELLOW}Starting HFT server...${NC}"
cd build
./hft_server > server.log 2>&1 &
SERVER_PID=$!

# Wait for server to start
echo -e "${YELLOW}Waiting for server to start...${NC}"
sleep 3

# Check if server is running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}Server failed to start${NC}"
    cat server.log
    exit 1
fi

echo -e "${GREEN}Server started successfully (PID: $SERVER_PID)${NC}"

# Run latency tests
echo -e "\n${BLUE}=== Running Latency Tests ===${NC}"

# Test 1: Basic latency test
echo -e "\n${YELLOW}Test 1: Basic Latency Test (50 messages)${NC}"
./latency_test_client --test latency --messages 50 --interval 0

# Test 2: Burst test
echo -e "\n${YELLOW}Test 2: Burst Test (10 bursts of 20 messages)${NC}"
./latency_test_client --test burst --burst-size 20 --bursts 10 --burst-interval 50

# Test 3: High frequency test
echo -e "\n${YELLOW}Test 3: High Frequency Test (200 messages, no interval)${NC}"
./latency_test_client --test latency --messages 200 --interval 0

# Test 4: Sustained load test (short duration)
echo -e "\n${YELLOW}Test 4: Sustained Load Test (10 seconds, 100 msg/s)${NC}"
./latency_test_client --test sustained --duration 10 --rate 100

echo -e "\n${GREEN}=== All Tests Completed ===${NC}"

# Stop server
echo -e "${YELLOW}Stopping server...${NC}"
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

echo -e "${GREEN}Latency testing completed successfully!${NC}"

