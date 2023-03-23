#pragma once


enum SocketType 
{
  SEND,
  RECEIVE
};

class UdpSocket 
{
  public:
    UdpSocket(const char*, SocketType, const char* = "-1");
    const char* port;
    SocketType socktype;
    int _fd;
    struct addrinfo* sockaddr;
    

    struct addrinfo* destaddr;
    const char* dest_port;
    
    void setSocketAddress();
    void setDestination();
    int createSocket();


    ~UdpSocket()
    {
      close(_fd);
    }
};