#pragma once

/*
   The first twelve octets are present in every RTP packet, while the
   list of CSRC identifiers is present only when inserted by a mixer.

   https://www.rfc-editor.org/rfc/rfc3550.html#section-5
*/
// #pragma pack(1)
struct rtp_header 
{
  uint16_t version:2; // 2 bits, version
  uint16_t padding:1; // 1 bit, presence of padding at the end
  uint16_t extensions:1; // 1 bit, presence of header extension
  uint16_t cc:4; // 4 bits, contribution count
  uint16_t marker:1; // 1 bit, marker defined by the profile used
  uint16_t payload_type:7; // 7 bits, tpe of payload (33 for mpeg-ts)
  uint16_t sequence_number;
  uint32_t timestamp;
  uint32_t ssrc;
  // uint32_t csrc; // really cc * 32 bits, but shouldn't have to worry about this.

  uint16_t get_sequence_number()
  {
    return ntohs(sequence_number);
  }

  uint32_t get_timestamp()
  {
    return ntohl(timestamp);
  }

  uint32_t get_ssrc()
  {
    return ntohl(ssrc);
  }

  uint8_t get_payload_type() 
  {
      return ntohs(payload_type);
  }
};


