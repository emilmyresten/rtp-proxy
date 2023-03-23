#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <unistd.h>

#include "udp_socket.h"

const int IPVERSION = AF_INET6;
const int SOCKETTYPE = SOCK_DGRAM;
 
UdpSocket::UdpSocket(const char* port, SocketType tpe, const char* dest_port) 
{
  this->port = port;
  this->dest_port = dest_port;
  this->socktype = tpe;
  this->setSocketAddress();
  if (this->socktype == SEND) {
    if (std::strcmp(dest_port, "-1") == 0) { // this value is default, set in udp_socket.h
      std::cerr << "must set destination port on sending socket.";
      exit(1);
    }
    this->setDestination();
  }

  this->_fd = createSocket();  
  std::cout << "Created " << (this->socktype == RECEIVE ? "receiving" : "sending") << " socket on port: " << this->port << "\n";
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


void UdpSocket::setSocketAddress() 
{
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = IPVERSION;
    hints.ai_socktype = SOCKETTYPE;
    hints.ai_flags = AI_PASSIVE; // same as INETADDR_ANY
    
    int status = getaddrinfo(nullptr, this->port, &hints, &this->sockaddr);

    if (status != 0)
    {
      std::cerr << "getaddrinfo error: " << gai_strerror(status) << "\n";
      exit(1);
    }
}


int UdpSocket::createSocket()
{
  int sockfd = socket(IPVERSION, SOCKETTYPE, 0);
  if (sockfd < 0) {
    perror("Failed to create socket");
    exit(1);
  }
  
  if ((bind(sockfd, this->sockaddr->ai_addr, this->sockaddr->ai_addrlen)) < 0) {
        perror("Failed to bind socket");
        close(sockfd);
        exit(1);
      }
  freeaddrinfo(this->sockaddr);

  return sockfd;
}