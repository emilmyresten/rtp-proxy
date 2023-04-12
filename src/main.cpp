#include <iostream>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <unistd.h>
#include <cmath>
#include <cstring>
#include <chrono>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <iomanip>
#include <random>
#include <fstream>

#include "rtp_packet.h"
#include "udp_socket.h"

const int MAXBUFLEN = 1024; // max pkt_size should be specified by ffmpeg.
std::mutex network_mutex; // protect the priority queue

auto constant_playout_delay = std::chrono::milliseconds(40);
using variable_playout_delay_unit = std::chrono::microseconds;
std::default_random_engine random_generator(1); // explicitly seed the random generator for clarity.

// PDV with Gaussian/Normal/Binomial probability density function. Haven't found real support in the literature for values of mean and stddev.
std::uniform_int_distribution<int> jitter_distribution(400, 800); // jitter as 1-2% of constant delay factor.

// used to simulate jitter in ns3: https://gitlab.com/nsnam/ns-3-dev/-/blob/master/src/aodv/model/aodv-routing-protocol.cc
// std::uniform_int_distribution<int> jitter_distribution(0, 10); // between 0 and 10 microseconds. Too low.

/*
Creates a histogram with the distribution of packet delay variation.
Measured in microseconds (us), and stored in BUCKETS buckets. Each bucket represents BUCKET_RESOLUTION us, everything rounded down to nearest hundredth.
As an example; A packet that arrives 686 us after the previous, will be rounded to 600 us and 'put' in bucket 6.
*/
const int BUCKET_RESOLUTION = 100; // 100 ms bucket resolution
const int BUCKETS = 6000 / BUCKET_RESOLUTION;
std::vector<int> jitter_histogram(BUCKETS);

void dump_jitter_histogram_styled()
{
  std::cout << "\nJitter distribution, measured in microseconds, and stored in " 
            << BUCKETS 
            << " buckets. \nEach bucket represents " 
            << BUCKET_RESOLUTION 
            << " microseconds (us), everything rounded down to nearest hundredth. \nAs an example; A packet that arrives 686 us after the previous, will be rounded to 600 us and 'put' in bucket 6.\n"
            << "Each * represents "
            << BUCKET_RESOLUTION
            << " samples.\n";

  std::cout << "bucket_no\tno_samples\thistogram\n";
  for (int bucket = 0; bucket < jitter_histogram.size(); bucket++)
  {
    std::string o = "";
    for (int i = 1; i <= jitter_histogram[bucket]; i++)
    {
      if (i % BUCKET_RESOLUTION == 0)
      {
        o += '*';
      }
      
    }
    std::cout << bucket << "\t\t" << jitter_histogram[bucket] << "\t\t";
    std::cout << o << "\n";
  }
}

void dump_jitter_histogram_raw()
{
  std::cerr << "jitter_histogram start: \n";
  for (int bucket = 0; bucket < jitter_histogram.size(); bucket++)
  {
    std::cerr << bucket << "," << jitter_histogram[bucket] << "\n";
  }
  std::cerr << "jitter_histogram end: \n";
}

struct Packet {
  char* data;
  int num_bytes_to_send; // the bytes received by recvfrom
  std::chrono::time_point<std::chrono::steady_clock> send_time;
};

auto cmp = [](Packet left, Packet right){
    auto now = std::chrono::steady_clock::now();
    auto left_delta = left.send_time - now;
    auto right_delta = right.send_time - now;
    return left_delta > right_delta;
  };
std::priority_queue<Packet, std::vector<Packet>, decltype(cmp)> network_queue(cmp); 

std::atomic<bool> keep_server_running{true};

void receiver(char* from) {
  UdpSocket receiving_socket { from, "9002" };
  char buffer[MAXBUFLEN];
  std::chrono::steady_clock::time_point previous_packet_arrival {};
  bool is_first_packet = true;
  while (keep_server_running) {
    
    int received_bytes = recvfrom(receiving_socket.socket_fd, buffer, MAXBUFLEN, 0, nullptr, nullptr);
    if (received_bytes < 0) 
    {
      perror("Failed to receive datagram\n");
      continue;
    }
    buffer[received_bytes] = '\0';

    rtp_header header;
    memcpy(&header, buffer, sizeof(header));
    
    char* payload = new char[received_bytes];
    memcpy(payload, &buffer, received_bytes);

    auto now = std::chrono::steady_clock::now();
    auto jitter = variable_playout_delay_unit(jitter_distribution(random_generator));
    Packet p {
      payload,
      received_bytes,
      now + constant_playout_delay + jitter
    };

    if (is_first_packet)
    {
      previous_packet_arrival = now;
      is_first_packet = false;
    } else 
    {
      auto diff = now - previous_packet_arrival;
      auto diff_in_us = std::chrono::duration_cast<std::chrono::microseconds>(diff);
      
      /*
      Round down to tens of microseconds
      */
      int bucket = (diff_in_us.count() / BUCKET_RESOLUTION);
      if (bucket > BUCKETS) // If larger than highest bucket, put the measurement in the highest bucket 
      {
        bucket = BUCKETS - 1;
      }
      jitter_histogram[bucket]++;
      previous_packet_arrival = now;
    }



    // std::cout << "Received " << header.get_sequence_number() << " with ts " << header.get_timestamp() << "\n";
    // std::cout << "Received " << header.get_sequence_number() << " at " << now.time_since_epoch().count() << "\n";
    
    auto ssq_no = header.get_sequence_number();
    if (ssq_no >= 100 && ssq_no < 110) {
      std::cerr << ssq_no << ", " << now.time_since_epoch().count() << "\n";
      if (ssq_no == 109)
      {
        dump_jitter_histogram_raw();
      } 
    }

    std::lock_guard<std::mutex> lock(network_mutex);
    network_queue.push(p); 
  }
}

void sender(char* to) {
  UdpSocket sending_socket { "9002", to };

  while (keep_server_running) {
    if (!network_queue.empty()) {
      Packet p = network_queue.top(); // .top() doesn't mutate, no need to lock.
      if ((p.send_time - std::chrono::steady_clock::now()).count() <= 0) {
        std::unique_lock<std::mutex> lock(network_mutex);
        network_queue.pop();
        lock.unlock();

        int sent_bytes = sendto(sending_socket.socket_fd, p.data, p.num_bytes_to_send, 0, sending_socket.destaddr->ai_addr, sending_socket.destaddr->ai_addrlen);
        if (sent_bytes < 0 || sent_bytes != p.num_bytes_to_send) // we should proxy the examt same message.
        {
          perror("Failed to send datagram\n");
          continue;
        }
        // std::cout << "Sent " << ((rtp_header*) p.data)->get_sequence_number() << "\n";
        delete p.data;
      }
    } 
  }
}

std::time_t get_current_date(std::chrono::system_clock::time_point tp)
{
  std::chrono::nanoseconds ns_since_epoch(tp.time_since_epoch());
  std::chrono::time_point<std::chrono::system_clock> time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(ns_since_epoch));
  return std::chrono::system_clock::to_time_t(time_point);
}

int main() {  
  char from[5] { "9001" };
  char to[5] { "9024" };
  std::string filepath { "../data/original_timings.txt"};

  auto test_duration { std::chrono::hours(24) };
  auto test_start { std::chrono::system_clock::now() };

  std::thread recv_thread(receiver, from);
  std::thread send_thread(sender, to);

  std::cout << "Started proxy: " << from << "->" << to 
            << " over " << (IPVERSION == AF_INET ? "IPv4" : "IPv6")
            << " with a " << constant_playout_delay.count() << " second propagation delay." << "\n";

  
  std::time_t start_date { get_current_date(test_start) };
  std::cerr << "Session init: " << std::put_time(std::localtime(&start_date), "%Y-%m-%d %H:%M:%S") << "\n";

  std::this_thread::sleep_for(test_duration);
  // signal to stop the threads
  keep_server_running = false;

  std::time_t end_date { get_current_date(std::chrono::system_clock::now()) };
  std::cerr << "Session stopped at: " << std::put_time(std::localtime(&end_date), "%Y-%m-%d %H:%M:%S") << "\n";

  recv_thread.join();
  send_thread.join();

  // dump_jitter_histogram_styled();

  return 0;
}