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
 
UdpSocket::UdpSocket() 
{
  this->setSocketMetadata(RECEIVE);
  this->setSocketMetadata(SEND);
  this->setDestination();
  Socket receive_socket = this->createSocket(RECEIVE);
  Socket send_socket = this->createSocket(SEND);
  this->receive_sock_fd = receive_socket;
  this->send_sock_fd = send_socket;

}

void UdpSocket::setDestination()
{
  struct addrinfo hints, *destaddr ;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = IPVERSION;
  hints.ai_socktype = SOCKETTYPE;
  // hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(nullptr, this->destination_port, &hints, &this->destination_meta);
  if (status < 0)
  {
    std::cerr << "getaddrinfo destination error: " << gai_strerror(status) << "\n";
    exit(1);
  }

}


void UdpSocket::setSocketMetadata(SocketType socket_type) 
{
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = IPVERSION;
    hints.ai_socktype = SOCKETTYPE;
    hints.ai_flags = AI_PASSIVE;
    
    int status;
    switch (socket_type)
    {
      case RECEIVE:
        status = getaddrinfo(nullptr, this->receive_port, &hints, &this->receive_sock_meta);
        break;
      case SEND:
        status = getaddrinfo(nullptr, this->send_port, &hints, &this->send_sock_meta);
        break;
    }

    if (status != 0)
    {
      std::cerr << "getaddrinfo error: " << gai_strerror(status) << "\n";
      exit(1);
    }
}


Socket UdpSocket::createSocket(SocketType socket_type)
{
  int sockfd = socket(IPVERSION, SOCKETTYPE, 0);
  if (sockfd < 0) {
    perror("Failed to create socket");
    exit(1);
  }
  
  switch (socket_type)
  {
    case RECEIVE:
      if ((bind(sockfd, this->receive_sock_meta->ai_addr, this->receive_sock_meta->ai_addrlen)) < 0) {
        perror("Failed to bind socket");
        close(sockfd);
        exit(1);
      }
      freeaddrinfo(this->receive_sock_meta);
      break;
    case SEND:
      if ((bind(sockfd, this->send_sock_meta->ai_addr, this->send_sock_meta->ai_addrlen)) < 0) {
        perror("Failed to bind socket");
        close(sockfd);
        exit(1);
      }
      freeaddrinfo(this->send_sock_meta);
      break;
  }

  return sockfd;
}