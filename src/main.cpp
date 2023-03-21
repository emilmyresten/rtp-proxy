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
  UdpSocket udp_socket;

  char buffer[1024];
  while (true) {
    struct sockaddr_in cliaddr;
    memset(&cliaddr, 0, sizeof(cliaddr));
    socklen_t len = sizeof(cliaddr);
    int n = recvfrom(udp_socket.sock_fd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
    if (n < 0) {
      perror("Failed to receive datagram\n");
      continue;
    }
    buffer[n] = '\0';
    std::cout << "Received datagram from " << inet_ntoa(cliaddr.sin_addr) << ":" << ntohs(cliaddr.sin_port) << ": " << buffer << std::endl;
  }
  
  close(udp_socket.sock_fd);
  freeaddrinfo(udp_socket.sock_meta);
  return 0;
}