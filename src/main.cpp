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

#include "rtp_packet.h"
#include "udp_socket.h"

const int MAXBUFLEN = 1024; // max pkt_size should be specified by ffmpeg.
std::mutex mtx; // protect the priority queue
std::condition_variable coordinator; // coordinate threads

auto constant_factor = std::chrono::seconds(2);
using variable_time_unit = std::chrono::microseconds;


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


void receiver(char* from) {
  UdpSocket receiving_socket { from, "9002" };
  char buffer[MAXBUFLEN];
  while (true) {
    
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

    auto jitter = variable_time_unit(rand()%1000);
    Packet p {
      payload,
      received_bytes,
      std::chrono::high_resolution_clock::now() + constant_factor + jitter
    };

    std::cout << "Received " << header.get_sequence_number() << "\n";

    std::lock_guard<std::mutex> lock(mtx);
    network_queue.push(p);
    coordinator.notify_all();
  }
}

void sender(char* to) {
  UdpSocket sending_socket { "9002", to };

  while (true) {
    if (!network_queue.empty()) {
      Packet p = network_queue.top(); // .top() doesn't mutate, no need to lock.
      if ((p.send_time - std::chrono::high_resolution_clock::now()).count() <= 0) {
        std::unique_lock<std::mutex> lock(mtx);
        network_queue.pop();
        lock.unlock();

        int sent_bytes = sendto(sending_socket.socket_fd, p.data, p.num_bytes_to_send, 0, sending_socket.destaddr->ai_addr, sending_socket.destaddr->ai_addrlen);
        if (sent_bytes < 0 || sent_bytes != p.num_bytes_to_send) // we should proxy the examt same message.
        {
          perror("Failed to send datagram\n");
          continue;
        }

        std::cout << "Sent " << ((rtp_header*) p.data)->get_sequence_number() << "\n";

        delete p.data;
      }
    }
        
  }
}


int main() {  
  char from[5] { "9001" };
  char to[5] { "9024" };
  std::thread recv_thread(receiver, from);
  std::thread send_thread(sender, to);

  std::cout << "Started proxy: " << from << "->" << to 
            << " with a " << constant_factor.count() << " second propagation delay." << "\n";

  recv_thread.join();
  send_thread.join();

  return 0;
}