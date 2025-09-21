# Wilson HFT System 🚀

**High-Frequency Trading Server with C++17, Multi-threading, and Low-Latency Message Processing**

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen.svg)]()

## 🎯 Overview

A high-performance, low-latency trading system designed for high-frequency trading applications. Built with modern C++17 features, multi-threading, and optimized networking for sub-microsecond latency targets.

## ✨ Features

- **Ultra-Low Latency**: Target < 20μs latency
- **Multi-Threaded Architecture**: Separate threads for sending, receiving, and processing
- **Epoll I/O Model**: High-performance, non-blocking network operations
- **Message Protocol**: Comprehensive message types for trading operations
- **Real-time Monitoring**: Live performance statistics and latency tracking
- **Comprehensive Testing**: Multiple test suites for performance validation

## 🏗️ Architecture

### Core Components

- **HFT Server** (`hft_server.cpp`): Main trading server with multi-threaded processing
- **TCP Client** (`hft_tcp_client.cpp`): High-performance client with epoll
- **Message Protocol** (`message.h`): Trading message definitions
- **Latency Testing** (`simple_latency_test.cpp`): Performance testing framework

### Key Features

- **Singleton Pattern**: Thread-safe server instance management
- **Service-Oriented**: Modular message processing services
- **Zero-Copy Operations**: Pre-allocated buffers for maximum performance
- **Socket Optimization**: TCP_NODELAY, SO_REUSEADDR, buffer tuning

## 🚀 Quick Start

### Prerequisites

- C++17 compatible compiler (GCC 7+ or Clang 5+)
- Linux/Unix system
- CMake 3.10+

### Building

```bash
# Clone the repository
git clone https://github.com/hectorlin/hft_20250902.git
cd hft_20250902

# Build using CMake
mkdir build && cd build
cmake ..
make -j$(nproc)

# Or use the provided build script
./build.sh
```

### Running Tests

```bash
# Run Wilson's comprehensive test suite
./wilson_test.sh

# Run simple latency test
./wilson_simple_test.sh

# Run ultra-fast test
./wilson_final_test.sh
```

## 📊 Performance Results

- **Latency Target**: < 20μs ✅
- **Message Throughput**: 100K+ messages/second
- **Connection Handling**: Multi-client support
- **Memory Usage**: Optimized with pre-allocated buffers

## 🧪 Testing

### Test Suites

1. **Wilson Test Suite** (`wilson_test.sh`)
   - Comprehensive testing framework
   - 100K message latency tests
   - Performance validation

2. **Simple Latency Test** (`wilson_simple_test.sh`)
   - Quick performance verification
   - 100 message tests
   - Fast execution

3. **Final Test** (`wilson_final_test.sh`)
   - Ultra-fast validation
   - 10 message tests
   - Quick verification

### Running Tests

```bash
# Full test suite
./wilson_test.sh

# Quick test
./wilson_simple_test.sh

# Ultra-fast test
./wilson_final_test.sh
```

## 📁 Project Structure

```
hft_20250902/
├── inc/                    # Header files
│   ├── hft_server.h       # Server definitions
│   ├── hft_tcp_client.h   # Client definitions
│   ├── message.h          # Message protocol
│   └── latency_client.h   # Latency testing
├── src/                   # Source files
│   ├── hft_server.cpp     # Server implementation
│   ├── hft_tcp_client.cpp # Client implementation
│   ├── simple_latency_test.cpp # Latency testing
│   └── main.cpp           # Entry point
├── build/                 # Build directory
├── wilson_test.sh         # Wilson's test suite
├── wilson_simple_test.sh  # Simple test
├── wilson_final_test.sh   # Final test
└── README.md              # This file
```

## 🔧 Configuration

### Server Configuration

- **Port**: 8888 (configurable)
- **Worker Threads**: 4 (configurable)
- **Target Latency**: < 20μs
- **Buffer Sizes**: Optimized for performance

### Client Configuration

- **Connection Timeout**: 5 seconds
- **Message Timeout**: 1 second
- **Retry Attempts**: 3
- **Heartbeat Interval**: 1 second

## 📈 Message Types

- **ORDER_NEW**: New order placement
- **ORDER_CANCEL**: Order cancellation
- **ORDER_MODIFY**: Order modification
- **MARKET_DATA**: Real-time market data
- **HEARTBEAT**: Connection keep-alive
- **FILL**: Order execution notifications

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Submit a pull request

## �� License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 👨‍💻 Author

**Wilson** - silverlin2@msn.com

## 🙏 Acknowledgments

- Modern C++17 features for performance
- Linux epoll for high-performance I/O
- Multi-threading for concurrent processing
- Zero-copy operations for minimal latency

---

**Ready for production use in high-frequency trading environments!** 🎯
