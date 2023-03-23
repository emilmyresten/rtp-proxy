#pragma once


class UdpSocket 
{
  public:
    UdpSocket(short, const char*);
    int socket_fd;
    // struct addrinfo* sockaddr;
    

    struct addrinfo* destaddr;
    const char* dest_port;
    
    // void setSocketAddress(short);
    void setDestination();
    int createSocket(short);


    ~UdpSocket()
    {
      close(socket_fd);
    }
};