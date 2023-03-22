#pragma once

/*
   The first twelve octets are present in every RTP packet, while the
   list of CSRC identifiers is present only when inserted by a mixer.

   https://www.rfc-editor.org/rfc/rfc3550.html#section-5
*/
struct rtp_header 
{
  // uint8_t v; // 2 bits, version
  // uint8_t p; // 1 bit, presence of padding at the end
  // uint8_t x; // 1 bit, presence of header extension
  // uint8_t cc; // 4 bits, contribution count
  // uint8_t m; // 1 bit, marker defined by the profile used
  // uint8_t payload_type; // 7 bits, tpe of payload (33 for mpeg-ts)
  uint16_t sequence_number;
  uint32_t timestamp;
  // uint32_t ssrc;
  // uint32_t csrc; // really cc * 32 bits, but shouldn't have to worry about this.
};


