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
  UdpSocket s;

  const int MAXBUFLEN = 1024*2;

  char buffer[MAXBUFLEN];
  while (true) {
    struct sockaddr_in cliaddr;
    memset(&cliaddr, 0, sizeof(cliaddr));
    socklen_t len = sizeof(cliaddr);
    int received_bytes = recvfrom(s.receive_sock_fd, buffer, MAXBUFLEN-1, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
    if (received_bytes < 0) 
    {
      perror("Failed to receive datagram\n");
      continue;
    }
    buffer[received_bytes] = '\0';

    std::cout << "Received datagram from " << inet_ntoa(cliaddr.sin_addr) << ":" << ntohs(cliaddr.sin_port) << ": " << buffer << "\n";

    int sent_bytes = sendto(s.send_sock_fd, buffer, received_bytes, 0, (struct sockaddr*) s.destination_meta, sizeof(*s.destination_meta));
    if (sent_bytes < 0 || sent_bytes != received_bytes) // we should proxy the examt same message.
    {
      perror("Failed to send datagram\n");
      continue;
    }

    std::cout << "Sent datagram to " << inet_ntoa(s.destination_meta->sin_addr) << ":" << ntohs(s.destination_meta->sin_port) << ": " << buffer <<  "\n";

    
  }
  
  return 0;
}