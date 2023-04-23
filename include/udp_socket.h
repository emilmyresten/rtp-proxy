#pragma once


const int IPVERSION = AF_INET;
const int SOCKETTYPE = SOCK_DGRAM;

class UdpSocket 
{
  public:
    UdpSocket(char*); // receive(from)
    UdpSocket(char*, char*, char*); //  send(via, to_ip, to_port)
    int socket_fd;
    struct addrinfo* destaddr;
    
    void setDestination(char*, char*);
    int createSocket(char*);


    ~UdpSocket()
    {
      close(socket_fd);
    }
};