#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <unistd.h>

#include "rtp_packet.h"
#include "udp_socket.h"

int main() {
  UdpSocket s;

  char buffer[1024];
  while (true) {
    struct sockaddr_in cliaddr;
    memset(&cliaddr, 0, sizeof(cliaddr));
    socklen_t len = sizeof(cliaddr);
    int n = recvfrom(s.receive_sock_fd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
    if (n < 0) 
    {
      perror("Failed to receive datagram\n");
      continue;
    }
    buffer[n] = '\0';

    int sent = sendto(s.send_sock_fd, buffer, sizeof(buffer), 0, (struct sockaddr*) s.destination_meta, sizeof(*s.destination_meta));
    if (sent < 0)
    {
      perror("Failed to send datagram\n");
      continue;
    }
    std::cout << "Sending " << buffer << " to " << inet_ntoa(s.destination_meta->sin_addr) << ":" << ntohs(s.destination_meta->sin_port) << "\n";

    std::cout << "Received datagram from " << inet_ntoa(cliaddr.sin_addr) << ":" << ntohs(cliaddr.sin_port) << ": " << buffer << std::endl;
  }
  
  return 0;
}