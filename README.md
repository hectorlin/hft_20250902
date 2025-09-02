# HFT Server - High Frequency Trading Server

A high-performance, low-latency trading server designed for high-frequency trading applications with sub-20μs latency targets.

## 🚀 Features

- **Ultra-Low Latency**: Target latency < 20μs
- **High Performance**: Optimized C++17 with native CPU optimization
- **Multi-threaded**: Configurable worker threads for concurrent processing
- **Real-time Statistics**: Live performance monitoring and reporting
- **Order Management**: Support for new, cancel, and replace orders
- **Market Data**: Real-time market data broadcasting
- **Connection Management**: Efficient client connection handling with epoll
- **Zero Warnings**: Clean compilation with strict warning checks

## 📁 Project Structure

```
hft_20250902/
├── README.md              # This file
├── compile.sh             # Compilation script
├── src/                   # Source code
│   ├── main.cpp          # Main application entry point
│   └── hft_server.cpp    # Core server implementation
├── inc/                   # Header files
│   ├── hft_server.h      # Server class definitions
│   └── message.h         # Message structures and types
├── bin/                   # Executable files
│   └── hft_server        # Compiled server binary
└── lib/                   # Object files (temporary)
```

## 🛠️ Compilation

### Prerequisites
- GCC 7.0+ with C++17 support
- Linux system with epoll support
- pthread library

### Build Commands

```bash
# Release mode (default) - Optimized for production
./compile.sh

# Debug mode - With debug symbols and no optimization
./compile.sh debug
```

### Compilation Features
- **Release Mode**: `-O3 -march=native -mtune=native` for maximum performance
- **Debug Mode**: `-g -O0 -DDEBUG` for development and debugging
- **Strict Warnings**: `-Wall -Wextra -Wpedantic` for code quality
- **Auto Cleanup**: Intermediate files automatically removed in release mode

## �� Usage

### Basic Usage
```bash
# Start server with default configuration
./bin/hft_server

# Default settings:
# - IP: 127.0.0.1
# - Port: 8888
# - Threads: 4
```

### Advanced Configuration
```bash
# Custom IP and port
./bin/hft_server --ip 0.0.0.0 --port 9999

# Custom thread count
./bin/hft_server --threads 8

# Combined options
./bin/hft_server --ip 192.168.1.100 --port 8888 --threads 16

# Show help
./bin/hft_server --help
```

### Command Line Options
| Option | Description | Default |
|--------|-------------|---------|
| `--ip <ip>` | Server IP address | 127.0.0.1 |
| `--port <port>` | Server port | 8888 |
| `--threads <n>` | Number of worker threads | 4 |
| `--help` | Show help message | - |

## 📊 Performance Monitoring

The server provides real-time statistics every 5 seconds:

```
=== Server Statistics ===
Total Messages: 1250
Active Connections: 3
Peak Connections: 5
✓ Latency target met (< 20μs)
========================
```

### Statistics Explained
- **Total Messages**: Cumulative messages processed
- **Active Connections**: Current connected clients
- **Peak Connections**: Maximum concurrent connections
- **Latency Target**: Real-time latency monitoring

## 🔧 Architecture

### Core Components

#### HFTServer (Singleton)
- Main server class managing all operations
- Thread pool for concurrent request processing
- Connection management with epoll
- Service registration and routing

#### Message Services
- **OrderService**: Handles order operations (new, cancel, replace)
- **MarketDataService**: Manages market data broadcasting

#### Message Types
- **OrderMessage**: Trading order data
- **MarketDataMessage**: Real-time market data
- **Message**: Base message structure

### Network Architecture
- **Protocol**: TCP with custom message format
- **I/O Model**: epoll-based event-driven architecture
- **Connection Handling**: Non-blocking sockets with edge-triggered events
- **Buffering**: Pre-allocated buffers for zero-copy operations

## 📡 Message Protocol

### Message Structure
```cpp
struct Message {
    uint64_t message_id;           // Unique identifier
    uint64_t timestamp;            // Nanosecond timestamp
    uint32_t sequence_number;      // Ordering sequence
    MessageType message_type;      // Message type
    MessageStatus status;          // Processing status
    uint32_t source_id;            // Source system ID
    uint32_t destination_id;       // Destination system ID
    uint32_t payload_size;         // Payload size
    std::array<uint8_t, 1024> payload; // Message data
};
```

### Message Types
- `ORDER_NEW`: New order submission
- `ORDER_CANCEL`: Order cancellation
- `ORDER_REPLACE`: Order modification
- `ORDER_FILL`: Order execution
- `ORDER_REJECT`: Order rejection
- `MARKET_DATA`: Market data update
- `HEARTBEAT`: Connection keep-alive
- `LOGIN`: Client authentication
- `LOGOUT`: Client disconnection
- `ERROR`: Error message

## 🔒 Security Features

- **Connection Authentication**: Client authentication support
- **Input Validation**: Message size and type validation
- **Error Handling**: Graceful error recovery
- **Resource Management**: Automatic cleanup of resources

## 🚦 Signal Handling

The server supports graceful shutdown:
- **SIGINT** (Ctrl+C): Graceful shutdown
- **SIGTERM**: Graceful shutdown
- **Connection Cleanup**: Automatic client disconnection handling

## 📈 Performance Optimization

### Compilation Optimizations
- **CPU-specific**: `-march=native -mtune=native`
- **Aggressive Optimization**: `-O3` for maximum speed
- **Link-time Optimization**: Available with LTO flags

### Runtime Optimizations
- **Zero-copy Operations**: Pre-allocated buffers
- **Lock-free Design**: Minimal locking for high concurrency
- **Memory Pool**: Efficient memory management
- **Socket Tuning**: Optimized TCP settings

## 🧪 Testing

### Connection Testing
```bash
# Test server connectivity
nc -v 127.0.0.1 8888

# Test with telnet (if available)
telnet 127.0.0.1 8888
```

### Performance Testing
- Monitor latency statistics in server output
- Check connection handling under load
- Verify message processing rates

## 🐛 Troubleshooting

### Common Issues

#### Port Already in Use
```bash
# Check if port is in use
netstat -tlnp | grep 8888

# Kill existing process
kill <process_id>
```

#### Permission Denied
```bash
# Make script executable
chmod +x compile.sh

# Make binary executable
chmod +x bin/hft_server
```

#### Compilation Errors
- Ensure GCC 7.0+ is installed
- Check C++17 support: `g++ --version`
- Verify all dependencies are available

## 📝 Development

### Code Style
- **C++17 Standard**: Modern C++ features
- **RAII**: Resource management
- **Exception Safety**: Proper error handling
- **Const Correctness**: Immutable where possible

### Adding New Features
1. Define new message types in `inc/message.h`
2. Implement service handlers in `src/hft_server.cpp`
3. Register services in `main.cpp`
4. Update documentation

## 📄 License

This project is part of a high-frequency trading system implementation.

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make changes with zero warnings
4. Test thoroughly
5. Submit a pull request

## 📞 Support

For issues and questions:
- Check the troubleshooting section
- Review server logs for error messages
- Verify system requirements

---

**Note**: This is a high-performance trading server designed for low-latency applications. Ensure proper testing before production use.
