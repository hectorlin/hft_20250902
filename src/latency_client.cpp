#include "latency_client.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <mutex>

namespace hft {

LatencyTestClient::LatencyTestClient(const std::string& server_ip, uint16_t server_port)
    : server_ip_(server_ip), server_port_(server_port), socket_fd_(-1), connected_(false),
      gen_(rd_()), message_id_dist_(1, UINT64_MAX), quantity_dist_(100, 10000),
      price_dist_(100000, 200000) {
    
    // Initialize test symbols
    test_symbols_ = {"AAPL", "GOOGL", "MSFT", "TSLA", "AMZN", "NVDA", "META", "NFLX"};
}

LatencyTestClient::~LatencyTestClient() {
    disconnect();
}

bool LatencyTestClient::connect() {
    // Create socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ == -1) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Set socket options
    setup_socket_options(socket_fd_);
    set_non_blocking(socket_fd_);
    
    // Connect to server
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port_);
    if (inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid server IP address" << std::endl;
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
    
    // Wait for connection to complete
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
        std::cerr << "Connection failed" << std::endl;
        close(socket_fd_);
        return false;
    }
    
    connected_ = true;
    
    // Start receiver thread
    stop_receiver_ = false;
    receiver_thread_ = std::thread(&LatencyTestClient::receive_responses, this);
    
    std::cout << "Connected to HFT server at " << server_ip_ << ":" << server_port_ << std::endl;
    return true;
}

void LatencyTestClient::disconnect() {
    connected_ = false;
    stop_receiver_ = true;
    
    if (receiver_thread_.joinable()) {
        receiver_thread_.join();
    }
    
    if (socket_fd_ != -1) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    
    std::cout << "Disconnected from HFT server" << std::endl;
}

void LatencyTestClient::run_latency_test(size_t num_messages, uint32_t message_interval_ms) {
    if (!connected_) {
        std::cerr << "Not connected to server" << std::endl;
        return;
    }
    
    std::cout << "\n=== Latency Test ===" << std::endl;
    std::cout << "Messages: " << num_messages << std::endl;
    std::cout << "Interval: " << message_interval_ms << "ms" << std::endl;
    std::cout << "===================" << std::endl;
    
    reset_stats();
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < num_messages; ++i) {
        Message msg = create_test_message();
        msg.message_id = message_id_dist_(gen_);
        msg.update_timestamp();
        
        auto send_time = std::chrono::high_resolution_clock::now();
        send_message(msg);
        
        // Store send time for latency calculation
        {
            std::lock_guard<std::mutex> lock(latency_mutex_);
            latency_measurements_.push_back(send_time.time_since_epoch().count());
        }
        
        messages_sent_++;
        
        if (message_interval_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(message_interval_ms));
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Wait for responses
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    std::cout << "\nTest completed in " << duration.count() << "ms" << std::endl;
    print_stats();
}

void LatencyTestClient::run_burst_test(size_t burst_size, size_t num_bursts, uint32_t burst_interval_ms) {
    if (!connected_) {
        std::cerr << "Not connected to server" << std::endl;
        return;
    }
    
    std::cout << "\n=== Burst Test ===" << std::endl;
    std::cout << "Burst size: " << burst_size << std::endl;
    std::cout << "Number of bursts: " << num_bursts << std::endl;
    std::cout << "Burst interval: " << burst_interval_ms << "ms" << std::endl;
    std::cout << "=================" << std::endl;
    
    reset_stats();
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t burst = 0; burst < num_bursts; ++burst) {
        // Send burst
        for (size_t i = 0; i < burst_size; ++i) {
            Message msg = create_test_message();
            msg.message_id = message_id_dist_(gen_);
            msg.update_timestamp();
            
            auto send_time = std::chrono::high_resolution_clock::now();
            send_message(msg);
            
            {
                std::lock_guard<std::mutex> lock(latency_mutex_);
                latency_measurements_.push_back(send_time.time_since_epoch().count());
            }
            
            messages_sent_++;
        }
        
        if (burst_interval_ms > 0 && burst < num_bursts - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(burst_interval_ms));
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Wait for responses
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    std::cout << "\nBurst test completed in " << duration.count() << "ms" << std::endl;
    print_stats();
}

void LatencyTestClient::run_sustained_test(uint32_t duration_seconds, uint32_t messages_per_second) {
    if (!connected_) {
        std::cerr << "Not connected to server" << std::endl;
        return;
    }
    
    std::cout << "\n=== Sustained Load Test ===" << std::endl;
    std::cout << "Duration: " << duration_seconds << " seconds" << std::endl;
    std::cout << "Target rate: " << messages_per_second << " msg/s" << std::endl;
    std::cout << "===========================" << std::endl;
    
    reset_stats();
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_time = start_time + std::chrono::seconds(duration_seconds);
    
    uint32_t message_interval_us = 1000000 / messages_per_second; // microseconds between messages
    
    while (std::chrono::high_resolution_clock::now() < end_time) {
        Message msg = create_test_message();
        msg.message_id = message_id_dist_(gen_);
        msg.update_timestamp();
        
        auto send_time = std::chrono::high_resolution_clock::now();
        send_message(msg);
        
        {
            std::lock_guard<std::mutex> lock(latency_mutex_);
            latency_measurements_.push_back(send_time.time_since_epoch().count());
        }
        
        messages_sent_++;
        
        std::this_thread::sleep_for(std::chrono::microseconds(message_interval_us));
    }
    
    auto actual_end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(actual_end_time - start_time);
    
    // Wait for responses
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    std::cout << "\nSustained test completed in " << duration.count() << "ms" << std::endl;
    print_stats();
}

void LatencyTestClient::send_message(const Message& msg) {
    if (!connected_) return;
    
    ssize_t bytes_sent = send(socket_fd_, &msg, sizeof(msg), MSG_NOSIGNAL);
    if (bytes_sent == -1) {
        errors_++;
        std::cerr << "Send failed: " << strerror(errno) << std::endl;
    }
}

void LatencyTestClient::receive_responses() {
    char buffer[4096];
    
    while (!stop_receiver_) {
        ssize_t bytes_received = recv(socket_fd_, buffer, sizeof(buffer), MSG_DONTWAIT);
        
        if (bytes_received > 0) {
                // Process received messages
                size_t offset = 0;
                while (offset + sizeof(Message) <= static_cast<size_t>(bytes_received)) {
                    // const Message* msg = reinterpret_cast<const Message*>(buffer + offset);
                
                // Calculate latency
                auto receive_time = std::chrono::high_resolution_clock::now();
                auto receive_ns = receive_time.time_since_epoch().count();
                
                // Find matching send time
                {
                    std::lock_guard<std::mutex> lock(latency_mutex_);
                    if (!latency_measurements_.empty()) {
                        auto send_ns = latency_measurements_.front();
                        latency_measurements_.erase(latency_measurements_.begin());
                        
                        uint64_t latency_ns = receive_ns - send_ns;
                        update_latency_stats(latency_ns);
                    }
                }
                
                messages_received_++;
                offset += sizeof(Message);
            }
        } else if (bytes_received == 0) {
            // Server disconnected
            break;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            errors_++;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

void LatencyTestClient::update_latency_stats(uint64_t latency_ns) {
    total_latency_ns_ += latency_ns;
    
    uint64_t current_min = min_latency_ns_.load();
    while (latency_ns < current_min && !min_latency_ns_.compare_exchange_weak(current_min, latency_ns)) {
        // Retry if another thread updated min_latency_ns_
    }
    
    uint64_t current_max = max_latency_ns_.load();
    while (latency_ns > current_max && !max_latency_ns_.compare_exchange_weak(current_max, latency_ns)) {
        // Retry if another thread updated max_latency_ns_
    }
}

void LatencyTestClient::calculate_percentiles() const {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    
    if (latency_measurements_.empty()) return;
    
    std::vector<uint64_t> sorted_measurements = latency_measurements_;
    std::sort(sorted_measurements.begin(), sorted_measurements.end());
    
    size_t size = sorted_measurements.size();
    if (size > 0) {
        p50_latency_us = sorted_measurements[size * 0.5] / 1000.0;
        p95_latency_us = sorted_measurements[size * 0.95] / 1000.0;
        p99_latency_us = sorted_measurements[size * 0.99] / 1000.0;
        p99_9_latency_us = sorted_measurements[size * 0.999] / 1000.0;
    }
}

Message LatencyTestClient::create_test_message() {
    Message msg;
    msg.message_type = MessageType::ORDER_NEW;
    msg.status = MessageStatus::PENDING;
    msg.source_id = 1;
    msg.destination_id = 0;
    msg.payload_size = sizeof(OrderMessage);
    
    // Create order message in payload
    OrderMessage order;
    order.side = OrderSide::BUY;
    order.order_type = OrderType::LIMIT;
    order.time_in_force = TimeInForce::DAY;
    order.quantity = quantity_dist_(gen_);
    order.price = price_dist_(gen_);
    
    // Set random symbol
    std::string symbol = test_symbols_[gen_() % test_symbols_.size()];
    std::strncpy(order.symbol.data(), symbol.c_str(), order.symbol.size() - 1);
    order.symbol[order.symbol.size() - 1] = '\0';
    
    // Copy order to message payload
    std::memcpy(msg.payload.data(), &order, sizeof(order));
    
    return msg;
}

void LatencyTestClient::setup_socket_options(int sock_fd) {
    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // TCP_NODELAY for low latency (use IPPROTO_TCP with value 1)
    int tcp_nodelay = 1;
    setsockopt(sock_fd, IPPROTO_TCP, 1, &tcp_nodelay, sizeof(tcp_nodelay));
    setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
    
    int send_buf_size = 1024 * 1024;
    int recv_buf_size = 1024 * 1024;
    setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, &send_buf_size, sizeof(send_buf_size));
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size));
}

void LatencyTestClient::set_non_blocking(int sock_fd) {
    int flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
}

LatencyTestClient::TestStats LatencyTestClient::get_stats() const {
    const_cast<LatencyTestClient*>(this)->calculate_percentiles();
    
    TestStats stats{};
    stats.total_messages_sent = messages_sent_.load();
    stats.total_messages_received = messages_received_.load();
    stats.total_latency_ns = total_latency_ns_.load();
    stats.min_latency_ns = min_latency_ns_.load();
    stats.max_latency_ns = max_latency_ns_.load();
    stats.avg_latency_us = (messages_received_.load() > 0) ? 
                          (total_latency_ns_.load() / 1000.0) / messages_received_.load() : 0.0;
    stats.p50_latency_us = p50_latency_us;
    stats.p95_latency_us = p95_latency_us;
    stats.p99_latency_us = p99_latency_us;
    stats.p99_9_latency_us = p99_9_latency_us;
    stats.errors = errors_.load();
    
    // Calculate throughput
    auto now = std::chrono::high_resolution_clock::now();
    stats.throughput_mps = (messages_received_.load() > 0) ? 
                          messages_received_.load() / 1.0 : 0.0; // Simplified for now
    
    return stats;
}

void LatencyTestClient::print_stats() const {
    auto stats = get_stats();
    
    std::cout << "\n=== Latency Test Results ===" << std::endl;
    std::cout << "Messages sent: " << stats.total_messages_sent << std::endl;
    std::cout << "Messages received: " << stats.total_messages_received << std::endl;
    std::cout << "Errors: " << stats.errors << std::endl;
    std::cout << "Success rate: " << std::fixed << std::setprecision(2) 
              << (stats.total_messages_sent > 0 ? 
                  (double)stats.total_messages_received / stats.total_messages_sent * 100.0 : 0.0) 
              << "%" << std::endl;
    
    if (stats.total_messages_received > 0) {
        std::cout << "\n--- Latency Statistics ---" << std::endl;
        std::cout << "Average latency: " << std::fixed << std::setprecision(2) 
                  << stats.avg_latency_us << " μs" << std::endl;
        std::cout << "Minimum latency: " << stats.min_latency_ns / 1000.0 << " μs" << std::endl;
        std::cout << "Maximum latency: " << stats.max_latency_ns / 1000.0 << " μs" << std::endl;
        std::cout << "P50 latency: " << std::fixed << std::setprecision(2) 
                  << stats.p50_latency_us << " μs" << std::endl;
        std::cout << "P95 latency: " << std::fixed << std::setprecision(2) 
                  << stats.p95_latency_us << " μs" << std::endl;
        std::cout << "P99 latency: " << std::fixed << std::setprecision(2) 
                  << stats.p99_latency_us << " μs" << std::endl;
        std::cout << "P99.9 latency: " << std::fixed << std::setprecision(2) 
                  << stats.p99_9_latency_us << " μs" << std::endl;
        
        // Performance assessment
        std::cout << "\n--- Performance Assessment ---" << std::endl;
        if (stats.avg_latency_us < 20.0) {
            std::cout << "✓ EXCELLENT: Average latency < 20μs (HFT target met)" << std::endl;
        } else if (stats.avg_latency_us < 100.0) {
            std::cout << "✓ GOOD: Average latency < 100μs (acceptable for HFT)" << std::endl;
        } else if (stats.avg_latency_us < 1000.0) {
            std::cout << "⚠ FAIR: Average latency < 1ms (may need optimization)" << std::endl;
        } else {
            std::cout << "✗ POOR: Average latency > 1ms (needs optimization)" << std::endl;
        }
    }
    
    std::cout << "=============================" << std::endl;
}

void LatencyTestClient::reset_stats() {
    messages_sent_ = 0;
    messages_received_ = 0;
    total_latency_ns_ = 0;
    min_latency_ns_ = UINT64_MAX;
    max_latency_ns_ = 0;
    errors_ = 0;
    
    {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        latency_measurements_.clear();
    }
}

} // namespace hft
