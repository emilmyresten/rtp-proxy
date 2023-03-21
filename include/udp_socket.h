#pragma once


using Socket = int;

enum SocketType 
{
  SEND,
  RECEIVE
};

class UdpSocket 
{
  public:
    UdpSocket();
    Socket receive_sock_fd;
    struct addrinfo* receive_sock_meta;
    const char* receive_port { "8000" };

    Socket send_sock_fd;
    struct addrinfo* send_sock_meta;
    const char* send_port { "8004" };


    struct sockaddr_in* destination_meta;
    const int destination_port { 8002 };
    
    void setSocketMetadata(SocketType);
    void setDestination();
    Socket createSocket(SocketType);


    ~UdpSocket()
    {
      close(receive_sock_fd);
      close(send_sock_fd);
      freeaddrinfo(receive_sock_meta);
      freeaddrinfo(send_sock_meta);
      
      delete this->destination_meta;
    }
};