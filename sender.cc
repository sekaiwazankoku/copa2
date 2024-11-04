#include <iostream>
#include <chrono>
#include <thread>
#include "udp-socket.hh"
#include "sender.hh"

#define PACKET_SIZE 1500

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

    // Calculating burst rate and packet transmission delay
    double burst_rate = static_cast<double>(burst_size) / burst_duration; // bytes per ms
    double burst_pkt_tx_delay = PACKET_SIZE / burst_rate; // using PACKET_SIZE defined as 1500 bytes by default

    // UDP socket setup
    UDPSocket socket;
    socket.connect(Address(target_ip, target_port));

    // Variables for controlling burst timing
    auto last_burst_time = std::chrono::steady_clock::now();
    auto last_send_time = std::chrono::steady_clock::now();
    int total_bytes_sent = 0;
    bool send_burst = false;

    while (true) {
        auto now = std::chrono::steady_clock::now();

        // Check if it's time to start a new burst
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_burst_time).count() > inter_burst_time) {
            send_burst = true;
        }

        // Send packets within the burst
        if (send_burst && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_send_time).count() > burst_pkt_tx_delay) {
            std::string packet(PACKET_SIZE, 'X'); // Packet of size PACKET_SIZE bytes
            socket.send(packet);

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

