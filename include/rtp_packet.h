#pragma once


struct rtp_header 
{
  int payload_type;
  int sequence_number;
  int timestamp;
  int ssrc;
};

class rtp_packet 
{
  public:
    rtp_header* header;
};


