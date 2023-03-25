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

#include "rtp_packet.h"
#include "udp_socket.h"

const int MAXBUFLEN = 1024; // max pkt_size should be specified by ffmpeg.

struct Packet {
  char* data;
  int num_bytes_to_send; // the bytes received by recvfrom
  std::chrono::time_point<std::chrono::high_resolution_clock> send_time;
};


int main() {
  UdpSocket proxy_socket { "9304", "9002" };

  auto cmp = [](Packet left, Packet right){
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration left_delta = left.send_time - now;
    std::chrono::duration right_delta = right.send_time - now;
    return left_delta > right_delta;
  };

  std::priority_queue<Packet, std::vector<Packet>, decltype(cmp)> network_queue(cmp); 

  char buffer[MAXBUFLEN];
  while (true) {
    std::cout << network_queue.size() << "\n";
    
    if (network_queue.size() > 4) {
      Packet t = network_queue.top();
      rtp_header* header = (rtp_header*) t.data;
      std::cout << header->get_sequence_number() << "\n";
      // std::cout << t.data;
      std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(t.send_time.time_since_epoch()).count();
      delete t.data;
      network_queue.pop();
    }
  

    int received_bytes = recvfrom(proxy_socket.socket_fd, buffer, MAXBUFLEN, 0, nullptr, nullptr);
    if (received_bytes < 0) 
    {
      perror("Failed to receive datagram\n");
      continue;
    }
    buffer[received_bytes] = '\0';

    rtp_header header;
    memcpy(&header, buffer, sizeof(header));
    
    // std::cout << header.get_sequence_number() << "\n";
    // std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() << "\n";


    char* payload = new char[received_bytes];
    memcpy(payload, &buffer, received_bytes);
    Packet p {
      payload,
      received_bytes,
      std::chrono::high_resolution_clock::now()
    };

    network_queue.push(p);

    int sent_bytes = sendto(proxy_socket.socket_fd, buffer, received_bytes, 0, proxy_socket.destaddr->ai_addr, proxy_socket.destaddr->ai_addrlen);
    if (sent_bytes < 0 || sent_bytes != received_bytes) // we should proxy the examt same message.
    {
      perror("Failed to send datagram\n");
      continue;
    }
    // char ipstr[INET6_ADDRSTRLEN];

    // std::cout << "Proxied datagram to " << inet_ntop(AF_INET, &((struct sockaddr_in6 *) s.destination_meta->ai_addr)->sin6_addr, ipstr, sizeof(ipstr)) << ": " << buffer <<  "\n";

    
  }
  
  return 0;
}