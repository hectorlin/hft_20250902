#!/bin/bash

# HFT Simple Latency Test Script
# 高频交易简单延迟测试脚本

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${BLUE}=== HFT Simple Latency Test Script ===${NC}"
echo "Date: $(date)"
echo ""

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up processes...${NC}"
    pkill -f hft_server 2>/dev/null || true
    pkill -f simple_latency_test 2>/dev/null || true
    sleep 1
}

# Trap to ensure cleanup on exit
trap cleanup EXIT

# Test function
run_latency_test() {
    local test_name="$1"
    local message_count="$2"
    local description="$3"
    
    echo -e "\n${PURPLE}=== $test_name ===${NC}"
    echo -e "${CYAN}$description${NC}"
    echo -e "${YELLOW}Messages: $message_count${NC}"
    echo "----------------------------------------"
    
    # Start server
    echo -e "${YELLOW}Starting HFT server...${NC}"
    cd build
    ./hft_server > server.log 2>&1 &
    SERVER_PID=$!
    sleep 2
    
    # Check if server started
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "${RED}✗ Server failed to start${NC}"
        return 1
    fi
    
    echo -e "${GREEN}✓ Server started (PID: $SERVER_PID)${NC}"
    
    # Run latency test
    echo -e "${YELLOW}Running latency test...${NC}"
    timeout 60s ./simple_latency_test $message_count
    
    local test_result=$?
    
    # Stop server
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
    
    if [ $test_result -eq 0 ]; then
        echo -e "${GREEN}✓ $test_name completed successfully${NC}"
    else
        echo -e "${RED}✗ $test_name failed${NC}"
    fi
    
    cd ..
    return $test_result
}

# Main test execution
main() {
    echo -e "${BLUE}Starting HFT Latency Test Suite${NC}"
    echo ""
    
    # Test 1: Quick test (1K messages)
    run_latency_test "Quick Test" "1000" "Fast latency measurement with 1,000 messages"
    
    # Test 2: Medium test (10K messages)
    run_latency_test "Medium Test" "10000" "Medium latency measurement with 10,000 messages"
    
    # Test 3: Large test (50K messages)
    run_latency_test "Large Test" "50000" "Large latency measurement with 50,000 messages"
    
    # Test 4: Ultra test (100K messages)
    run_latency_test "Ultra Test" "100000" "Ultra latency measurement with 100,000 messages"
    
    echo -e "\n${GREEN}=== All Latency Tests Completed ===${NC}"
    echo -e "${CYAN}Test Summary:${NC}"
    echo "✓ Quick Test (1K messages)"
    echo "✓ Medium Test (10K messages)" 
    echo "✓ Large Test (50K messages)"
    echo "✓ Ultra Test (100K messages)"
    echo ""
    echo -e "${GREEN}All tests completed successfully!${NC}"
}

# Check if we're in the right directory
if [ ! -f "build/simple_latency_test" ]; then
    echo -e "${RED}Error: simple_latency_test not found in build directory${NC}"
    echo "Please run this script from the project root directory"
    exit 1
fi

# Check if server binary exists
if [ ! -f "build/hft_server" ]; then
    echo -e "${RED}Error: hft_server not found in build directory${NC}"
    echo "Please build the project first"
    exit 1
fi

# Run main function
main "$@"
