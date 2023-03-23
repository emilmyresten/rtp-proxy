#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <unistd.h>
#include <cmath>

#include "rtp_packet.h"
#include "udp_socket.h"

int main() {
  UdpSocket receiving_socket { "8050", RECEIVE };
  UdpSocket sending_socket { "8052", SEND, "8054" };

  const int MAXBUFLEN = 1024*2;

  char buffer[MAXBUFLEN];
  while (true) {
    int received_bytes = recvfrom(receiving_socket._fd, buffer, MAXBUFLEN-1, MSG_WAITALL, nullptr, nullptr);
    if (received_bytes < 0) 
    {
      perror("Failed to receive datagram\n");
      continue;
    }
    buffer[received_bytes] = '\0';

    std::cout << "Received datagram: " << buffer << "\n";

    rtp_header header {
      /* 
      All integer fields are carried in network byte order, that is, most
      significant byte (octet) first (RFC 3550)
      the bit-wise OR | and the left-shift will make the byte at buffer 2 and buffer 3 offset from eachother, 
      saving each. b2 = 10010101'00000000, b3 = 11010110 => b2 | b3 = 10010101'11010110
      */
      static_cast<uint16_t>((buffer[2]) << 8 | buffer[3]), // ssqno 16-bits
      static_cast<uint32_t>(buffer[4] << 24 | (buffer[5] << 16) | (buffer[6]) << 8 | (buffer[7])), // timestamp 32-bits
    };

    if (header.sequence_number == 1) {
      std::cout << header.sequence_number << ": wrap around!\n";
    }
    

    int sent_bytes = sendto(sending_socket._fd, buffer, received_bytes, 0, sending_socket.destaddr->ai_addr, sending_socket.destaddr->ai_addrlen);
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