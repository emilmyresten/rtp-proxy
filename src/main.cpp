#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <float.h>
#include <utility>
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

const double nanos_per_90kHz = 11111.1111111;

auto test_duration { std::chrono::hours(1) };

const int DATA_COLL_FREQ = 512;

const int MAXBUFLEN = 1024; // max pkt_size should be specified by ffmpeg.
std::mutex network_mutex; // protect the priority queue

/*
Creates a histogram with the distribution of packet delay variation.
Measured in microseconds (us), and stored in BUCKETS buckets. Each bucket represents BUCKET_RESOLUTION us, everything rounded down to nearest hundredth.
As an example; A packet that arrives 686 us after the previous, will be rounded to 600 us and 'put' in bucket 6.
*/
const int BUCKET_RESOLUTION = 10; // 10 microseconds bucket resolution
const int BUCKETS = 2000;
std::vector<uint64_t> jitter_histogram(BUCKETS);

struct Packet {
  char* data;
  int num_bytes_to_send; // the bytes received by recvfrom
  std::chrono::time_point<std::chrono::steady_clock> send_time;
};

std::queue<Packet> network_queue{}; 

std::atomic<bool> keep_server_running{true};

struct interarrival_jitter
{
  uint64_t previous_transit = 0;
  double estimate = 0;
};

void add_to_jitter_histogram(std::chrono::steady_clock::time_point now, std::chrono::steady_clock::time_point prev)
{
  auto diff = now - prev;
  auto diff_in_us = std::chrono::duration_cast<std::chrono::microseconds>(diff);
      
  /*
  Round down to BUCKET_RESOLUTION of microseconds
  */
  int bucket = (diff_in_us.count() / BUCKET_RESOLUTION);
  if (bucket > BUCKETS) // If larger than highest bucket, put the measurement in the highest bucket 
  {
    bucket = BUCKETS - 1;
  }
  jitter_histogram[bucket]++;
}

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
  std::ostringstream o;
  o << "jitter_histogram: ";
  for (auto bucket : jitter_histogram)
  {
    o << bucket << ',';
  }
  o << '\n';
  std::cerr << o.str();
}

/*
Inter-arrival jitter estimate according to RFC3550 https://www.rfc-editor.org/rfc/rfc3550#appendix-A.8
*/
void update_jitter_estimate(interarrival_jitter* j, double transit)
{
  auto d = transit - j->previous_transit;
  j->previous_transit = transit;
  if (d < 0) { d = - d; }
  j->estimate += (1.0/16.0) * ((double) d - j->estimate);
}

void update_jitter_max_min(double* min, double* max, double transit)
{
  if (transit < *min)
  {
    *min = transit;
  }
  if (transit > *max)
  {
    *max = transit;
  }
}

void receiver(char* from, char* via) {
  UdpSocket receiving_socket { from };
  char buffer[MAXBUFLEN];
  std::chrono::steady_clock::time_point previous_packet_arrival {};
  bool is_first_packet = true;

  interarrival_jitter rfc3550_jitter{};

  double min_delay{};
  double max_delay{};

  uint64_t packets_received = 0;
  uint64_t reordered_packets = 0;
  uint32_t previous_ts = 0;

  while (keep_server_running) {
    
    int received_bytes = recvfrom(receiving_socket.socket_fd, buffer, MAXBUFLEN, 0, nullptr, nullptr);
    if (received_bytes < 0) 
    {
      perror("Failed to receive datagram\n");
      continue;
    }
    buffer[received_bytes] = '\0';
    packets_received++;

    rtp_header header;
    memcpy(&header, buffer, sizeof(header));
    
    char* payload = new char[received_bytes];
    memcpy(payload, &buffer, received_bytes);

    auto now = std::chrono::steady_clock::now();
    auto transit = now.time_since_epoch().count() - (header.get_timestamp() * nanos_per_90kHz);
    Packet p {
      payload,
      received_bytes,
      now
    };

    if (is_first_packet)
    {
      previous_packet_arrival = now;
      is_first_packet = false;
      rfc3550_jitter.previous_transit = transit;
      min_delay = transit;
      max_delay = transit;
    } else 
    {
      // add_to_jitter_histogram(now, previous_packet_arrival);
      update_jitter_estimate(&rfc3550_jitter, transit);
      update_jitter_max_min(&min_delay, &max_delay, transit);
      previous_packet_arrival = now;
      // std::cerr << "inter-arrival jitter at " << header.get_sequence_number() << ": " << rfc3550_jitter.estimate << "ns\n";
    }

    if (header.get_timestamp() < previous_ts)
    {
      reordered_packets++;
    }
    previous_ts = header.get_timestamp();

    // std::cout << rfc3550_jitter.previous_transit << "\n";

    // std::cout << "Received " << header.get_sequence_number() << " with ts " << header.get_timestamp() << "\n";
    // std::cout << "Received " << header.get_sequence_number() << " at " << now.time_since_epoch().count() << "\n";
    
    auto ssq_no = header.get_sequence_number();
    if (ssq_no % DATA_COLL_FREQ == 0) {
      std::cerr << "drift-measure: " << now.time_since_epoch().count() << "\n";
      std::cerr << "inter-arrival jitter (ns): " << rfc3550_jitter.estimate << "\n";
      std::cerr << "maximum-jitter (ns): " << max_delay - min_delay << "\n";
      std::cerr << "reorder-ratio: " << (double) reordered_packets / packets_received << "\n";
      max_delay = 0; min_delay = DBL_MAX; // reset to perform calculation for next section
    }

    std::lock_guard<std::mutex> lock(network_mutex);
    network_queue.push(p); 
  }
}

void sender(char* via, char* to_ip, char* to_port) {
  UdpSocket sending_socket { via, to_ip, to_port };

  while (keep_server_running) {
    if (!network_queue.empty()) {
      Packet p = network_queue.front();
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
void measurer(char* from) 
{
  UdpSocket receiving_socket { from };
  char buffer[MAXBUFLEN];
  std::chrono::steady_clock::time_point previous_packet_arrival {};
  bool is_first_packet = true;  

  interarrival_jitter rfc3550_jitter{};

  double min_delay{};
  double max_delay{};

  uint64_t packets_received = 0;
  uint64_t reordered_packets = 0;
  uint32_t previous_ts = 0;

  while (keep_server_running) {
    
    int received_bytes = recvfrom(receiving_socket.socket_fd, buffer, MAXBUFLEN, 0, nullptr, nullptr);
    if (received_bytes < 0) 
    {
      perror("Failed to receive datagram\n");
      continue;
    }
    buffer[received_bytes] = '\0';
    packets_received++;

    rtp_header header;
    memcpy(&header, buffer, sizeof(header));
    
    auto now = std::chrono::steady_clock::now();
    auto transit = now.time_since_epoch().count() - (header.get_timestamp() * nanos_per_90kHz);
    if (is_first_packet)
    {
      previous_packet_arrival = now;
      is_first_packet = false;
      rfc3550_jitter.previous_transit = transit;
      min_delay = transit;
      max_delay = transit;
    } else 
    {
      // add_to_jitter_histogram(now, previous_packet_arrival);
      update_jitter_estimate(&rfc3550_jitter, transit);
      update_jitter_max_min(&min_delay, &max_delay, transit);
      previous_packet_arrival = now;
    }

    if (header.get_timestamp() < previous_ts)
    {
      reordered_packets++;
    }
    previous_ts = header.get_timestamp();
    
    // std::cout << "Received " << header.get_sequence_number() << " with ts " << header.get_timestamp() << "\n";
    // std::cout << "Received " << header.get_sequence_number() << " at " << now.time_since_epoch().count() << "\n";
    
    auto seq_no = header.get_sequence_number();
    if (seq_no % DATA_COLL_FREQ == 0) 
    {
      std::cerr << "drift-measure: " << now.time_since_epoch().count() << "\n";
      std::cerr << "inter-arrival jitter (ns): " << rfc3550_jitter.estimate << "\n";
      std::cerr << "maximum-jitter (ns): " << max_delay - min_delay << "\n";
      std::cerr << "reorder-ratio: " << (double) reordered_packets / packets_received << "\n";
      max_delay = 0; min_delay = DBL_MAX; // reset to perform calculation for next section
    }
  }
}

std::time_t get_current_date(std::chrono::system_clock::time_point tp)
{
  std::chrono::nanoseconds ns_since_epoch(tp.time_since_epoch());
  std::chrono::time_point<std::chrono::system_clock> time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(ns_since_epoch));
  return std::chrono::system_clock::to_time_t(time_point);
}

void run_as_proxy(char* argv[])
{
  char* from { argv[1] };
  char* to_ip { argv[2] };
  char* to_port { argv[3] };
  char* via { argv[4] };

  auto test_start { std::chrono::system_clock::now() };

  std::thread recv_thread(receiver, from, via);
  std::thread send_thread(sender, via, to_ip, to_port);

  std::cout << "Started proxy: " << from << " -> " << to_ip << ":" << to_port << " via " << via
            << " over " << (IPVERSION == AF_INET ? "IPv4" : "IPv6");

  
  std::time_t start_date { get_current_date(test_start) };
  std::cerr << "Session init: " << std::put_time(std::localtime(&start_date), "%Y-%m-%d %H:%M:%S") << "\n";

  std::this_thread::sleep_for(test_duration);
  // signal to stop the threads
  keep_server_running = false;

  std::time_t end_date { get_current_date(std::chrono::system_clock::now()) };
  std::cerr << "Session stopped at: " << std::put_time(std::localtime(&end_date), "%Y-%m-%d %H:%M:%S") << "\n";

  recv_thread.join();
  send_thread.join();

}


void run_as_util(char* argv[])
{
  char* port { argv[1] };

  auto test_start { std::chrono::system_clock::now() };

  std::thread measure_thread(measurer, port);

  std::cout << "Started point-of-measure on " << port
            << " over " << (IPVERSION == AF_INET ? "IPv4" : "IPv6") << "\n";

  
  std::time_t start_date { get_current_date(test_start) };
  std::cerr << "Session init: " << std::put_time(std::localtime(&start_date), "%Y-%m-%d %H:%M:%S") << "\n";

  std::this_thread::sleep_for(test_duration + std::chrono::seconds(5)); // run the final data-point longer to allow all packets to be delivered.
  // signal to stop the threads
  keep_server_running = false;

  std::time_t end_date { get_current_date(std::chrono::system_clock::now()) };
  std::cerr << "Session stopped at: " << std::put_time(std::localtime(&end_date), "%Y-%m-%d %H:%M:%S") << "\n";

  measure_thread.join();
}


int main(int argc, char* argv[]) 
{
  if (argc-1 != 1 && argc-1 != 4)
  {
    std::cout << "Supply 1 argument for measurement only, 4 arguments for proxy. ./main <from> [<to_ip> <to_port> <via>]\n";
    exit(1);
  }

  bool is_proxy = argc-1 == 4 ? true : false;
  if (is_proxy)
  {
    run_as_proxy(argv);
  } else
  {
    run_as_util(argv);
  }

  // dump_jitter_histogram_styled();
  // dump_jitter_histogram_raw(); 

  return 0;
}