#include "hft_tcp_client.h"
#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>
#include <iomanip>

namespace hft {

// Global client instance for signal handling
HFTTCPClient* g_client = nullptr;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    if (g_client && signal == SIGINT) {
        std::cout << "\nReceived SIGINT, stopping client..." << std::endl;
        g_client->stop();
        g_client->disconnect();
    }
}

} // namespace hft

int main(int argc, char* argv[]) {
    using namespace hft;
    
    // Default configuration
    std::string server_ip = "127.0.0.1";
    uint16_t server_port = 8888;
    uint32_t client_id = 1;
    std::string test_mode = "interactive";
    size_t num_messages = 1000;
    uint32_t message_interval_ms = 1;
    uint32_t test_duration_seconds = 60;
    uint32_t messages_per_second = 100;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ip" && i + 1 < argc) {
            server_ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            server_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--client-id" && i + 1 < argc) {
            client_id = std::stoul(argv[++i]);
        } else if (arg == "--mode" && i + 1 < argc) {
            test_mode = argv[++i];
        } else if (arg == "--messages" && i + 1 < argc) {
            num_messages = std::stoul(argv[++i]);
        } else if (arg == "--interval" && i + 1 < argc) {
            message_interval_ms = std::stoul(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            test_duration_seconds = std::stoul(argv[++i]);
        } else if (arg == "--rate" && i + 1 < argc) {
            messages_per_second = std::stoul(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "HFT TCP Client Test\n"
                      << "Usage: " << argv[0] << " [options]\n\n"
                      << "Options:\n"
                      << "  --ip <ip>              Server IP address (default: 127.0.0.1)\n"
                      << "  --port <port>          Server port (default: 8888)\n"
                      << "  --client-id <id>       Client ID (default: 1)\n"
                      << "  --mode <mode>          Test mode: interactive, latency, burst, sustained (default: interactive)\n"
                      << "  --messages <n>         Number of messages for latency test (default: 1000)\n"
                      << "  --interval <ms>        Message interval in ms (default: 1)\n"
                      << "  --duration <s>         Duration for sustained test (default: 60)\n"
                      << "  --rate <msg/s>         Messages per second for sustained test (default: 100)\n"
                      << "  --help                 Show this help message\n\n"
                      << "Examples:\n"
                      << "  " << argv[0] << " --mode interactive\n"
                      << "  " << argv[0] << " --mode latency --messages 5000 --interval 0\n"
                      << "  " << argv[0] << " --mode burst --messages 100 --interval 0\n"
                      << "  " << argv[0] << " --mode sustained --duration 120 --rate 200\n";
            return 0;
        }
    }
    
    std::cout << "=== HFT TCP Client Test ===" << std::endl;
    std::cout << "Server: " << server_ip << ":" << server_port << std::endl;
    std::cout << "Client ID: " << client_id << std::endl;
    std::cout << "Test Mode: " << test_mode << std::endl;
    std::cout << "=========================" << std::endl;
    
    // Create client
    HFTTCPClient client(server_ip, server_port, client_id);
    g_client = &client;
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Set up message handlers
    client.set_message_handler([](const Message& msg) {
        std::cout << "Message received: Type=" << static_cast<int>(msg.message_type) 
                  << " ID=" << msg.message_id << " Size=" << msg.payload_size << std::endl;
    });
    
    client.set_order_handler([](const OrderMessage& order) {
        std::cout << "Order received: " << order.symbol.data() 
                  << " " << (order.side == OrderSide::BUY ? "BUY" : "SELL")
                  << " " << order.quantity << " @ " << order.price << std::endl;
    });
    
    client.set_market_data_handler([](const MarketDataMessage& market_data) {
        std::cout << "Market data received: " << market_data.symbol.data()
                  << " Bid: " << market_data.bid_price << " Ask: " << market_data.ask_price << std::endl;
    });
    
    // Connect to server
    if (!client.connect()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }
    
    // Start client
    client.start();
    
    // Wait a moment for connection to stabilize
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    try {
        if (test_mode == "interactive") {
            std::cout << "\n=== Interactive Mode ===" << std::endl;
            std::cout << "Commands:" << std::endl;
            std::cout << "  o <symbol> <side> <quantity> <price> - Send order" << std::endl;
            std::cout << "  m <symbol> <bid> <ask> - Send market data" << std::endl;
            std::cout << "  h - Send heartbeat" << std::endl;
            std::cout << "  s - Show statistics" << std::endl;
            std::cout << "  q - Quit" << std::endl;
            std::cout << "=========================" << std::endl;
            
            std::string command;
            while (std::getline(std::cin, command)) {
                if (command.empty()) continue;
                
                if (command == "q" || command == "quit") {
                    break;
                } else if (command == "h" || command == "heartbeat") {
                    client.send_heartbeat();
                    std::cout << "Heartbeat sent" << std::endl;
                } else if (command == "s" || command == "stats") {
                    client.print_stats();
                } else if (command.substr(0, 2) == "o ") {
                    // Parse order command: o AAPL BUY 100 150000
                    std::istringstream iss(command.substr(2));
                    std::string symbol, side_str;
                    uint32_t quantity;
                    uint64_t price;
                    
                    if (iss >> symbol >> side_str >> quantity >> price) {
                        OrderSide side = (side_str == "SELL") ? OrderSide::SELL : OrderSide::BUY;
                        OrderMessage order = client.create_test_order(symbol, side, quantity, price);
                        if (client.send_order(order)) {
                            std::cout << "Order sent: " << symbol << " " << side_str 
                                      << " " << quantity << " @ " << price << std::endl;
                        } else {
                            std::cout << "Failed to send order" << std::endl;
                        }
                    } else {
                        std::cout << "Invalid order format. Use: o <symbol> <side> <quantity> <price>" << std::endl;
                    }
                } else if (command.substr(0, 2) == "m ") {
                    // Parse market data command: m AAPL 149900 150100
                    std::istringstream iss(command.substr(2));
                    std::string symbol;
                    uint64_t bid_price, ask_price;
                    
                    if (iss >> symbol >> bid_price >> ask_price) {
                        MarketDataMessage market_data = client.create_test_market_data(symbol, bid_price, 100, ask_price, 100);
                        if (client.send_market_data(market_data)) {
                            std::cout << "Market data sent: " << symbol << " Bid: " << bid_price 
                                      << " Ask: " << ask_price << std::endl;
                        } else {
                            std::cout << "Failed to send market data" << std::endl;
                        }
                    } else {
                        std::cout << "Invalid market data format. Use: m <symbol> <bid> <ask>" << std::endl;
                    }
                } else {
                    std::cout << "Unknown command: " << command << std::endl;
                }
            }
            
        } else if (test_mode == "latency") {
            std::cout << "\n=== Latency Test ===" << std::endl;
            std::cout << "Messages: " << num_messages << std::endl;
            std::cout << "Interval: " << message_interval_ms << "ms" << std::endl;
            std::cout << "===================" << std::endl;
            
            client.reset_stats();
            
            for (size_t i = 0; i < num_messages; ++i) {
                OrderMessage order = client.create_test_order("AAPL", OrderSide::BUY, 100, 150000 + i);
                client.send_order(order);
                
                if (message_interval_ms > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(message_interval_ms));
                }
            }
            
            // Wait for responses
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            client.print_stats();
            
        } else if (test_mode == "burst") {
            std::cout << "\n=== Burst Test ===" << std::endl;
            std::cout << "Messages: " << num_messages << std::endl;
            std::cout << "=================" << std::endl;
            
            client.reset_stats();
            
            // Send all messages as fast as possible
            for (size_t i = 0; i < num_messages; ++i) {
                OrderMessage order = client.create_test_order("AAPL", OrderSide::BUY, 100, 150000 + i);
                client.send_order(order);
            }
            
            // Wait for responses
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            client.print_stats();
            
        } else if (test_mode == "sustained") {
            std::cout << "\n=== Sustained Load Test ===" << std::endl;
            std::cout << "Duration: " << test_duration_seconds << " seconds" << std::endl;
            std::cout << "Rate: " << messages_per_second << " msg/s" << std::endl;
            std::cout << "===========================" << std::endl;
            
            client.reset_stats();
            
            auto start_time = std::chrono::steady_clock::now();
            auto end_time = start_time + std::chrono::seconds(test_duration_seconds);
            
            uint32_t message_interval_us = 1000000 / messages_per_second; // microseconds between messages
            size_t message_count = 0;
            
            while (std::chrono::steady_clock::now() < end_time) {
                OrderMessage order = client.create_test_order("AAPL", OrderSide::BUY, 100, 150000 + message_count);
                client.send_order(order);
                message_count++;
                
                std::this_thread::sleep_for(std::chrono::microseconds(message_interval_us));
            }
            
            // Wait for final responses
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            client.print_stats();
            
        } else {
            std::cerr << "Unknown test mode: " << test_mode << std::endl;
            std::cerr << "Valid modes: interactive, latency, burst, sustained" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    
    // Stop and disconnect
    client.stop();
    client.disconnect();
    
    std::cout << "\nHFT TCP Client test completed successfully!" << std::endl;
    return 0;
}
