#!/bin/bash

# HFT System Test Script
# 高频交易系统测试脚本

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Project configuration
PROJECT_NAME="HFT_System"
BUILD_DIR="build"
SERVER_BINARY="hft_server"
CLIENT_BINARY="latency_test_client"
SERVER_PORT=8888
TEST_DURATION=5

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TOTAL_TESTS=0

# Function to print colored output
print_status() {
    local status=$1
    local message=$2
    case $status in
        "INFO")  echo -e "${BLUE}[INFO]${NC} $message" ;;
        "SUCCESS") echo -e "${GREEN}[SUCCESS]${NC} $message" ;;
        "WARNING") echo -e "${YELLOW}[WARNING]${NC} $message" ;;
        "ERROR") echo -e "${RED}[ERROR]${NC} $message" ;;
        "TEST") echo -e "${PURPLE}[TEST]${NC} $message" ;;
        "HEADER") echo -e "${CYAN}=== $message ===${NC}" ;;
    esac
}

# Function to run a test
run_test() {
    local test_name="$1"
    local test_command="$2"
    local expected_exit_code=${3:-0}
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    print_status "TEST" "Running: $test_name"
    
    if eval "$test_command" > /dev/null 2>&1; then
        local exit_code=$?
        if [ $exit_code -eq $expected_exit_code ]; then
            print_status "SUCCESS" "$test_name - PASSED"
            TESTS_PASSED=$((TESTS_PASSED + 1))
            return 0
        else
            print_status "ERROR" "$test_name - FAILED (exit code: $exit_code, expected: $expected_exit_code)"
            TESTS_FAILED=$((TESTS_FAILED + 1))
            return 1
        fi
    else
        print_status "ERROR" "$test_name - FAILED (command execution failed)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Function to cleanup processes
cleanup() {
    print_status "INFO" "Cleaning up processes..."
    pkill -f "$SERVER_BINARY" 2>/dev/null || true
    pkill -f "$CLIENT_BINARY" 2>/dev/null || true
    sleep 1
}

# Function to check if port is available
check_port() {
    local port=$1
    if netstat -tlnp 2>/dev/null | grep -q ":$port "; then
        return 1  # Port is in use
    else
        return 0  # Port is available
    fi
}

# Function to wait for server to start
wait_for_server() {
    local max_attempts=30
    local attempt=0
    
    while [ $attempt -lt $max_attempts ]; do
        if check_port $SERVER_PORT; then
            sleep 0.1
            attempt=$((attempt + 1))
        else
            print_status "SUCCESS" "Server is listening on port $SERVER_PORT"
            return 0
        fi
    done
    
    print_status "ERROR" "Server failed to start within timeout"
    return 1
}

# Main test function
main() {
    print_status "HEADER" "$PROJECT_NAME Test Suite"
    echo "Date: $(date)"
    echo "Host: $(hostname)"
    echo "User: $(whoami)"
    echo ""
    
    # Cleanup any existing processes
    cleanup
    
    # Test 1: Build System
    print_status "HEADER" "Build System Tests"
    
    if [ ! -d "$BUILD_DIR" ]; then
        print_status "INFO" "Creating build directory..."
        mkdir -p "$BUILD_DIR"
    fi
    
    cd "$BUILD_DIR"
    
    print_status "INFO" "Configuring with CMake..."
    if ! cmake .. > cmake.log 2>&1; then
        print_status "ERROR" "CMake configuration failed"
        cat cmake.log
        exit 1
    fi
    
    print_status "INFO" "Building project..."
    if ! make -j$(nproc) > build.log 2>&1; then
        print_status "ERROR" "Build failed"
        cat build.log
        exit 1
    fi
    
    print_status "SUCCESS" "Build completed successfully"
    
    # Test 2: Binary Existence
    print_status "HEADER" "Binary Tests"
    
    run_test "Server binary exists" "[ -f $SERVER_BINARY ]"
    run_test "Client binary exists" "[ -f $CLIENT_BINARY ]"
    run_test "Server binary is executable" "[ -x $SERVER_BINARY ]"
    run_test "Client binary is executable" "[ -x $CLIENT_BINARY ]"
    
    # Test 3: Server Help
    print_status "HEADER" "Server Functionality Tests"
    
    run_test "Server help command" "./$SERVER_BINARY --help"
    
    # Test 4: Server Startup
    print_status "HEADER" "Server Startup Tests"
    
    print_status "INFO" "Starting server in background..."
    ./$SERVER_BINARY > server.log 2>&1 &
    SERVER_PID=$!
    
    # Wait for server to start
    if wait_for_server; then
        run_test "Server process is running" "kill -0 $SERVER_PID"
        
        # Test 5: Client Connection Tests
        print_status "HEADER" "Client Connection Tests"
        
        # Test basic connection
        print_status "INFO" "Testing client connection..."
        timeout 3s ./$CLIENT_BINARY --test latency --messages 10 --interval 100 > client.log 2>&1 || true
        
        if grep -q "Connected to HFT server" client.log; then
            print_status "SUCCESS" "Client connection test - PASSED"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            print_status "ERROR" "Client connection test - FAILED"
            TESTS_FAILED=$((TESTS_FAILED + 1))
            print_status "INFO" "Client log:"
            cat client.log
        fi
        
        # Test 6: Latency Tests
        print_status "HEADER" "Latency Performance Tests"
        
        print_status "INFO" "Running latency test (50 messages)..."
        timeout 10s ./$CLIENT_BINARY --test latency --messages 50 --interval 0 > latency_test.log 2>&1 || true
        
        if grep -q "Latency Test Results" latency_test.log; then
            print_status "SUCCESS" "Latency test completed"
            TESTS_PASSED=$((TESTS_PASSED + 1))
            
            # Extract and display latency results
            print_status "INFO" "Latency Test Results:"
            grep -A 20 "Latency Test Results" latency_test.log | head -15
        else
            print_status "ERROR" "Latency test failed"
            TESTS_FAILED=$((TESTS_FAILED + 1))
            print_status "INFO" "Latency test log:"
            cat latency_test.log
        fi
        
        # Test 7: Burst Test
        print_status "INFO" "Running burst test..."
        timeout 10s ./$CLIENT_BINARY --test burst --burst-size 10 --bursts 5 --burst-interval 100 > burst_test.log 2>&1 || true
        
        if grep -q "Burst test completed" burst_test.log; then
            print_status "SUCCESS" "Burst test completed"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            print_status "ERROR" "Burst test failed"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
        
        # Test 8: Server Statistics
        print_status "HEADER" "Server Performance Tests"
        
        print_status "INFO" "Checking server statistics..."
        sleep 2
        
        if grep -q "Total Messages:" server.log; then
            print_status "SUCCESS" "Server statistics are being generated"
            TESTS_PASSED=$((TESTS_PASSED + 1))
            
            # Show recent server statistics
            print_status "INFO" "Recent server statistics:"
            tail -10 server.log | grep -E "(Total Messages|Active Connections|Latency target)" || true
        else
            print_status "ERROR" "Server statistics not found"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
        
    else
        print_status "ERROR" "Server failed to start"
        print_status "INFO" "Server log:"
        cat server.log
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    
    # Cleanup
    cleanup
    
    # Test 9: Error Handling
    print_status "HEADER" "Error Handling Tests"
    
    print_status "INFO" "Testing client with no server..."
    timeout 3s ./$CLIENT_BINARY --test latency --messages 5 --interval 100 > error_test.log 2>&1 || true
    
    if grep -q "Connection failed\|Failed to connect" error_test.log; then
        print_status "SUCCESS" "Client properly handles connection failure"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        print_status "ERROR" "Client error handling test failed"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    
    # Final Results
    print_status "HEADER" "Test Results Summary"
    echo "Total Tests: $TOTAL_TESTS"
    echo "Passed: $TESTS_PASSED"
    echo "Failed: $TESTS_FAILED"
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ]; then
        print_status "SUCCESS" "All tests passed! 🎉"
        echo ""
        print_status "INFO" "Build artifacts:"
        echo "  - Server: $BUILD_DIR/$SERVER_BINARY"
        echo "  - Client: $BUILD_DIR/$CLIENT_BINARY"
        echo ""
        print_status "INFO" "Usage examples:"
        echo "  Server: ./$BUILD_DIR/$SERVER_BINARY --help"
        echo "  Client: ./$BUILD_DIR/$CLIENT_BINARY --help"
        echo "  Latency test: ./$BUILD_DIR/$CLIENT_BINARY --test latency --messages 100"
        echo "  Burst test: ./$BUILD_DIR/$CLIENT_BINARY --test burst --burst-size 50 --bursts 10"
        exit 0
    else
        print_status "ERROR" "$TESTS_FAILED test(s) failed"
        echo ""
        print_status "INFO" "Log files for debugging:"
        echo "  - CMake: $BUILD_DIR/cmake.log"
        echo "  - Build: $BUILD_DIR/build.log"
        echo "  - Server: $BUILD_DIR/server.log"
        echo "  - Client: $BUILD_DIR/client.log"
        echo "  - Latency: $BUILD_DIR/latency_test.log"
        echo "  - Burst: $BUILD_DIR/burst_test.log"
        echo "  - Error: $BUILD_DIR/error_test.log"
        exit 1
    fi
}

# Trap to ensure cleanup on exit
trap cleanup EXIT

# Run main function
main "$@"
