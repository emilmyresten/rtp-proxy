#pragma once


using Socket = int;

class UdpSocket 
{
  public:
    UdpSocket();
    Socket sock_fd;
    struct addrinfo* sock_meta;
    

  private:
    
    char* receive_port { "8000" };
    char* send_port { "8002" };
    
    void setSocketMetadata();
    Socket create();
};