#ifndef LATENCY_CLIENT_H
#define LATENCY_CLIENT_H

#include "message.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <random>
#include <algorithm>
#include <iomanip>
#include <mutex>

namespace hft {

/**
 * @brief Latency test client for HFT server
 */
class LatencyTestClient {
public:
    LatencyTestClient(const std::string& server_ip = "127.0.0.1", uint16_t server_port = 8888);
    ~LatencyTestClient();
    
    /**
     * @brief Connect to the server
     */
    bool connect();
    
    /**
     * @brief Disconnect from the server
     */
    void disconnect();
    
    /**
     * @brief Run latency test
     * @param num_messages Number of messages to send
     * @param message_interval_ms Interval between messages in milliseconds
     */
    void run_latency_test(size_t num_messages = 1000, uint32_t message_interval_ms = 1);
    
    /**
     * @brief Run burst test
     * @param burst_size Number of messages in each burst
     * @param num_bursts Number of bursts to send
     * @param burst_interval_ms Interval between bursts in milliseconds
     */
    void run_burst_test(size_t burst_size = 100, size_t num_bursts = 10, uint32_t burst_interval_ms = 100);
    
    /**
     * @brief Run sustained load test
     * @param duration_seconds Test duration in seconds
     * @param messages_per_second Target messages per second
     */
    void run_sustained_test(uint32_t duration_seconds = 60, uint32_t messages_per_second = 1000);
    
    /**
     * @brief Get test statistics
     */
    struct TestStats {
        uint64_t total_messages_sent;
        uint64_t total_messages_received;
        uint64_t total_latency_ns;
        uint64_t min_latency_ns;
        uint64_t max_latency_ns;
        double avg_latency_us;
        double p50_latency_us;
        double p95_latency_us;
        double p99_latency_us;
        double p99_9_latency_us;
        uint64_t errors;
        double throughput_mps;
    };
    
    TestStats get_stats() const;
    void print_stats() const;
    void reset_stats();

private:
    void send_message(const Message& msg);
    void receive_responses();
    void update_latency_stats(uint64_t latency_ns);
    void calculate_percentiles() const;
    Message create_test_message();
    void setup_socket_options(int sock_fd);
    void set_non_blocking(int sock_fd);
    
    // Configuration
    std::string server_ip_;
    uint16_t server_port_;
    int socket_fd_;
    bool connected_;
    
    // Test data
    std::atomic<uint64_t> messages_sent_{0};
    std::atomic<uint64_t> messages_received_{0};
    std::atomic<uint64_t> total_latency_ns_{0};
    std::atomic<uint64_t> min_latency_ns_{UINT64_MAX};
    std::atomic<uint64_t> max_latency_ns_{0};
    std::atomic<uint64_t> errors_{0};
    
    // Latency measurements
    std::vector<uint64_t> latency_measurements_;
    mutable std::mutex latency_mutex_;
    
    // Percentile calculations
    mutable double p50_latency_us{0.0};
    mutable double p95_latency_us{0.0};
    mutable double p99_latency_us{0.0};
    mutable double p99_9_latency_us{0.0};
    
    // Threading
    std::thread receiver_thread_;
    std::atomic<bool> stop_receiver_{false};
    
    // Random number generation
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_int_distribution<uint64_t> message_id_dist_;
    std::uniform_int_distribution<uint32_t> quantity_dist_;
    std::uniform_int_distribution<uint64_t> price_dist_;
    
    // Test symbols
    std::vector<std::string> test_symbols_;
};

} // namespace hft

#endif // LATENCY_CLIENT_H
