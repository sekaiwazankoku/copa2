#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#include "udp-socket.hh"
#include "sender.hh"

// Initialize the sender by setting up the socket connection
bool initialize_sender(UDPSocket& socket) {
    if (socket.is_valid()) { // Check if the socket descriptor is valid
        std::cout << "Sender socket initialized and ready to send." << std::endl;
        return true;
    } else {
        std::cerr << "Error: Sender socket initialization failed." << std::endl;
        return false;
    }
}

// Function to send a packet via UDP socket
bool send_packet(UDPSocket& socket, const Packet& packet, const std::string& target_ip, int target_port) {
    try {
        socket.senddata(packet.data, PACKET_SIZE, target_ip, target_port); 
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to send packet: " << e.what() << std::endl;
        return false;
    }
}

// Function to calculate burst rate in bytes per ms from burst size and duration
double calculate_burst_rate(int burst_size, int burst_duration) {
    return static_cast<double>(burst_size) / burst_duration;
}

int main(int argc, char *argv[]) {
    // Command-line arguments
    if (argc != 6) {
        std::cerr << "Usage: " << argv[0] << " <IP> <Port> <burst_size> <burst_duration> <inter_burst_time>" << std::endl;
        return 1;
    }

    std::string target_ip = argv[1];
    int target_port = std::stoi(argv[2]);
    int burst_size = std::stoi(argv[3]); // in bytes
    int burst_duration = std::stoi(argv[4]); // in ms
    int inter_burst_time = std::stoi(argv[5]); // in ms

    // UDP socket setup
    UDPSocket socket;
    if (!initialize_sender(socket)) {
        return 1;
    }

    // Calculating burst rate and packet transmission delay
    double burst_rate = calculate_burst_rate(burst_size, burst_duration); // bytes per ms
    double burst_pkt_tx_delay = PACKET_SIZE / burst_rate; // using PACKET_SIZE defined as 1500 bytes by default

    // Variables for controlling burst timing
    auto last_burst_time = std::chrono::steady_clock::now();
    auto last_send_time = std::chrono::steady_clock::now();
    int total_bytes_sent = 0;
    bool send_burst = false;
    int seq_number = 0;

    while (true) {
        auto now = std::chrono::steady_clock::now();

        // Check if it's time to start a new burst
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_burst_time).count() > inter_burst_time) {
            send_burst = true;
        }

        // Send packets within the burst
        if (send_burst && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_send_time).count() > burst_pkt_tx_delay) {
            // Initialize packet directly
            Packet packet;
            strncpy(packet.data, std::string(PACKET_SIZE, 'X').c_str(), PACKET_SIZE);
            packet.seq_number = seq_number++;
            packet.send_time = std::chrono::steady_clock::now();

            if (!send_packet(socket, packet, target_ip, target_port)) { // Attempt to send packet
                std::cerr << "Error in sending packet. Aborting current burst." << std::endl;
                send_burst = false; // Exit the current burst if there's a send failure
                continue;
            }

            total_bytes_sent += PACKET_SIZE;
            last_send_time = now;

            // End burst if burst_size is reached
            if (total_bytes_sent >= burst_size) {
                total_bytes_sent = 0;
                last_burst_time = now;
                send_burst = false;
            }
        }
    }

    return 0;
}

