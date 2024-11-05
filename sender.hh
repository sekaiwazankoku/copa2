#ifndef SENDER_HH
#define SENDER_HH

#include <string>
#include <chrono>

// Constants
#define PACKET_SIZE 1500
#define DEFAULT_BURST_SIZE 1024 // Example burst size in bytes
#define DEFAULT_BURST_DURATION 40 // Example burst duration in ms
#define DEFAULT_INTER_BURST_TIME 100 // Example inter-burst interval in ms

// Packet structure for sending data
struct Packet {
    char data[PACKET_SIZE]; // Payload data
    int seq_number; // Sequence number for tracking
    std::chrono::steady_clock::time_point send_time; // Timestamp for sending time
};

// Function prototypes
bool initialize_sender(UDPSocket& socket);
bool send_packet(UDPSocket& socket, const Packet& packet);
double calculate_burst_rate(int burst_size, int burst_duration);
double calculate_packet_tx_delay(double burst_rate);

#endif // SENDER_HH
