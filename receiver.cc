
#include <iostream>
#include <chrono>
#include <cstring>
#include <fstream>
#include "udp-socket.hh"
#include "receiver.hh"

std::ofstream log_file("receiver_log.txt"); // Log file for receiver activity

// Initialize receiver by binding to a specific port
bool initialize_receiver(UDPSocket& socket, int port) {
    if (socket.bindsocket(port) == 0) { // Using bindsocket instead of bind
        std::cout << "Receiver initialized and listening on port " << port << "..." << std::endl;
        return true;
    } else {
        std::cerr << "Failed to initialize receiver: binding to port failed." << std::endl;
        return false;
    }
}

// Log packet details for debugging purposes, including inter-arrival times
void log_received_packet(const Packet& packet, int packet_size, long inter_arrival_time) {
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(packet.receive_time.time_since_epoch()).count();
    log_file << "[Packet] Time(ms): " << now_ms 
             << ", Seq Number: " << packet.seq_number 
             << ", Packet Size(bytes): " << packet_size
             << ", Inter-arrival Time(ms): " << inter_arrival_time << std::endl;
    std::cout << "Received packet of size " << packet_size << " bytes at " << now_ms 
              << " ms, seq_number: " << packet.seq_number 
              << ", Inter-arrival Time(ms): " << inter_arrival_time << std::endl;
}


// Log packet details for debugging purposes
// void log_received_packet(const Packet& packet) {
//     auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(packet.receive_time.time_since_epoch()).count();
//     std::cout << "Received packet of size " << BUFFER_SIZE << " bytes at " << now_ms 
//               << " ms, seq_number: " << packet.seq_number << std::endl;
// }

int main(int argc, char *argv[]) {
    // Command-line arguments
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <Port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);

    // UDP socket setup
    UDPSocket socket;
    if (!initialize_receiver(socket, port)) {
        return 1;
    }

    char buffer[BUFFER_SIZE];
    int seq_number = 0;
    UDPSocket::SockAddress other_addr;

    auto last_receive_time = std::chrono::steady_clock::now();

    while (true) {
        // Receive packets
        int received;
        try {
            received = socket.receivedata(buffer, BUFFER_SIZE, -1, other_addr);
        } catch (const std::exception& e) {
            std::cerr << "Failed to receive packet: " << e.what() << std::endl;
            continue; // Continue listening even after a receive failure
        }

        // Populate the Packet structure
        Packet packet;
        strncpy(packet.data, buffer, received);
        packet.seq_number = seq_number++;
        packet.receive_time = std::chrono::steady_clock::now();

        // Calculate inter-arrival time
        auto inter_arrival_time = std::chrono::duration_cast<std::chrono::milliseconds>(packet.receive_time - last_receive_time).count();
        last_receive_time = packet.receive_time;

        // Log received packet for verification
        log_received_packet(packet, received, inter_arrival_time);
    }

    log_file.close();
    return 0;
}