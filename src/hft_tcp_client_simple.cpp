#include "hft_tcp_client.h"
#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>
#include <iomanip>

namespace hft {

// Simplified HFT TCP Client without epoll thread
class HFTTCPClientSimple {
public:
    HFTTCPClientSimple(const std::string& server_ip = "127.0.0.1", 
                       uint16_t server_port = 8888,
                       uint32_t client_id = 1)
        : server_ip_(server_ip), server_port_(server_port), client_id_(client_id),
          gen_(rd_()), message_id_dist_(1, UINT64_MAX), quantity_dist_(100, 10000),
          price_dist_(100000, 200000) {
        
        test_symbols_ = {"AAPL", "GOOGL", "MSFT", "TSLA", "AMZN", "NVDA", "META", "NFLX"};
        recv_buffer_.resize(65536);
        send_buffer_.resize(65536);
        
        stats_.start_time = std::chrono::steady_clock::now();
        stats_.last_message_time = stats_.start_time;
    }
    
    ~HFTTCPClientSimple() {
        disconnect();
    }
    
    bool connect(uint32_t timeout_ms = 5000) {
        if (connection_state_.load() == ConnectionState::CONNECTED) {
            return true;
        }
        
        connection_state_.store(ConnectionState::CONNECTING);
        
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ == -1) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            connection_state_.store(ConnectionState::ERROR);
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
            socket_fd_ = -1;
            connection_state_.store(ConnectionState::ERROR);
            return false;
        }
        
        int result = ::connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
        if (result == -1) {
            if (errno != EINPROGRESS) {
                std::cerr << "Failed to connect: " << strerror(errno) << std::endl;
                close(socket_fd_);
                socket_fd_ = -1;
                connection_state_.store(ConnectionState::ERROR);
                return false;
            }
        }
        
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(socket_fd_, &write_fds);
        
        timeval timeout{};
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        
        int select_result = select(socket_fd_ + 1, nullptr, &write_fds, nullptr, &timeout);
        if (select_result <= 0) {
            std::cerr << "Connection timeout after " << timeout_ms << "ms" << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            connection_state_.store(ConnectionState::ERROR);
            return false;
        }
        
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &error, &len) == -1 || error != 0) {
            std::cerr << "Connection failed: " << strerror(error) << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            connection_state_.store(ConnectionState::ERROR);
            return false;
        }
        
        connection_state_.store(ConnectionState::CONNECTED);
        stats_.connection_attempts++;
        
        std::cout << "Connected to HFT server at " << server_ip_ << ":" << server_port_ << std::endl;
        return true;
    }
    
    void disconnect() {
        if (connection_state_.load() == ConnectionState::DISCONNECTED) {
            return;
        }
        
        connection_state_.store(ConnectionState::DISCONNECTED);
        
        if (socket_fd_ != -1) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        
        std::cout << "Disconnected from HFT server" << std::endl;
    }
    
    bool is_connected() const {
        return connection_state_.load() == ConnectionState::CONNECTED;
    }
    
    bool send_message(const Message& msg) {
        if (connection_state_.load() != ConnectionState::CONNECTED) {
            return false;
        }
        
        ssize_t bytes_sent = send(socket_fd_, &msg, sizeof(msg), MSG_NOSIGNAL);
        if (bytes_sent == sizeof(msg)) {
            stats_.messages_sent++;
            stats_.bytes_sent += sizeof(msg);
            return true;
        } else {
            stats_.errors++;
            return false;
        }
    }
    
    bool send_order(const OrderMessage& order) {
        Message msg;
        msg.message_id = message_id_dist_(gen_);
        msg.update_timestamp();
        msg.message_type = MessageType::ORDER_NEW;
        msg.status = MessageStatus::PENDING;
        msg.source_id = client_id_;
        msg.destination_id = 0;
        msg.payload_size = sizeof(OrderMessage);
        
        // Copy order to payload (truncated to fit)
        size_t copy_size = std::min(sizeof(order), msg.payload.size());
        std::memcpy(msg.payload.data(), &order, copy_size);
        
        return send_message(msg);
    }
    
    bool send_heartbeat() {
        Message msg;
        msg.message_id = message_id_dist_(gen_);
        msg.update_timestamp();
        msg.message_type = MessageType::HEARTBEAT;
        msg.status = MessageStatus::PENDING;
        msg.source_id = client_id_;
        msg.destination_id = 0;
        msg.payload_size = 0;
        
        return send_message(msg);
    }
    
    void receive_messages() {
        while (connection_state_.load() == ConnectionState::CONNECTED) {
            ssize_t bytes_received = recv(socket_fd_, recv_buffer_.data(), recv_buffer_.size(), MSG_DONTWAIT);
            
            if (bytes_received > 0) {
                stats_.bytes_received += bytes_received;
                stats_.last_message_time = std::chrono::steady_clock::now();
                
                // Process received data
                size_t offset = 0;
                while (offset + sizeof(Message) <= static_cast<size_t>(bytes_received)) {
                    const Message* msg = reinterpret_cast<const Message*>(recv_buffer_.data() + offset);
                    
                    stats_.messages_received++;
                    
                    // Calculate latency
                    auto receive_time = std::chrono::high_resolution_clock::now();
                    auto receive_ns = receive_time.time_since_epoch().count();
                    
                    if (msg->timestamp > 0) {
                        uint64_t latency_ns = receive_ns - msg->timestamp;
                        update_latency_stats(latency_ns);
                    }
                    
                    // Process message
                    process_message(*msg);
                    
                    offset += sizeof(Message);
                }
            } else if (bytes_received == 0) {
                std::cout << "Server disconnected" << std::endl;
                break;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                std::cerr << "Receive error: " << strerror(errno) << std::endl;
                stats_.errors++;
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    
    void process_message(const Message& msg) {
        std::cout << "Message received: Type=" << static_cast<int>(msg.message_type) 
                  << " ID=" << msg.message_id << " Size=" << msg.payload_size << std::endl;
        
        switch (msg.message_type) {
            case MessageType::ORDER_NEW:
            case MessageType::ORDER_CANCEL:
            case MessageType::ORDER_REPLACE:
            case MessageType::ORDER_FILL:
            case MessageType::ORDER_REJECT:
                if (msg.payload_size >= sizeof(OrderMessage)) {
                    const OrderMessage& order = *reinterpret_cast<const OrderMessage*>(msg.payload.data());
                    std::cout << "Order received: " << order.symbol.data() 
                              << " " << (order.side == OrderSide::BUY ? "BUY" : "SELL")
                              << " " << order.quantity << " @ " << order.price << std::endl;
                }
                break;
                
            case MessageType::MARKET_DATA:
                if (msg.payload_size >= sizeof(MarketDataMessage)) {
                    const MarketDataMessage& market_data = *reinterpret_cast<const MarketDataMessage*>(msg.payload.data());
                    std::cout << "Market data received: " << market_data.symbol.data()
                              << " Bid: " << market_data.bid_price << " Ask: " << market_data.ask_price << std::endl;
                }
                break;
                
            case MessageType::HEARTBEAT:
                std::cout << "Heartbeat received" << std::endl;
                break;
                
            default:
                break;
        }
    }
    
    OrderMessage create_test_order(const std::string& symbol = "AAPL",
                                  OrderSide side = OrderSide::BUY,
                                  uint32_t quantity = 100,
                                  uint64_t price = 150000) {
        OrderMessage order;
        order.message_id = message_id_dist_(gen_);
        order.update_timestamp();
        order.side = side;
        order.order_type = OrderType::LIMIT;
        order.time_in_force = TimeInForce::DAY;
        order.quantity = quantity;
        order.price = price;
        order.client_order_id = message_id_dist_(gen_);
        
        std::strncpy(order.symbol.data(), symbol.c_str(), order.symbol.size() - 1);
        order.symbol[order.symbol.size() - 1] = '\0';
        
        return order;
    }
    
    void print_stats() const {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - stats_.start_time);
        
        std::cout << "\n=== HFT TCP Client Statistics ===" << std::endl;
        std::cout << "Uptime: " << uptime.count() << " seconds" << std::endl;
        std::cout << "Connection State: " << static_cast<int>(connection_state_.load()) << std::endl;
        std::cout << "Messages Sent: " << stats_.messages_sent << std::endl;
        std::cout << "Messages Received: " << stats_.messages_received << std::endl;
        std::cout << "Bytes Sent: " << stats_.bytes_sent << std::endl;
        std::cout << "Bytes Received: " << stats_.bytes_received << std::endl;
        std::cout << "Connection Attempts: " << stats_.connection_attempts << std::endl;
        std::cout << "Errors: " << stats_.errors << std::endl;
        
        if (stats_.messages_received > 0) {
            std::cout << "\n--- Latency Statistics ---" << std::endl;
            std::cout << "Average Latency: " << std::fixed << std::setprecision(2) 
                      << stats_.avg_latency_us << " μs" << std::endl;
            std::cout << "Min Latency: " << stats_.min_latency_ns / 1000.0 << " μs" << std::endl;
            std::cout << "Max Latency: " << stats_.max_latency_ns / 1000.0 << " μs" << std::endl;
        }
        
        std::cout << "===============================" << std::endl;
    }
    
    void reset_stats() {
        stats_ = ClientStats{};
        stats_.start_time = std::chrono::steady_clock::now();
        stats_.last_message_time = stats_.start_time;
        
        std::lock_guard<std::mutex> lock(latency_mutex_);
        latency_measurements_.clear();
    }

private:
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
    
    void update_latency_stats(uint64_t latency_ns) {
        {
            std::lock_guard<std::mutex> lock(latency_mutex_);
            latency_measurements_.push_back(latency_ns);
            
            if (latency_measurements_.size() > 10000) {
                latency_measurements_.erase(latency_measurements_.begin());
            }
        }
        
        if (latency_ns < stats_.min_latency_ns) {
            stats_.min_latency_ns = latency_ns;
        }
        
        if (latency_ns > stats_.max_latency_ns) {
            stats_.max_latency_ns = latency_ns;
        }
        
        stats_.total_latency_ns += latency_ns;
        if (stats_.messages_received > 0) {
            stats_.avg_latency_us = (stats_.total_latency_ns / 1000.0) / stats_.messages_received;
        }
    }
    
    std::string server_ip_;
    uint16_t server_port_;
    uint32_t client_id_;
    
    std::atomic<ConnectionState> connection_state_{ConnectionState::DISCONNECTED};
    int socket_fd_{-1};
    
    ClientStats stats_;
    std::vector<char> recv_buffer_;
    std::vector<char> send_buffer_;
    
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_int_distribution<uint64_t> message_id_dist_;
    std::uniform_int_distribution<uint32_t> quantity_dist_;
    std::uniform_int_distribution<uint64_t> price_dist_;
    
    std::vector<std::string> test_symbols_;
    std::vector<uint64_t> latency_measurements_;
    mutable std::mutex latency_mutex_;
};

} // namespace hft

int main(int argc, char* argv[]) {
    using namespace hft;
    
    std::string server_ip = "127.0.0.1";
    uint16_t server_port = 8888;
    uint32_t client_id = 1;
    size_t num_messages = 10;
    uint32_t message_interval_ms = 100;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ip" && i + 1 < argc) {
            server_ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            server_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--client-id" && i + 1 < argc) {
            client_id = std::stoul(argv[++i]);
        } else if (arg == "--messages" && i + 1 < argc) {
            num_messages = std::stoul(argv[++i]);
        } else if (arg == "--interval" && i + 1 < argc) {
            message_interval_ms = std::stoul(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "HFT TCP Client Simple Test\n"
                      << "Usage: " << argv[0] << " [options]\n\n"
                      << "Options:\n"
                      << "  --ip <ip>              Server IP address (default: 127.0.0.1)\n"
                      << "  --port <port>          Server port (default: 8888)\n"
                      << "  --client-id <id>       Client ID (default: 1)\n"
                      << "  --messages <n>         Number of messages (default: 10)\n"
                      << "  --interval <ms>        Message interval in ms (default: 100)\n"
                      << "  --help                 Show this help message\n";
            return 0;
        }
    }
    
    std::cout << "=== HFT TCP Client Simple Test ===" << std::endl;
    std::cout << "Server: " << server_ip << ":" << server_port << std::endl;
    std::cout << "Client ID: " << client_id << std::endl;
    std::cout << "Messages: " << num_messages << std::endl;
    std::cout << "Interval: " << message_interval_ms << "ms" << std::endl;
    std::cout << "=================================" << std::endl;
    
    HFTTCPClientSimple client(server_ip, server_port, client_id);
    
    if (!client.connect()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }
    
    // Start receive thread
    std::thread receive_thread([&client]() {
        client.receive_messages();
    });
    
    // Send messages
    std::cout << "\n=== Sending Messages ===" << std::endl;
    for (size_t i = 0; i < num_messages; ++i) {
        OrderMessage order = client.create_test_order("AAPL", OrderSide::BUY, 100, 150000 + i);
        if (client.send_order(order)) {
            std::cout << "Order " << (i + 1) << " sent" << std::endl;
        } else {
            std::cout << "Failed to send order " << (i + 1) << std::endl;
        }
        
        if (message_interval_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(message_interval_ms));
        }
    }
    
    // Wait for responses
    std::cout << "\nWaiting for responses..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Stop receive thread
    client.disconnect();
    if (receive_thread.joinable()) {
        receive_thread.join();
    }
    
    client.print_stats();
    
    std::cout << "\nHFT TCP Client simple test completed!" << std::endl;
    return 0;
}
