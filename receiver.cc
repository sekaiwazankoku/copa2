
#include <iostream>
#include <chrono>
#include "udp-socket.hh"
#include "receiver.hh"

#define BUFFER_SIZE 1500

int main(int argc, char *argv[]) {
    // Command-line arguments
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <Port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);

    // UDP socket setup to listen on the specified port
    UDPSocket socket;
    socket.bind(Address("0.0.0.0", port));

    char buffer[BUFFER_SIZE];

    std::cout << "Receiver is listening on port " << port << "..." << std::endl;

    while (true) {
        // Receive packets
        auto received = socket.recv(buffer, BUFFER_SIZE);

        // Get current timestamp
        auto now = std::chrono::steady_clock::now();
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        // Output for verification
        std::cout << "Received packet of size " << received << " bytes at " << now_ms << " ms" << std::endl;
    }

    return 0;
}

