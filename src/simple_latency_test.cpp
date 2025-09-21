#include "message.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

namespace hft {

class SimpleLatencyTest {
public:
    SimpleLatencyTest(const std::string& server_ip = "127.0.0.1", uint16_t server_port = 8888)
        : server_ip_(server_ip), server_port_(server_port) {}
    
    bool connect() {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ == -1) return false;
        
        int opt = 1;
        setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        setsockopt(socket_fd_, IPPROTO_TCP, 1, &opt, sizeof(opt)); // TCP_NODELAY
        
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port_);
        inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr);
        
        return ::connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == 0;
    }
    
    void run_test(size_t num_messages) {
        std::cout << "\n=== HFT Latency Test ===" << std::endl;
        std::cout << "Messages: " << num_messages << std::endl;
        std::cout << "Mode: Round-trip latency measurement" << std::endl;
        std::cout << "===============================" << std::endl;
        
        std::vector<uint64_t> latencies;
        latencies.reserve(num_messages);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < num_messages; ++i) {
            // Create test message
            Message msg;
            msg.message_id = i + 1;
            msg.update_timestamp();
            msg.message_type = MessageType::HEARTBEAT;
            msg.status = MessageStatus::PENDING;
            msg.source_id = 1;
            msg.destination_id = 0;
            msg.payload_size = 0;
            
            auto send_time = std::chrono::high_resolution_clock::now();
            
            // Send message
            ssize_t sent = send(socket_fd_, &msg, sizeof(msg), MSG_NOSIGNAL);
            if (sent != sizeof(msg)) {
                std::cerr << "Send failed" << std::endl;
                continue;
            }
            
            // Receive response
            Message response;
            ssize_t received = recv(socket_fd_, &response, sizeof(response), 0);
            if (received != sizeof(response)) {
                std::cerr << "Receive failed" << std::endl;
                continue;
            }
            
            auto receive_time = std::chrono::high_resolution_clock::now();
            
            // Calculate latency
            uint64_t latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                receive_time - send_time).count();
            latencies.push_back(latency_ns);
            
            if (i % 1000 == 0 && i > 0) {
                std::cout << "Processed " << i << " messages..." << std::endl;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "\nAll " << num_messages << " messages processed in " << total_duration.count() << "ms" << std::endl;
        std::cout << "Processing rate: " << (num_messages * 1000.0 / total_duration.count()) << " msg/sec" << std::endl;
        
        // Calculate statistics
        calculate_statistics(latencies);
    }
    
    void calculate_statistics(const std::vector<uint64_t>& latencies) {
        if (latencies.empty()) {
            std::cout << "No latency measurements available" << std::endl;
            return;
        }
        
        std::vector<uint64_t> sorted_latencies = latencies;
        std::sort(sorted_latencies.begin(), sorted_latencies.end());
        
        uint64_t min_latency_ns = sorted_latencies.front();
        uint64_t max_latency_ns = sorted_latencies.back();
        uint64_t total_latency_ns = std::accumulate(latencies.begin(), latencies.end(), 0ULL);
        double avg_latency_ns = static_cast<double>(total_latency_ns) / latencies.size();
        
        size_t size = sorted_latencies.size();
        uint64_t p50 = sorted_latencies[size * 0.50];
        uint64_t p90 = sorted_latencies[size * 0.90];
        uint64_t p95 = sorted_latencies[size * 0.95];
        uint64_t p99 = sorted_latencies[size * 0.99];
        uint64_t p99_9 = sorted_latencies[size * 0.999];
        
        std::cout << "\n=== LATENCY STATISTICS ===" << std::endl;
        std::cout << "Total Messages: " << latencies.size() << std::endl;
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
    
    void disconnect() {
        if (socket_fd_ != -1) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
    }

private:
    std::string server_ip_;
    uint16_t server_port_;
    int socket_fd_{-1};
};

} // namespace hft

int main(int argc, char* argv[]) {
    using namespace hft;
    
    size_t num_messages = 10000;
    
    if (argc > 1) {
        num_messages = std::stoul(argv[1]);
    }
    
    std::cout << "=== HFT Simple Latency Test ===" << std::endl;
    std::cout << "Messages: " << num_messages << std::endl;
    std::cout << "=============================" << std::endl;
    
    SimpleLatencyTest test;
    
    if (!test.connect()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }
    
    test.run_test(num_messages);
    test.disconnect();
    
    std::cout << "\nLatency test completed!" << std::endl;
    return 0;
}
