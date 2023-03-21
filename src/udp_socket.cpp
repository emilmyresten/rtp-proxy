#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <unistd.h>

#include "udp_socket.h"


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
  // Allocate memory for destination_meta
  this->destination_meta = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in*));

  struct sockaddr_in destaddr;

  memset(&destaddr, 0, sizeof(destaddr));
  destaddr.sin_family = AF_INET;
  destaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  destaddr.sin_port = htons(this->destination_port);

  memcpy(this->destination_meta, &destaddr, sizeof(destaddr));
}


void UdpSocket::setSocketMetadata(SocketType socket_type) 
{
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM; // specifies udp
    hints.ai_flags = AI_PASSIVE;
    
    int status;
    switch (socket_type)
    {
      case RECEIVE:
        status = getaddrinfo(NULL, this->receive_port, &hints, &this->receive_sock_meta);
        break;
      case SEND:
        status = getaddrinfo(NULL, this->send_port, &hints, &this->send_sock_meta);
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
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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
      break;
    case SEND:
      if ((bind(sockfd, this->send_sock_meta->ai_addr, this->send_sock_meta->ai_addrlen)) < 0) {
        perror("Failed to bind socket");
        close(sockfd);
        exit(1);
      }
      break;
  }

  return sockfd;
}