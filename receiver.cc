
#include <iostream>
#include <chrono>
#include "udp-socket.hh"
#include "receiver.hh"

// Initialize receiver by binding to a specific port
bool initialize_receiver(UDPSocket& socket, int port) {
    try {
        socket.bind(Address("0.0.0.0", port));
        std::cout << "Receiver initialized and listening on port " << port << "..." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize receiver: " << e.what() << std::endl;
        return false;
    }
}

// Log packet details for debugging purposes
void log_received_packet(const Packet& packet) {
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(packet.receive_time.time_since_epoch()).count();
    std::cout << "Received packet of size " << BUFFER_SIZE << " bytes at " << now_ms 
              << " ms, seq_number: " << packet.seq_number << std::endl;
}

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

    while (true) {
        // Receive packets
        int received;
        try {
            received = socket.recv(buffer, BUFFER_SIZE);
        } catch (const std::exception& e) {
            std::cerr << "Failed to receive packet: " << e.what() << std::endl;
            continue; // Continue listening even after a receive failure
        }

        // Populate the Packet structure
        Packet packet;
        strncpy(packet.data, buffer, received);
        packet.seq_number = seq_number++;
        packet.receive_time = std::chrono::steady_clock::now();

        // Log received packet for verification
        log_received_packet(packet);
    }

    return 0;
}

