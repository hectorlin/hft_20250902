#include "latency_client.h"
#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>

namespace hft {

// Global client instance for signal handling
LatencyTestClient* g_client = nullptr;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    if (g_client && signal == SIGINT) {
        std::cout << "\nReceived SIGINT, stopping test..." << std::endl;
        g_client->disconnect();
    }
}

} // namespace hft

int main(int argc, char* argv[]) {
    using namespace hft;
    
    // Default configuration
    std::string server_ip = "127.0.0.1";
    uint16_t server_port = 8888;
    std::string test_type = "latency";
    size_t num_messages = 1000;
    uint32_t message_interval_ms = 1;
    size_t burst_size = 100;
    size_t num_bursts = 10;
    uint32_t burst_interval_ms = 100;
    uint32_t duration_seconds = 60;
    uint32_t messages_per_second = 1000;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ip" && i + 1 < argc) {
            server_ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            server_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--test" && i + 1 < argc) {
            test_type = argv[++i];
        } else if (arg == "--messages" && i + 1 < argc) {
            num_messages = std::stoul(argv[++i]);
        } else if (arg == "--interval" && i + 1 < argc) {
            message_interval_ms = std::stoul(argv[++i]);
        } else if (arg == "--burst-size" && i + 1 < argc) {
            burst_size = std::stoul(argv[++i]);
        } else if (arg == "--bursts" && i + 1 < argc) {
            num_bursts = std::stoul(argv[++i]);
        } else if (arg == "--burst-interval" && i + 1 < argc) {
            burst_interval_ms = std::stoul(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            duration_seconds = std::stoul(argv[++i]);
        } else if (arg == "--rate" && i + 1 < argc) {
            messages_per_second = std::stoul(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "HFT Latency Test Client\n"
                      << "Usage: " << argv[0] << " [options]\n\n"
                      << "Options:\n"
                      << "  --ip <ip>              Server IP address (default: 127.0.0.1)\n"
                      << "  --port <port>          Server port (default: 8888)\n"
                      << "  --test <type>          Test type: latency, burst, sustained (default: latency)\n"
                      << "  --messages <n>         Number of messages for latency test (default: 1000)\n"
                      << "  --interval <ms>        Message interval in ms (default: 1)\n"
                      << "  --burst-size <n>       Burst size for burst test (default: 100)\n"
                      << "  --bursts <n>           Number of bursts (default: 10)\n"
                      << "  --burst-interval <ms>  Interval between bursts (default: 100)\n"
                      << "  --duration <s>         Duration for sustained test (default: 60)\n"
                      << "  --rate <msg/s>         Messages per second for sustained test (default: 1000)\n"
                      << "  --help                 Show this help message\n\n"
                      << "Examples:\n"
                      << "  " << argv[0] << " --test latency --messages 5000 --interval 0\n"
                      << "  " << argv[0] << " --test burst --burst-size 200 --bursts 20\n"
                      << "  " << argv[0] << " --test sustained --duration 120 --rate 2000\n";
            return 0;
        }
    }
    
    std::cout << "=== HFT Latency Test Client ===" << std::endl;
    std::cout << "Server: " << server_ip << ":" << server_port << std::endl;
    std::cout << "Test type: " << test_type << std::endl;
    std::cout << "===============================" << std::endl;
    
    // Create client
    LatencyTestClient client(server_ip, server_port);
    g_client = &client;
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Connect to server
    if (!client.connect()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }
    
    // Wait a moment for connection to stabilize
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    try {
        // Run the specified test
        if (test_type == "latency") {
            std::cout << "Running latency test..." << std::endl;
            client.run_latency_test(num_messages, message_interval_ms);
        } else if (test_type == "burst") {
            std::cout << "Running burst test..." << std::endl;
            client.run_burst_test(burst_size, num_bursts, burst_interval_ms);
        } else if (test_type == "sustained") {
            std::cout << "Running sustained load test..." << std::endl;
            client.run_sustained_test(duration_seconds, messages_per_second);
        } else {
            std::cerr << "Unknown test type: " << test_type << std::endl;
            std::cerr << "Valid types: latency, burst, sustained" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    
    // Disconnect
    client.disconnect();
    
    std::cout << "\nLatency test completed successfully!" << std::endl;
    return 0;
}

