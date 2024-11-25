#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>
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

// Function to launch low-rate volumetric attack (constant line rate)
void low_rate_volumetric_attack(UDPSocket& socket, const std::string& target_ip, int target_port, double packet_interval, int duration, int& total_bytes_sent, std::ofstream& log_file) {
    int seq_number = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto last_log_time = start_time;

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

        if (elapsed_time >= duration) {
            std::cout << "Experiment duration reached. Stopping low-rate volumetric attack." << std::endl;
            break; // Stop attack after the specified duration
        }

        // Send packet at the specified interval
        Packet packet;
        strncpy(packet.data, std::string(PACKET_SIZE, 'X').c_str(), PACKET_SIZE);
        packet.seq_number = seq_number++;
        packet.send_time = std::chrono::steady_clock::now();

        if (!send_packet(socket, packet, target_ip, target_port)) {
            std::cerr << "Error in sending packet. Retrying." << std::endl;
            continue;
        }

        total_bytes_sent += PACKET_SIZE;

        // Log total bytes sent every millisecond
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_log_time).count() >= 1) {
            log_file << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
                     << " : " << total_bytes_sent << std::endl;
            last_log_time = now;
        }

        // Sleep to maintain line rate
        std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(packet_interval));
    }
}

// Low-rate attack before the start of custom attack
void pre_attack_phase(UDPSocket& socket, const std::string& target_ip, int target_port, int pre_attack_duration_ms, double pre_attack_rate_mbps, int& total_bytes_sent, std::ofstream& log_file, std::chrono::steady_clock::time_point& last_log_time) {
    double packet_interval_ms = 1.0 / ((pre_attack_rate_mbps * 1024 * 1024 / PACKET_SIZE) / 8); // Packet interval in ms
    auto start_time = std::chrono::steady_clock::now();

    std::cout << "Starting pre-attack phase at " << pre_attack_rate_mbps << " Mbps for " << pre_attack_duration_ms << " ms." << std::endl;
    log_file << "Pre attack phase:" << std::endl;

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();

        if (elapsed_time_ms >= pre_attack_duration_ms) {
            std::cout << "Pre-attack phase complete. Switching to custom attack." << std::endl;
            break; // End pre-attack phase
        }

        // Send packet
        Packet packet;
        strncpy(packet.data, std::string(PACKET_SIZE, 'X').c_str(), PACKET_SIZE);
        packet.seq_number = total_bytes_sent / PACKET_SIZE; // Sequence number based on total bytes sent
        packet.send_time = std::chrono::steady_clock::now();

        if (!send_packet(socket, packet, target_ip, target_port)) {
            std::cerr << "Error: Failed to send packet in pre-attack phase. Retrying." << std::endl;
            continue; // Retry sending if there's an error
        }

        total_bytes_sent += PACKET_SIZE;

        // Log total bytes sent every millisecond
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_log_time).count() >= 1) {
            log_file << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
                     << " : " << total_bytes_sent << std::endl;
            last_log_time = now;
        }

        // Sleep to maintain the 90 Mbps line rate
        std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(packet_interval_ms));
    }

    log_file << "End of pre attack phase." << std::endl;
}

int main(int argc, char *argv[]) {
    // Command-line arguments
    if (argc < 8 || argc > 9) {
        std::cerr << "Usage: " << argv[0] << " <IP> <Port> <burst_size> <burst_duration> <inter_burst_time> <logfile> <duration> [-c | -v]" << std::endl;
        return 1;
    }

    std::string target_ip = argv[1];
    int target_port = std::stoi(argv[2]);
    int burst_size = std::stoi(argv[3]);
    int burst_duration = std::stoi(argv[4]);
    int inter_burst_time = std::stoi(argv[5]);
    std::string logfile_name = argv[6];
    int duration = std::stoi(argv[7]);
    std::string attack_type = (argc == 9) ? argv[8] : "-c"; //don't keep it optional

    // Open log file
    std::ofstream log_file(logfile_name);
    if (!log_file.is_open() || !log_file.good()) {
        std::cerr << "Error: Unable to open log file " << logfile_name << std::endl;
        return 1;
    }

    // UDP socket setup
    UDPSocket socket;
    if (!initialize_sender(socket)) {
        return 1;
    }

    auto start_time = std::chrono::steady_clock::now();
    int total_bytes_sent = 0;
    auto last_log_time = std::chrono::steady_clock::now();

    log_file << "Burst Size: " << burst_size 
             << ", Burst Duration: " << burst_duration
             << ", Inter Burst Time: " << inter_burst_time
             << ", Duration of Experiment(s): " << duration << std::endl;
    log_file << "Log started at " << std::chrono::duration_cast<std::chrono::milliseconds>(start_time.time_since_epoch()).count() << " ms" << std::endl;

    // Attack selection
    if (attack_type == "-v") {
        double packet_interval = 1.0 / ((9 * 1024 * 1024 / PACKET_SIZE) / 8); // 12 Mbps line rate
        low_rate_volumetric_attack(socket, target_ip, target_port, packet_interval, duration, total_bytes_sent, log_file);
    } else {
        int pre_attack_duration_ms = 4000; // 4 seconds
        double pre_attack_rate_mbps = 90;
        pre_attack_phase(socket, target_ip, target_port, pre_attack_duration_ms, pre_attack_rate_mbps, total_bytes_sent, log_file, last_log_time);
    
        // Custom burst attack logic
        double burst_rate = calculate_burst_rate(burst_size, burst_duration); // bytes per ms
        double burst_pkt_tx_delay = PACKET_SIZE / burst_rate; // using PACKET_SIZE defined as 1500 bytes by default

        // Variables for controlling burst timing
        auto last_burst_time = std::chrono::steady_clock::now();
        auto last_send_time = std::chrono::steady_clock::now();
        auto last_log_time = std::chrono::steady_clock::now();
        //int total_bytes_sent = 0;
        int burst_bytes_sent = 0;
        bool send_burst = false;
        //bool log_packet_flag = true;
        int seq_number = 0;
        int interval_bytes_sent = 0;
        long last_burst_end_time = 0;

        while (true) {
            auto now = std::chrono::steady_clock::now();
            long current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count(); // Convert to seconds

            // Terminate after the specified duration
            if (elapsed_time >= duration) {
                std::cout << "Experiment duration reached. Stopping sender." << std::endl;
                break;
            }

            // Check if it's time to start a new burst
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_burst_time).count() > inter_burst_time) { 
                // Send new burst
                send_burst = true;
                last_burst_time = now;
                last_burst_end_time = current_time_ms;
                burst_bytes_sent = 0;
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
                burst_bytes_sent += PACKET_SIZE;
                interval_bytes_sent += PACKET_SIZE;

                last_send_time = now;

                // End burst if burst_size is reached
                if (burst_bytes_sent >= burst_size) {
                    burst_bytes_sent = 0;
                    last_burst_time = now;
                    send_burst = false;
                }
            }
            auto now_ms = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now_ms - last_log_time).count() >= 1) {
                log_file << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
                        << " : " << total_bytes_sent << std::endl;
                last_log_time = now_ms;
            } 

        }
    }

    // End time after the loop completes
    auto end_time = std::chrono::steady_clock::now();
    double duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
    double average_throughput = (total_bytes_sent * 8) / duration_seconds; // in bits per second

    log_file << "Average Throughput (bps): " << average_throughput << std::endl;
    log_file.close();
    return 0;
}