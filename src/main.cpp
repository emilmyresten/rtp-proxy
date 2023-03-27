#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <unistd.h>
#include <cmath>
#include <cstring>
#include <chrono>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <iomanip>
#include <random>
#include <fstream>

#include "rtp_packet.h"
#include "udp_socket.h"

const int MAXBUFLEN = 1024; // max pkt_size should be specified by ffmpeg.
std::mutex network_mutex; // protect the priority queue

auto constant_playout_delay = std::chrono::seconds(2);
using variable_playout_delay_unit = std::chrono::microseconds;
std::default_random_engine random_generator(1); // explicitly seed the random generator for clarity.
std::binomial_distribution<int> jitter_distribution(10.0, 5.0); // mean of 10.0, std deviation of 5.0 (dont want negative jitter)


struct Packet {
  char* data;
  int num_bytes_to_send; // the bytes received by recvfrom
  std::chrono::time_point<std::chrono::high_resolution_clock> send_time;
};

auto cmp = [](Packet left, Packet right){
    auto now = std::chrono::high_resolution_clock::now();
    auto left_delta = left.send_time - now;
    auto right_delta = right.send_time - now;
    return left_delta > right_delta;
  };
std::priority_queue<Packet, std::vector<Packet>, decltype(cmp)> network_queue(cmp); 


struct LogInfo {
  uint16_t ssq_no;
  long long time_since_epoch;
};

std::queue<LogInfo> log_queue;
std::mutex log_queue_mutex;
std::ofstream log_file;

std::atomic<bool> keep_server_running{true};

void file_writer(std::string filepath) {
  log_file.open(filepath, std::ofstream::app);
  std::chrono::nanoseconds ns_since_epoch(std::chrono::system_clock::now().time_since_epoch());
  std::chrono::time_point<std::chrono::system_clock> time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(ns_since_epoch));
  std::time_t time_t(std::chrono::system_clock::to_time_t(time_point));
  
  log_file << "Session init: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\n" << std::endl;

  while (keep_server_running) {
    if (!log_queue.empty()) {
      std::lock_guard<std::mutex> lock(log_queue_mutex);
      LogInfo l = log_queue.front();
      log_queue.pop();
      log_file << l.ssq_no << ", " << l.time_since_epoch << std::endl;
    }
  }
  log_file.close();
}


void receiver(char* from) {
  UdpSocket receiving_socket { from, "9002" };
  char buffer[MAXBUFLEN];
  while (keep_server_running) {
    
    int received_bytes = recvfrom(receiving_socket.socket_fd, buffer, MAXBUFLEN, 0, nullptr, nullptr);
    if (received_bytes < 0) 
    {
      perror("Failed to receive datagram\n");
      continue;
    }
    buffer[received_bytes] = '\0';

    rtp_header header;
    memcpy(&header, buffer, sizeof(header));
    
    char* payload = new char[received_bytes];
    memcpy(payload, &buffer, received_bytes);

    auto now = std::chrono::high_resolution_clock::now();
    auto jitter = variable_playout_delay_unit(jitter_distribution(random_generator)%1000);
    Packet p {
      payload,
      received_bytes,
      now + constant_playout_delay + jitter
    };

    // std::cout << "Received " << header.get_sequence_number() << "\n";
    auto ssq_no = header.get_sequence_number();
    if (ssq_no >= 100 && ssq_no < 110) {
      LogInfo l {
        ssq_no,
        now.time_since_epoch().count()
      };
      std::unique_lock<std::mutex> lock(log_queue_mutex);
      log_queue.push(l);
      lock.unlock();
    }

    std::lock_guard<std::mutex> lock(network_mutex);
    network_queue.push(p); 
  }
}

void sender(char* to) {
  UdpSocket sending_socket { "9002", to };

  while (keep_server_running) {
    if (!network_queue.empty()) {
      Packet p = network_queue.top(); // .top() doesn't mutate, no need to lock.
      if ((p.send_time - std::chrono::high_resolution_clock::now()).count() <= 0) {
        std::unique_lock<std::mutex> lock(network_mutex);
        network_queue.pop();
        lock.unlock();

        int sent_bytes = sendto(sending_socket.socket_fd, p.data, p.num_bytes_to_send, 0, sending_socket.destaddr->ai_addr, sending_socket.destaddr->ai_addrlen);
        if (sent_bytes < 0 || sent_bytes != p.num_bytes_to_send) // we should proxy the examt same message.
        {
          perror("Failed to send datagram\n");
          continue;
        }
        // std::cout << "Sent " << ((rtp_header*) p.data)->get_sequence_number() << "\n";
        delete p.data;
      }
    } 
  }
}


int main() {  
  char from[5] { "9001" };
  char to[5] { "9024" };
  std::string filepath { "../data/original_timings.txt"};

  std::thread recv_thread(receiver, from);
  std::thread send_thread(sender, to);
  std::thread datastore_thread(file_writer, filepath);

  std::cout << "Started proxy: " << from << "->" << to 
            << " with a " << constant_playout_delay.count() << " second propagation delay." << "\n";

  std::cout << "Press enter to stop.\n";
  std::cin.get();

  // signal to stop the thread
  keep_server_running = false;

  recv_thread.join();
  send_thread.join();
  datastore_thread.join();

  return 0;
}