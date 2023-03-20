#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>

#include "rtp_packet.h"

namespace asio = boost::asio;

int main() 
{
  
  asio::io_service _io_service;

  auto io_work = []() -> void
  {
    std::cout << "Doing some async work!\n";
  };

  _io_service.post(io_work);



  _io_service.run();



  return 0;
}