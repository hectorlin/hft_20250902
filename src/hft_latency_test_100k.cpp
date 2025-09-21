#include "hft_tcp_client.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>

namespace hft {

class HFTLatencyTest100K {
public:
    HFTLatencyTest100K(const std::string& server_ip = "127.0.0.1", 
                       uint16_t server_port = 8888)
        : server_ip_(server_ip), server_port_(server_port),
          gen_(rd_()), message_id_dist_(1, UINT64_MAX), 
          quantity_dist_(100, 10000), price_dist_(100000, 200000) {
        
        test_symbols_ = {"AAPL", "GOOGL", "MSFT", "TSLA", "AMZN", "NVDA", "META", "NFLX"};
        recv_buffer_.resize(65536);
        send_buffer_.resize(65536);
    }
    
    bool connect() {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ == -1) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            return false;
        }
        
        setup_socket_options(socket_fd_);
        set_non_blocking(socket_fd_);
        
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port_);
        if (inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "Invalid server IP address: " << server_ip_ << std::endl;
            close(socket_fd_);
            return false;
        }
        
        int result = ::connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
        if (result == -1) {
            if (errno != EINPROGRESS) {
                std::cerr << "Failed to connect: " << strerror(errno) << std::endl;
                close(socket_fd_);
                return false;
            }
        }
        
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(socket_fd_, &write_fds);
        
        timeval timeout{};
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        
        int select_result = select(socket_fd_ + 1, nullptr, &write_fds, nullptr, &timeout);
        if (select_result <= 0) {
            std::cerr << "Connection timeout" << std::endl;
            close(socket_fd_);
            return false;
        }
        
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &error, &len) == -1 || error != 0) {
            std::cerr << "Connection failed: " << strerror(error) << std::endl;
            close(socket_fd_);
            return false;
        }
        
        std::cout << "Connected to HFT server at " << server_ip_ << ":" << server_port_ << std::endl;
        return true;
    }
    
    void disconnect() {
        if (socket_fd_ != -1) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
    }
    
    void run_latency_test(size_t num_messages) {
        std::cout << "\n=== HFT Latency Test (100K Messages) ===" << std::endl;
        std::cout << "Messages: " << num_messages << std::endl;
        std::cout << "Mode: High-frequency burst" << std::endl;
        std::cout << "=========================================" << std::endl;
        
        latency_measurements_.clear();
        latency_measurements_.reserve(num_messages);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Send all messages as fast as possible
        for (size_t i = 0; i < num_messages; ++i) {
            OrderMessage order = create_test_order();
            order.message_id = message_id_dist_(gen_);
            order.update_timestamp();
            
            auto send_time = std::chrono::high_resolution_clock::now();
            send_order(order);
            
            // Store send time for latency calculation
            send_times_.push_back(send_time.time_since_epoch().count());
            
            if (i % 10000 == 0 && i > 0) {
                std::cout << "Sent " << i << " messages..." << std::endl;
            }
        }
        
        auto send_end_time = std::chrono::high_resolution_clock::now();
        auto send_duration = std::chrono::duration_cast<std::chrono::milliseconds>(send_end_time - start_time);
        
        std::cout << "All " << num_messages << " messages sent in " << send_duration.count() << "ms" << std::endl;
        std::cout << "Sending rate: " << (num_messages * 1000.0 / send_duration.count()) << " msg/sec" << std::endl;
        
        // Receive responses and calculate latency
        std::cout << "\nReceiving responses and calculating latency..." << std::endl;
        receive_responses();
        
        // Calculate and display statistics
        calculate_statistics();
    }
    
    void receive_responses() {
        char buffer[65536];
        size_t messages_received = 0;
        size_t send_time_index = 0;
        
        auto start_receive = std::chrono::high_resolution_clock::now();
        
        while (messages_received < send_times_.size()) {
            ssize_t bytes_received = recv(socket_fd_, buffer, sizeof(buffer), MSG_DONTWAIT);
            
            if (bytes_received > 0) {
                size_t offset = 0;
                while (offset + sizeof(Message) <= static_cast<size_t>(bytes_received) && 
                       send_time_index < send_times_.size()) {
                    
                    const Message* msg = reinterpret_cast<const Message*>(buffer + offset);
                    auto receive_time = std::chrono::high_resolution_clock::now();
                    auto receive_ns = receive_time.time_since_epoch().count();
                    
                    // Calculate latency
                    uint64_t latency_ns = receive_ns - send_times_[send_time_index];
                    latency_measurements_.push_back(latency_ns);
                    
                    messages_received++;
                    send_time_index++;
                    
                    if (messages_received % 10000 == 0) {
                        std::cout << "Received " << messages_received << " responses..." << std::endl;
                    }
                    
                    offset += sizeof(Message);
                }
            } else if (bytes_received == 0) {
                std::cout << "Server disconnected" << std::endl;
                break;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        
        auto end_receive = std::chrono::high_resolution_clock::now();
        auto receive_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_receive - start_receive);
        
        std::cout << "Received " << messages_received << " responses in " << receive_duration.count() << "ms" << std::endl;
    }
    
    void calculate_statistics() {
        if (latency_measurements_.empty()) {
            std::cout << "No latency measurements available" << std::endl;
            return;
        }
        
        // Sort for percentile calculations
        std::vector<uint64_t> sorted_latencies = latency_measurements_;
        std::sort(sorted_latencies.begin(), sorted_latencies.end());
        
        // Calculate statistics
        uint64_t min_latency_ns = *std::min_element(latency_measurements_.begin(), latency_measurements_.end());
        uint64_t max_latency_ns = *std::max_element(latency_measurements_.begin(), latency_measurements_.end());
        uint64_t total_latency_ns = std::accumulate(latency_measurements_.begin(), latency_measurements_.end(), 0ULL);
        double avg_latency_ns = static_cast<double>(total_latency_ns) / latency_measurements_.size();
        
        // Calculate percentiles
        size_t size = sorted_latencies.size();
        uint64_t p50 = sorted_latencies[size * 0.50];
        uint64_t p90 = sorted_latencies[size * 0.90];
        uint64_t p95 = sorted_latencies[size * 0.95];
        uint64_t p99 = sorted_latencies[size * 0.99];
        uint64_t p99_9 = sorted_latencies[size * 0.999];
        
        // Display results
        std::cout << "\n=== LATENCY STATISTICS (100K Messages) ===" << std::endl;
        std::cout << "Total Messages: " << latency_measurements_.size() << std::endl;
        std::cout << "Min Latency:    " << std::fixed << std::setprecision(2) 
                  << min_latency_ns / 1000.0 << " μs" << std::endl;
        std::cout << "Max Latency:    " << std::fixed << std::setprecision(2) 
                  << max_latency_ns / 1000.0 << " μs" << std::endl;
        std::cout << "Avg Latency:    " << std::fixed << std::setprecision(2) 
                  << avg_latency_ns / 1000.0 << " μs" << std::endl;
        std::cout << "\n--- Percentile Analysis ---" << std::endl;
        std::cout << "P50  (Median):  " << std::fixed << std::setprecision(2) 
                  << p50 / 1000.0 << " μs" << std::endl;
        std::cout << "P90:            " << std::fixed << std::setprecision(2) 
                  << p90 / 1000.0 << " μs" << std::endl;
        std::cout << "P95:            " << std::fixed << std::setprecision(2) 
                  << p95 / 1000.0 << " μs" << std::endl;
        std::cout << "P99:            " << std::fixed << std::setprecision(2) 
                  << p99 / 1000.0 << " μs" << std::endl;
        std::cout << "P99.9:          " << std::fixed << std::setprecision(2) 
                  << p99_9 / 1000.0 << " μs" << std::endl;
        
        // Performance assessment
        std::cout << "\n--- Performance Assessment ---" << std::endl;
        if (avg_latency_ns < 1000) {
            std::cout << "✓ EXCELLENT: Average latency < 1μs (Ultra-low latency)" << std::endl;
        } else if (avg_latency_ns < 10000) {
            std::cout << "✓ VERY GOOD: Average latency < 10μs (HFT target met)" << std::endl;
        } else if (avg_latency_ns < 100000) {
            std::cout << "✓ GOOD: Average latency < 100μs (Acceptable for HFT)" << std::endl;
        } else if (avg_latency_ns < 1000000) {
            std::cout << "⚠ FAIR: Average latency < 1ms (May need optimization)" << std::endl;
        } else {
            std::cout << "✗ POOR: Average latency > 1ms (Needs optimization)" << std::endl;
        }
        
        std::cout << "===============================" << std::endl;
    }
    
    OrderMessage create_test_order() {
        OrderMessage order;
        order.side = OrderSide::BUY;
        order.order_type = OrderType::LIMIT;
        order.time_in_force = TimeInForce::DAY;
        order.quantity = quantity_dist_(gen_);
        order.price = price_dist_(gen_);
        order.client_order_id = message_id_dist_(gen_);
        
        std::string symbol = test_symbols_[gen_() % test_symbols_.size()];
        std::strncpy(order.symbol.data(), symbol.c_str(), order.symbol.size() - 1);
        order.symbol[order.symbol.size() - 1] = '\0';
        
        return order;
    }
    
    bool send_order(const OrderMessage& order) {
        Message msg;
        msg.message_id = order.message_id;
        msg.timestamp = order.timestamp;
        msg.message_type = MessageType::ORDER_NEW;
        msg.status = MessageStatus::PENDING;
        msg.source_id = 1;
        msg.destination_id = 0;
        msg.payload_size = sizeof(OrderMessage);
        
        size_t copy_size = std::min(sizeof(order), msg.payload.size());
        std::memcpy(msg.payload.data(), &order, copy_size);
        
        ssize_t bytes_sent = send(socket_fd_, &msg, sizeof(msg), MSG_NOSIGNAL);
        return bytes_sent == sizeof(msg);
    }
    
    void setup_socket_options(int sock_fd) {
        int opt = 1;
        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        int tcp_nodelay = 1;
        setsockopt(sock_fd, IPPROTO_TCP, 1, &tcp_nodelay, sizeof(tcp_nodelay));
        
        setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
        
        int send_buf_size = 1024 * 1024;
        int recv_buf_size = 1024 * 1024;
        setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, &send_buf_size, sizeof(send_buf_size));
        setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size));
    }
    
    void set_non_blocking(int sock_fd) {
        int flags = fcntl(sock_fd, F_GETFL, 0);
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
    }

private:
    std::string server_ip_;
    uint16_t server_port_;
    int socket_fd_{-1};
    
    std::vector<uint64_t> latency_measurements_;
    std::vector<uint64_t> send_times_;
    
    std::vector<char> recv_buffer_;
    std::vector<char> send_buffer_;
    
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_int_distribution<uint64_t> message_id_dist_;
    std::uniform_int_distribution<uint32_t> quantity_dist_;
    std::uniform_int_distribution<uint64_t> price_dist_;
    
    std::vector<std::string> test_symbols_;
};

} // namespace hft

int main(int argc, char* argv[]) {
    using namespace hft;
    
    std::string server_ip = "127.0.0.1";
    uint16_t server_port = 8888;
    size_t num_messages = 100000;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ip" && i + 1 < argc) {
            server_ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            server_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--messages" && i + 1 < argc) {
            num_messages = std::stoul(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "HFT Latency Test (100K Messages)\n"
                      << "Usage: " << argv[0] << " [options]\n\n"
                      << "Options:\n"
                      << "  --ip <ip>              Server IP address (default: 127.0.0.1)\n"
                      << "  --port <port>          Server port (default: 8888)\n"
                      << "  --messages <n>         Number of messages (default: 100000)\n"
                      << "  --help                 Show this help message\n";
            return 0;
        }
    }
    
    std::cout << "=== HFT Latency Test (100K Messages) ===" << std::endl;
    std::cout << "Server: " << server_ip << ":" << server_port << std::endl;
    std::cout << "Messages: " << num_messages << std::endl;
    std::cout << "=======================================" << std::endl;
    
    HFTLatencyTest100K test(server_ip, server_port);
    
    if (!test.connect()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }
    
    test.run_latency_test(num_messages);
    test.disconnect();
    
    std::cout << "\nHFT Latency Test (100K) completed!" << std::endl;
    return 0;
}
