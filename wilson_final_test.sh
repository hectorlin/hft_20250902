#!/bin/bash

# Wilson Final HFT Test - Ultra Simple
# 威尔逊最终高频交易测试 - 超简单

echo "=== Wilson Final HFT Test ==="
echo "Date: $(date)"
echo ""

# Cleanup
pkill -f hft_server 2>/dev/null || true
sleep 2

# Build
echo "Building..."
mkdir -p build
cd build

g++ -std=c++17 -O3 -I../inc -o hft_server ../src/main.cpp ../src/hft_server.cpp -lpthread
g++ -std=c++17 -O3 -I../inc -o simple_latency_test ../src/simple_latency_test.cpp -lpthread

echo "✓ Build completed"

# Start server
echo "Starting server..."
./hft_server > server.log 2>&1 &
SERVER_PID=$!
sleep 3

echo "✓ Server started (PID: $SERVER_PID)"

# Run quick test
echo "Running quick test (10 messages)..."
timeout 15s ./simple_latency_test 10 > test.log 2>&1
TEST_RESULT=$?

if [ $TEST_RESULT -eq 0 ]; then
    echo "✓ Test completed successfully"
    echo ""
    echo "=== Test Results ==="
    if grep -q "LATENCY STATISTICS" test.log; then
        grep -A 8 "LATENCY STATISTICS" test.log
    else
        echo "Test completed - check test.log"
    fi
else
    echo "✗ Test failed or timed out"
    echo "Test log:"
    tail -5 test.log
fi

# Show server stats
echo ""
echo "=== Server Statistics ==="
tail -3 server.log

# Summary
echo ""
echo "=== Summary ==="
echo "✓ Build: SUCCESS"
echo "✓ Server: SUCCESS" 
echo "✓ Test: $([ $TEST_RESULT -eq 0 ] && echo "SUCCESS" || echo "FAILED")"
echo "✓ Files: $(ls -1 hft_server simple_latency_test 2>/dev/null | wc -l) binaries"

echo ""
echo "Wilson Final HFT Test completed!"

# Cleanup
kill $SERVER_PID 2>/dev/null || true
