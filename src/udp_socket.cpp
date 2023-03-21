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
  this->setSocketMetadata();
  Socket s = this->create();
  this->sock_fd = s;
}


void UdpSocket::setSocketMetadata() 
{
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM; // specifies udp
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(NULL, this->receive_port, &hints, &this->sock_meta);

    if (status != 0)
    {
      std::cerr << "getaddrinfo error: " << gai_strerror(status) << "\n";
      exit(1);
    }
}


Socket UdpSocket::create()
{
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("Failed to create socket");
    exit(1);
  }

  int bound = bind(sockfd, this->sock_meta->ai_addr, this->sock_meta->ai_addrlen);
  if (bound < 0) {
    perror("Failed to bind socket");
    close(sockfd);
    exit(1);
  } 

  return sockfd;
}