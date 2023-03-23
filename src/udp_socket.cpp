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
 
UdpSocket::UdpSocket(char* recv_port, char* dest_port) 
{
  this->setDestination(dest_port);
  this->socket_fd = createSocket(recv_port);  
  std::cout << "Created proxy: port " << recv_port << "->" << dest_port << "\n";
}

void UdpSocket::setDestination(char* port)
{
  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = IPVERSION;
  hints.ai_socktype = SOCKETTYPE;
  // hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(nullptr, port, &hints, &this->destaddr);
  if (status < 0)
  {
    std::cerr << "getaddrinfo destination error: " << gai_strerror(status) << "\n";
    exit(1);
  }

}

int UdpSocket::createSocket(char* port)
{

  struct addrinfo hints, *servaddr;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = IPVERSION;
  hints.ai_socktype = SOCKETTYPE;
  hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(nullptr, port, &hints, &servaddr);
  if (status < 0)
  {
    std::cerr << "getaddrinfo receiving socket error: " << gai_strerror(status) << "\n";
    exit(1);
  }


  int sockfd = socket(IPVERSION, SOCKETTYPE, 0);
  if (sockfd < 0) {
    perror("Failed to create socket");
    exit(1);
  }
  
  if ((bind(sockfd, servaddr->ai_addr, servaddr->ai_addrlen)) < 0) {
        perror("Failed to bind socket");
        close(sockfd);
        exit(1);
      }

  return sockfd;
}