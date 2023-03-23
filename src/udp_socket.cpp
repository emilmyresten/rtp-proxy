#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <unistd.h>

#include "udp_socket.h"

const int IPVERSION = AF_INET;
const int SOCKETTYPE = SOCK_DGRAM;
 
UdpSocket::UdpSocket(short recv_port, const char* dest_port) 
{
  this->dest_port = dest_port;
  this->setDestination();

  this->socket_fd = createSocket(recv_port);  
  std::cout << "Created socket on port: " << recv_port << "\n";
}

void UdpSocket::setDestination()
{
  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = IPVERSION;
  hints.ai_socktype = SOCKETTYPE;
  // hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(nullptr, this->dest_port, &hints, &this->destaddr);
  if (status < 0)
  {
    std::cerr << "getaddrinfo destination error: " << gai_strerror(status) << "\n";
    exit(1);
  }

}

int UdpSocket::createSocket(short port)
{

  sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = IPVERSION;
  servaddr.sin_port = htons(port);
  servaddr.sin_addr.s_addr = inet_addr("192.168.0.9");

  int sockfd = socket(IPVERSION, SOCKETTYPE, 0);
  if (sockfd < 0) {
    perror("Failed to create socket");
    exit(1);
  }
  
  if ((bind(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr))) < 0) {
        perror("Failed to bind socket");
        close(sockfd);
        exit(1);
      }

  return sockfd;
}