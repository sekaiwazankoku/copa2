
#include <iostream>
#include <chrono>
#include <cstring>
#include <fstream>
#include "udp-socket.hh"
#include "receiver.hh"

std::ofstream log_file("receiver_log.txt"); // Log file for receiver activity "receiver_log.txt"

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
    log_file.flush();
    // std::cout << "Received packet of size " << packet_size << " bytes at " << now_ms 
    //           << " ms, seq_number: " << packet.seq_number 
    //           << ", Inter-arrival Time(ms): " << inter_arrival_time << std::endl;
}

// Log interval throughput and reset for next interval
void log_interval_throughput(int interval_bytes_received, double interval_duration_s) {
    double throughput_bps = (interval_bytes_received * 8) / interval_duration_s; // Throughput in bits per second
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    log_file << "[Throughput] Time(ms): " << ms 
             << ", Bytes Received: " << interval_bytes_received 
             << ", Throughput(bps): " << throughput_bps << std::endl;
    log_file.flush();
}

// Log packet details for debugging purposes
// void log_received_packet(const Packet& packet) {
//     auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(packet.receive_time.time_since_epoch()).count();
//     std::cout << "Received packet of size " << BUFFER_SIZE << " bytes at " << now_ms 
//               << " ms, seq_number: " << packet.seq_number << std::endl;
// }

int main(int argc, char *argv[]) {

    if (!log_file.is_open()) {
        std::cerr << "Error: Failed to open receiver_log.txt for logging." << std::endl;
        return 1; // Ensure the program exits if the file cannot be opened
    }

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

    auto start_time = std::chrono::steady_clock::now(); // Start of the experiment
    auto last_log_time = std::chrono::steady_clock::now();

    char buffer[BUFFER_SIZE];
    int seq_number = 0;
    UDPSocket::SockAddress other_addr;

    auto last_receive_time = std::chrono::steady_clock::now();

    int total_bytes_received = 0;
    int interval_bytes_received = 0;

    while (true) {

        // Check elapsed time to break loop
        // auto now = std::chrono::steady_clock::now();
        // int elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        // if (elapsed_seconds >= duration) {
        //     std::cout << "Specified duration reached. Stopping receiver." << std::endl;
        //     break;
        // }

        // Receive packets
        int received;
        try {
            received = socket.receivedata(buffer, BUFFER_SIZE, -1, other_addr);
        } catch (const std::exception& e) {
            std::cerr << "Failed to receive packet: " << e.what() << std::endl;
            continue; // Continue listening even after a receive failure
        }
        
        total_bytes_received += received;
        interval_bytes_received += received;

        // Populate the Packet structure
        Packet packet;
        strncpy(packet.data, buffer, received);
        packet.seq_number = seq_number++;
        packet.receive_time = std::chrono::steady_clock::now();

        // Calculate inter-arrival time
        auto inter_arrival_time = std::chrono::duration_cast<std::chrono::milliseconds>(packet.receive_time - last_receive_time).count();
        last_receive_time = packet.receive_time;

        // Log packet details for verification every 10ms
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_log_time).count() >= 10) {
            log_received_packet(packet, received, inter_arrival_time);

            // Log throughput for the interval and reset counters
            double interval_duration_s = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_log_time).count() / 1000.0;
            log_interval_throughput(interval_bytes_received, interval_duration_s);
            interval_bytes_received = 0; // Reset for the next interval
            last_log_time = now;

            // // Debugging statement to confirm logging is occurring
            // std::cout << "Logged data to receiver_log.txt at time(ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() << std::endl;
        }
    }

    // End time after the loop completes
    auto end_time = std::chrono::steady_clock::now();
    double duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
    double average_throughput = (total_bytes_received * 8) / duration_seconds; // in bits per second

    log_file << "Average Throughput (bps): " << average_throughput << std::endl;

    log_file.close();
    return 0;
}