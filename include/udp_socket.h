#pragma once


class UdpSocket 
{
  public:
    UdpSocket(char*, char*);
    int socket_fd;
    struct addrinfo* destaddr;
    
    // void setSocketAddress(short);
    void setDestination(char*);
    int createSocket(char*);


    ~UdpSocket()
    {
      close(socket_fd);
    }
};