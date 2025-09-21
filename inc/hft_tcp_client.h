#ifndef HFT_TCP_CLIENT_H
#define HFT_TCP_CLIENT_H

#include "message.h"

#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <random>
#include <algorithm>
#include <iomanip>
#include <unordered_map>

namespace hft {

/**
 * @brief Connection state for the client
 */
enum class ConnectionState : uint8_t {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    RECONNECTING = 3,
    ERROR = 4
};

/**
 * @brief Message handler callback type
 */
using MessageHandler = std::function<void(const Message&)>;
using OrderMessageHandler = std::function<void(const OrderMessage&)>;
using MarketDataHandler = std::function<void(const MarketDataMessage&)>;

/**
 * @brief Client statistics structure
 */
struct ClientStats {
    uint64_t messages_sent{0};
    uint64_t messages_received{0};
    uint64_t bytes_sent{0};
    uint64_t bytes_received{0};
    uint64_t connection_attempts{0};
    uint64_t reconnection_attempts{0};
    uint64_t errors{0};
    uint64_t min_latency_ns{UINT64_MAX};
    uint64_t max_latency_ns{0};
    uint64_t total_latency_ns{0};
    double avg_latency_us{0.0};
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_message_time;
};

/**
 * @brief High-performance HFT TCP Client with epoll and multi-threading
 */
class HFTTCPClient {
public:
    /**
     * @brief Constructor
     * @param server_ip Server IP address
     * @param server_port Server port
     * @param client_id Unique client identifier
     */
    HFTTCPClient(const std::string& server_ip = "127.0.0.1", 
                 uint16_t server_port = 8888,
                 uint32_t client_id = 1);
    
    /**
     * @brief Destructor
     */
    ~HFTTCPClient();
    
    // Delete copy constructor and assignment operator
    HFTTCPClient(const HFTTCPClient&) = delete;
    HFTTCPClient& operator=(const HFTTCPClient&) = delete;
    
    /**
     * @brief Connect to the server
     * @param timeout_ms Connection timeout in milliseconds
     * @return true if connection successful
     */
    bool connect(uint32_t timeout_ms = 5000);
    
    /**
     * @brief Disconnect from the server
     */
    void disconnect();
    
    /**
     * @brief Check if client is connected
     */
    bool is_connected() const;
    
    /**
     * @brief Send a message to the server
     * @param msg Message to send
     * @return true if message queued for sending
     */
    bool send_message(const Message& msg);
    
    /**
     * @brief Send an order message
     * @param order Order message to send
     * @return true if message queued for sending
     */
    bool send_order(const OrderMessage& order);
    
    /**
     * @brief Send a market data message
     * @param market_data Market data message to send
     * @return true if message queued for sending
     */
    bool send_market_data(const MarketDataMessage& market_data);
    
    /**
     * @brief Send a heartbeat message
     * @return true if heartbeat queued for sending
     */
    bool send_heartbeat();
    
    /**
     * @brief Register message handlers
     */
    void set_message_handler(MessageHandler handler);
    void set_order_handler(OrderMessageHandler handler);
    void set_market_data_handler(MarketDataHandler handler);
    
    /**
     * @brief Start the client (starts background threads)
     */
    void start();
    
    /**
     * @brief Stop the client (stops background threads)
     */
    void stop();
    
    /**
     * @brief Get client statistics
     */
    ClientStats get_stats() const;
    
    /**
     * @brief Print statistics
     */
    void print_stats() const;
    
    /**
     * @brief Reset statistics
     */
    void reset_stats();
    
    /**
     * @brief Enable/disable auto-reconnection
     */
    void set_auto_reconnect(bool enable, uint32_t reconnect_interval_ms = 1000);
    
    /**
     * @brief Set heartbeat interval
     */
    void set_heartbeat_interval(uint32_t interval_ms);
    
    /**
     * @brief Create test order message
     */
    OrderMessage create_test_order(const std::string& symbol = "AAPL",
                                  OrderSide side = OrderSide::BUY,
                                  uint32_t quantity = 100,
                                  uint64_t price = 150000);
    
    /**
     * @brief Create test market data message
     */
    MarketDataMessage create_test_market_data(const std::string& symbol = "AAPL",
                                            uint64_t bid_price = 149900,
                                            uint32_t bid_size = 100,
                                            uint64_t ask_price = 150100,
                                            uint32_t ask_size = 100);

private:
    // Thread functions
    void receive_thread_func();
    void send_thread_func();
    void heartbeat_thread_func();
    void epoll_thread_func();
    
    // Connection management
    bool establish_connection();
    void handle_disconnection();
    void attempt_reconnection();
    
    // Message processing
    void process_received_data(const char* data, size_t size);
    void process_message(const Message& msg);
    void process_order_message(const OrderMessage& order);
    void process_market_data_message(const MarketDataMessage& market_data);
    
    // Socket operations
    void setup_socket_options(int sock_fd);
    void set_non_blocking(int sock_fd);
    bool send_data(const void* data, size_t size);
    
    // Statistics
    void update_latency_stats(uint64_t latency_ns);
    void calculate_average_latency();
    
    // Configuration
    std::string server_ip_;
    uint16_t server_port_;
    uint32_t client_id_;
    
    // Connection state
    std::atomic<ConnectionState> connection_state_{ConnectionState::DISCONNECTED};
    int socket_fd_{-1};
    int epoll_fd_{-1};
    
    // Threading
    std::atomic<bool> running_{false};
    std::thread receive_thread_;
    std::thread send_thread_;
    std::thread heartbeat_thread_;
    std::thread epoll_thread_;
    
    // Message queues
    std::queue<Message> send_queue_;
    mutable std::mutex send_queue_mutex_;
    std::condition_variable send_queue_cv_;
    
    // Message handlers
    MessageHandler message_handler_;
    OrderMessageHandler order_handler_;
    MarketDataHandler market_data_handler_;
    
    // Auto-reconnection
    std::atomic<bool> auto_reconnect_{true};
    std::atomic<uint32_t> reconnect_interval_ms_{1000};
    std::atomic<uint32_t> heartbeat_interval_ms_{1000};
    
    // Statistics
    mutable std::mutex stats_mutex_;
    ClientStats stats_;
    
    // Buffers
    static constexpr size_t BUFFER_SIZE = 65536;
    std::vector<char> recv_buffer_;
    std::vector<char> send_buffer_;
    
    // Random number generation for test data
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_int_distribution<uint64_t> message_id_dist_;
    std::uniform_int_distribution<uint32_t> quantity_dist_;
    std::uniform_int_distribution<uint64_t> price_dist_;
    
    // Test symbols
    std::vector<std::string> test_symbols_;
    
    // Latency measurements
    std::vector<uint64_t> latency_measurements_;
    mutable std::mutex latency_mutex_;
    
    // Performance monitoring
    std::chrono::steady_clock::time_point last_stats_time_;
    static constexpr auto STATS_INTERVAL = std::chrono::seconds(5);
};

} // namespace hft

#endif // HFT_TCP_CLIENT_H
