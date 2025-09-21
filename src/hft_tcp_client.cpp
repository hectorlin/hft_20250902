#include "hft_tcp_client.h"
#include <errno.h>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace hft {

HFTTCPClient::HFTTCPClient(const std::string& server_ip, uint16_t server_port, uint32_t client_id)
    : server_ip_(server_ip), server_port_(server_port), client_id_(client_id),
      gen_(rd_()), message_id_dist_(1, UINT64_MAX), quantity_dist_(100, 10000),
      price_dist_(100000, 200000) {
    
    // Initialize test symbols
    test_symbols_ = {"AAPL", "GOOGL", "MSFT", "TSLA", "AMZN", "NVDA", "META", "NFLX", "BABA", "NIO"};
    
    // Initialize buffers
    recv_buffer_.resize(BUFFER_SIZE);
    send_buffer_.resize(BUFFER_SIZE);
    
    // Initialize statistics
    stats_.start_time = std::chrono::steady_clock::now();
    stats_.last_message_time = stats_.start_time;
}

HFTTCPClient::~HFTTCPClient() {
    stop();
    disconnect();
}

bool HFTTCPClient::connect(uint32_t timeout_ms) {
    if (connection_state_.load() == ConnectionState::CONNECTED) {
        return true;
    }
    
    connection_state_.store(ConnectionState::CONNECTING);
    
    // Create socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ == -1) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        connection_state_.store(ConnectionState::ERROR);
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
    
    // Wait for connection to complete
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
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.connection_attempts++;
    }
    
    std::cout << "Connected to HFT server at " << server_ip_ << ":" << server_port_ << std::endl;
    return true;
}

void HFTTCPClient::disconnect() {
    if (connection_state_.load() == ConnectionState::DISCONNECTED) {
        return;
    }
    
    connection_state_.store(ConnectionState::DISCONNECTED);
    
    if (socket_fd_ != -1) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
    
    std::cout << "Disconnected from HFT server" << std::endl;
}

bool HFTTCPClient::is_connected() const {
    return connection_state_.load() == ConnectionState::CONNECTED;
}

bool HFTTCPClient::send_message(const Message& msg) {
    if (connection_state_.load() != ConnectionState::CONNECTED) {
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(send_queue_mutex_);
        send_queue_.push(msg);
    }
    send_queue_cv_.notify_one();
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.messages_sent++;
        stats_.bytes_sent += sizeof(Message);
    }
    
    return true;
}

bool HFTTCPClient::send_order(const OrderMessage& order) {
    Message msg;
    msg.message_id = message_id_dist_(gen_);
    msg.update_timestamp();
    msg.message_type = MessageType::ORDER_NEW;
    msg.status = MessageStatus::PENDING;
    msg.source_id = client_id_;
    msg.destination_id = 0;
    msg.payload_size = sizeof(OrderMessage);
    
    // Copy order to payload
    std::memcpy(msg.payload.data(), &order, sizeof(order));
    
    return send_message(msg);
}

bool HFTTCPClient::send_market_data(const MarketDataMessage& market_data) {
    Message msg;
    msg.message_id = message_id_dist_(gen_);
    msg.update_timestamp();
    msg.message_type = MessageType::MARKET_DATA;
    msg.status = MessageStatus::PENDING;
    msg.source_id = client_id_;
    msg.destination_id = 0;
    msg.payload_size = sizeof(MarketDataMessage);
    
    // Copy market data to payload
    std::memcpy(msg.payload.data(), &market_data, sizeof(market_data));
    
    return send_message(msg);
}

bool HFTTCPClient::send_heartbeat() {
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

void HFTTCPClient::set_message_handler(MessageHandler handler) {
    message_handler_ = handler;
}

void HFTTCPClient::set_order_handler(OrderMessageHandler handler) {
    order_handler_ = handler;
}

void HFTTCPClient::set_market_data_handler(MarketDataHandler handler) {
    market_data_handler_ = handler;
}

void HFTTCPClient::start() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    
    // Start background threads
    receive_thread_ = std::thread(&HFTTCPClient::receive_thread_func, this);
    send_thread_ = std::thread(&HFTTCPClient::send_thread_func, this);
    heartbeat_thread_ = std::thread(&HFTTCPClient::heartbeat_thread_func, this);
    epoll_thread_ = std::thread(&HFTTCPClient::epoll_thread_func, this);
    
    std::cout << "HFT TCP Client started with background threads" << std::endl;
}

void HFTTCPClient::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    // Notify all threads to stop
    send_queue_cv_.notify_all();
    
    // Join threads
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    if (send_thread_.joinable()) {
        send_thread_.join();
    }
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    if (epoll_thread_.joinable()) {
        epoll_thread_.join();
    }
    
    std::cout << "HFT TCP Client stopped" << std::endl;
}

ClientStats HFTTCPClient::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void HFTTCPClient::print_stats() const {
    auto stats = get_stats();
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - stats.start_time);
    
    std::cout << "\n=== HFT TCP Client Statistics ===" << std::endl;
    std::cout << "Uptime: " << uptime.count() << " seconds" << std::endl;
    std::cout << "Connection State: " << static_cast<int>(connection_state_.load()) << std::endl;
    std::cout << "Messages Sent: " << stats.messages_sent << std::endl;
    std::cout << "Messages Received: " << stats.messages_received << std::endl;
    std::cout << "Bytes Sent: " << stats.bytes_sent << std::endl;
    std::cout << "Bytes Received: " << stats.bytes_received << std::endl;
    std::cout << "Connection Attempts: " << stats.connection_attempts << std::endl;
    std::cout << "Reconnection Attempts: " << stats.reconnection_attempts << std::endl;
    std::cout << "Errors: " << stats.errors << std::endl;
    
    if (stats.messages_received > 0) {
        std::cout << "\n--- Latency Statistics ---" << std::endl;
        std::cout << "Average Latency: " << std::fixed << std::setprecision(2) 
                  << stats.avg_latency_us << " μs" << std::endl;
        std::cout << "Min Latency: " << stats.min_latency_ns / 1000.0 << " μs" << std::endl;
        std::cout << "Max Latency: " << stats.max_latency_ns / 1000.0 << " μs" << std::endl;
    }
    
    std::cout << "===============================" << std::endl;
}

void HFTTCPClient::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = ClientStats{};
    stats_.start_time = std::chrono::steady_clock::now();
    stats_.last_message_time = stats_.start_time;
    
    {
        std::lock_guard<std::mutex> latency_lock(latency_mutex_);
        latency_measurements_.clear();
    }
}

void HFTTCPClient::set_auto_reconnect(bool enable, uint32_t reconnect_interval_ms) {
    auto_reconnect_.store(enable);
    reconnect_interval_ms_.store(reconnect_interval_ms);
}

void HFTTCPClient::set_heartbeat_interval(uint32_t interval_ms) {
    heartbeat_interval_ms_.store(interval_ms);
}

OrderMessage HFTTCPClient::create_test_order(const std::string& symbol, OrderSide side, 
                                           uint32_t quantity, uint64_t price) {
    OrderMessage order;
    order.message_id = message_id_dist_(gen_);
    order.update_timestamp();
    order.side = side;
    order.order_type = OrderType::LIMIT;
    order.time_in_force = TimeInForce::DAY;
    order.quantity = quantity;
    order.price = price;
    order.client_order_id = message_id_dist_(gen_);
    
    // Set symbol
    std::strncpy(order.symbol.data(), symbol.c_str(), order.symbol.size() - 1);
    order.symbol[order.symbol.size() - 1] = '\0';
    
    return order;
}

MarketDataMessage HFTTCPClient::create_test_market_data(const std::string& symbol, 
                                                       uint64_t bid_price, uint32_t bid_size,
                                                       uint64_t ask_price, uint32_t ask_size) {
    MarketDataMessage market_data;
    market_data.message_id = message_id_dist_(gen_);
    market_data.update_timestamp();
    market_data.bid_price = bid_price;
    market_data.bid_size = bid_size;
    market_data.ask_price = ask_price;
    market_data.ask_size = ask_size;
    market_data.last_price = (bid_price + ask_price) / 2;
    market_data.last_size = (bid_size + ask_size) / 2;
    market_data.volume = quantity_dist_(gen_);
    market_data.high_price = ask_price;
    market_data.low_price = bid_price;
    
    // Set symbol
    std::strncpy(market_data.symbol.data(), symbol.c_str(), market_data.symbol.size() - 1);
    market_data.symbol[market_data.symbol.size() - 1] = '\0';
    
    return market_data;
}

void HFTTCPClient::receive_thread_func() {
    std::cout << "Receive thread started" << std::endl;
    
    while (running_.load()) {
        if (connection_state_.load() != ConnectionState::CONNECTED) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        ssize_t bytes_received = recv(socket_fd_, recv_buffer_.data(), recv_buffer_.size(), MSG_DONTWAIT);
        
        if (bytes_received > 0) {
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.bytes_received += bytes_received;
                stats_.last_message_time = std::chrono::steady_clock::now();
            }
            
            process_received_data(recv_buffer_.data(), bytes_received);
        } else if (bytes_received == 0) {
            // Server disconnected
            std::cout << "Server disconnected" << std::endl;
            handle_disconnection();
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "Receive error: " << strerror(errno) << std::endl;
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.errors++;
            }
            handle_disconnection();
        }
        
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    std::cout << "Receive thread stopped" << std::endl;
}

void HFTTCPClient::send_thread_func() {
    std::cout << "Send thread started" << std::endl;
    
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(send_queue_mutex_);
        send_queue_cv_.wait(lock, [this] { return !send_queue_.empty() || !running_.load(); });
        
        if (!running_.load()) {
            break;
        }
        
        while (!send_queue_.empty() && connection_state_.load() == ConnectionState::CONNECTED) {
            Message msg = send_queue_.front();
            send_queue_.pop();
            lock.unlock();
            
            if (send_data(&msg, sizeof(msg))) {
                {
                    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                    stats_.messages_sent++;
                    stats_.bytes_sent += sizeof(msg);
                }
            } else {
                {
                    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                    stats_.errors++;
                }
                handle_disconnection();
            }
            
            lock.lock();
        }
    }
    
    std::cout << "Send thread stopped" << std::endl;
}

void HFTTCPClient::heartbeat_thread_func() {
    std::cout << "Heartbeat thread started" << std::endl;
    
    while (running_.load()) {
        if (connection_state_.load() == ConnectionState::CONNECTED) {
            send_heartbeat();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeat_interval_ms_.load()));
    }
    
    std::cout << "Heartbeat thread stopped" << std::endl;
}

void HFTTCPClient::epoll_thread_func() {
    std::cout << "Epoll thread started" << std::endl;
    
    // Create epoll instance
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        std::cerr << "Failed to create epoll: " << strerror(errno) << std::endl;
        return;
    }
    
    std::vector<epoll_event> events(1024);
    
    while (running_.load()) {
        if (connection_state_.load() == ConnectionState::CONNECTED && socket_fd_ != -1) {
            // Add socket to epoll
            epoll_event ev{};
            ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
            ev.data.fd = socket_fd_;
            
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket_fd_, &ev) == -1) {
                if (errno != EEXIST) {
                    std::cerr << "Failed to add socket to epoll: " << strerror(errno) << std::endl;
                }
            }
        }
        
        int nfds = epoll_wait(epoll_fd_, events.data(), events.size(), 100); // 100ms timeout
        
        if (nfds > 0) {
            for (int i = 0; i < nfds; ++i) {
                if (events[i].data.fd == socket_fd_) {
                    if (events[i].events & EPOLLIN) {
                        // Data available for reading
                        ssize_t bytes_received = recv(socket_fd_, recv_buffer_.data(), recv_buffer_.size(), MSG_DONTWAIT);
                        if (bytes_received > 0) {
                            {
                                std::lock_guard<std::mutex> lock(stats_mutex_);
                                stats_.bytes_received += bytes_received;
                                stats_.last_message_time = std::chrono::steady_clock::now();
                            }
                            process_received_data(recv_buffer_.data(), bytes_received);
                        }
                    }
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
    
    std::cout << "Epoll thread stopped" << std::endl;
}

void HFTTCPClient::handle_disconnection() {
    if (connection_state_.load() == ConnectionState::DISCONNECTED) {
        return;
    }
    
    connection_state_.store(ConnectionState::DISCONNECTED);
    
    if (socket_fd_ != -1) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    
    if (auto_reconnect_.load()) {
        connection_state_.store(ConnectionState::RECONNECTING);
        attempt_reconnection();
    }
}

void HFTTCPClient::attempt_reconnection() {
    std::cout << "Attempting reconnection..." << std::endl;
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.reconnection_attempts++;
    }
    
    if (connect(5000)) {
        std::cout << "Reconnection successful" << std::endl;
    } else {
        std::cout << "Reconnection failed, will retry in " << reconnect_interval_ms_.load() << "ms" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_interval_ms_.load()));
        
        if (auto_reconnect_.load()) {
            attempt_reconnection();
        }
    }
}

void HFTTCPClient::process_received_data(const char* data, size_t size) {
    size_t offset = 0;
    
    while (offset + sizeof(Message) <= size) {
        const Message* msg = reinterpret_cast<const Message*>(data + offset);
        
        // Calculate latency if this is a response
        auto receive_time = std::chrono::high_resolution_clock::now();
        auto receive_ns = receive_time.time_since_epoch().count();
        
        // Simple latency calculation (in real implementation, you'd match with send time)
        uint64_t latency_ns = 0;
        if (msg->timestamp > 0) {
            latency_ns = receive_ns - msg->timestamp;
            update_latency_stats(latency_ns);
        }
        
        process_message(*msg);
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.messages_received++;
        }
        
        offset += sizeof(Message);
    }
}

void HFTTCPClient::process_message(const Message& msg) {
    if (message_handler_) {
        message_handler_(msg);
    }
    
    // Process specific message types
    switch (msg.message_type) {
        case MessageType::ORDER_NEW:
        case MessageType::ORDER_CANCEL:
        case MessageType::ORDER_REPLACE:
        case MessageType::ORDER_FILL:
        case MessageType::ORDER_REJECT:
            if (msg.payload_size >= sizeof(OrderMessage)) {
                const OrderMessage& order = *reinterpret_cast<const OrderMessage*>(msg.payload.data());
                process_order_message(order);
            }
            break;
            
        case MessageType::MARKET_DATA:
            if (msg.payload_size >= sizeof(MarketDataMessage)) {
                const MarketDataMessage& market_data = *reinterpret_cast<const MarketDataMessage*>(msg.payload.data());
                process_market_data_message(market_data);
            }
            break;
            
        case MessageType::HEARTBEAT:
            // Heartbeat received
            break;
            
        default:
            break;
    }
}

void HFTTCPClient::process_order_message(const OrderMessage& order) {
    if (order_handler_) {
        order_handler_(order);
    }
    
    std::cout << "Order received: " << order.symbol.data() 
              << " " << (order.side == OrderSide::BUY ? "BUY" : "SELL")
              << " " << order.quantity << " @ " << order.price << std::endl;
}

void HFTTCPClient::process_market_data_message(const MarketDataMessage& market_data) {
    if (market_data_handler_) {
        market_data_handler_(market_data);
    }
    
    std::cout << "Market data received: " << market_data.symbol.data()
              << " Bid: " << market_data.bid_price << " Ask: " << market_data.ask_price << std::endl;
}

void HFTTCPClient::setup_socket_options(int sock_fd) {
    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 1 for low latency
    int tcp_nodelay = 1;
    setsockopt(sock_fd, IPPROTO_TCP, 1, &tcp_nodelay, sizeof(tcp_nodelay));
    
    setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
    
    // Set buffer sizes
    int send_buf_size = 1024 * 1024; // 1MB
    int recv_buf_size = 1024 * 1024; // 1MB
    setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, &send_buf_size, sizeof(send_buf_size));
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size));
}

void HFTTCPClient::set_non_blocking(int sock_fd) {
    int flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
}

bool HFTTCPClient::send_data(const void* data, size_t size) {
    if (socket_fd_ == -1 || connection_state_.load() != ConnectionState::CONNECTED) {
        return false;
    }
    
    ssize_t bytes_sent = send(socket_fd_, data, size, MSG_NOSIGNAL);
    return bytes_sent == static_cast<ssize_t>(size);
}

void HFTTCPClient::update_latency_stats(uint64_t latency_ns) {
    {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        latency_measurements_.push_back(latency_ns);
        
        // Keep only last 10000 measurements
        if (latency_measurements_.size() > 10000) {
            latency_measurements_.erase(latency_measurements_.begin());
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_latency_ns += latency_ns;
        
        if (latency_ns < stats_.min_latency_ns) {
            stats_.min_latency_ns = latency_ns;
        }
        
        if (latency_ns > stats_.max_latency_ns) {
            stats_.max_latency_ns = latency_ns;
        }
        
        calculate_average_latency();
    }
}

void HFTTCPClient::calculate_average_latency() {
    if (stats_.messages_received > 0) {
        stats_.avg_latency_us = (stats_.total_latency_ns / 1000.0) / stats_.messages_received;
    }
}

} // namespace hft
