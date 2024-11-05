#ifndef RECEIVER_HH
#define RECEIVER_HH

#include <string>
#include <chrono>

// Constants
#define BUFFER_SIZE 1500

// // Packet structure for received data
struct Packet {
    char data[BUFFER_SIZE]; // Payload data
    int seq_number; // Sequence number for tracking
    std::chrono::steady_clock::time_point receive_time; // Timestamp for receiving time
};

// Function prototypes
bool initialize_receiver(UDPSocket& socket, int port);
void log_received_packet(const Packet& packet);

#endif // RECEIVER_HH
