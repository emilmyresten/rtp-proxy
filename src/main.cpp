#include <iostream>
#include <vector>
#include <math.h>
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

auto test_duration { std::chrono::hours(24) };

const int MAXBUFLEN = 1024; // max pkt_size should be specified by ffmpeg.
std::mutex network_mutex; // protect the priority queue

auto constant_playout_delay = std::chrono::milliseconds(0);
using variable_playout_delay_unit = std::chrono::microseconds;
std::default_random_engine random_generator(1); // explicitly seed the random generator for clarity.

// PDV with Gaussian/Normal/Binomial probability density function. Haven't found real support in the literature for values of mean and stddev.
std::uniform_int_distribution<int> jitter_distribution(400, 800); // jitter as 1-2% of constant delay factor.

/* 
input when doing inverse transform sampling
*/
std::uniform_real_distribution<> uniform_dist(0.0, 1.0);

/*
Model end-to-end delay using pareto distribution according to "Modeling End-to-End Delay Using Pareto Distribution" by Zhang et. al.
*/
double xm = 40;
double k = 20;
std::pair<uint16_t, uint16_t> end_to_end_delay()
{  
  auto urnd = uniform_dist(random_generator);
  // inverse transform sampling of Pareto distribution using its CDF
  auto res =  xm * pow((1 - urnd),(-1/k));
  uint16_t whole_factor = (uint16_t) res;
  uint16_t decimal_factor = (uint16_t) round((res - whole_factor) * 1'000);
  return std::make_pair(whole_factor, decimal_factor);
}

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

auto cmp = [](Packet left, Packet right)
{
    auto now = std::chrono::steady_clock::now();
    auto left_delta = left.send_time - now;
    auto right_delta = right.send_time - now;
    return left_delta > right_delta;
};
std::priority_queue<Packet, std::vector<Packet>, decltype(cmp)> network_queue(cmp); 

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
  std::cerr << "- jitter_histogram start: \n";
  for (int bucket = 0; bucket < jitter_histogram.size(); bucket++)
  {
    std::cerr << bucket << "," << jitter_histogram[bucket] << "\n";
  }
  std::cerr << "- jitter_histogram end\n";
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

void receiver(char* from, char* via) {
  UdpSocket receiving_socket { from, via };
  char buffer[MAXBUFLEN];
  std::chrono::steady_clock::time_point previous_packet_arrival {};
  bool is_first_packet = true;

  interarrival_jitter rfc3550_jitter{};

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
    // auto jitter = variable_playout_delay_unit(jitter_distribution(random_generator));
    auto delay = end_to_end_delay();
    Packet p {
      payload,
      received_bytes,
      // now + std::chrono::milliseconds(delay.first) + std::chrono::microseconds(delay.second)
      now + constant_playout_delay
    };

    if (is_first_packet)
    {
      previous_packet_arrival = now;
      is_first_packet = false;
      rfc3550_jitter.previous_transit = now.time_since_epoch().count() - (header.get_sequence_number() * nanos_per_90kHz);
    } else 
    {
      add_to_jitter_histogram(now, previous_packet_arrival);
      auto transit = now.time_since_epoch().count() - (header.get_sequence_number() * nanos_per_90kHz);
      update_jitter_estimate(&rfc3550_jitter, transit);
      previous_packet_arrival = now;
    }

    // std::cout << rfc3550_jitter.previous_transit << "\n";

    // std::cout << "Received " << header.get_sequence_number() << " with ts " << header.get_timestamp() << "\n";
    // std::cout << "Received " << header.get_sequence_number() << " at " << now.time_since_epoch().count() << "\n";
    
    auto ssq_no = header.get_sequence_number();
    if (ssq_no == 100) {
      std::cerr << "drift-measure: " << now.time_since_epoch().count() << "\n";
      dump_jitter_histogram_raw(); 
      std::cerr << "inter-arrival jitter: " << rfc3550_jitter.estimate << "ns\n";
    }

    std::lock_guard<std::mutex> lock(network_mutex);
    network_queue.push(p); 
  }
}

void sender(char* to, char* via) {
  UdpSocket sending_socket { via, to };

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
void measurer(char* from) 
{
  UdpSocket receiving_socket { from, "1234" };
  char buffer[MAXBUFLEN];
  std::chrono::steady_clock::time_point previous_packet_arrival {};
  bool is_first_packet = true;  

  interarrival_jitter rfc3550_jitter{};

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
    
    auto now = std::chrono::steady_clock::now();
    if (is_first_packet)
    {
      previous_packet_arrival = now;
      is_first_packet = false;
      rfc3550_jitter.previous_transit = now.time_since_epoch().count() - (header.get_sequence_number() * nanos_per_90kHz);
    } else 
    {
      add_to_jitter_histogram(now, previous_packet_arrival);
      auto transit = now.time_since_epoch().count() - (header.get_sequence_number() * nanos_per_90kHz);
      update_jitter_estimate(&rfc3550_jitter, transit);
      previous_packet_arrival = now;
    }

    // std::cout << "Received " << header.get_sequence_number() << " with ts " << header.get_timestamp() << "\n";
    // std::cout << "Received " << header.get_sequence_number() << " at " << now.time_since_epoch().count() << "\n";
    
    auto seq_no = header.get_sequence_number();
    if (seq_no == 100) 
    {
      std::cerr << "drift-measure: " << now.time_since_epoch().count() << "\n";
      dump_jitter_histogram_raw(); 
      std::cerr << "inter-arrival jitter: " << rfc3550_jitter.estimate << "ns\n";
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
  char* to { argv[2] };
  char* via { argv[3] };

  auto test_start { std::chrono::system_clock::now() };

  std::thread recv_thread(receiver, from, via);
  std::thread send_thread(sender, to, via);

  std::cout << "Started proxy: " << from << "->" << to << " via " << via
            << " over " << (IPVERSION == AF_INET ? "IPv4" : "IPv6")
            << " with a " << constant_playout_delay.count() << " ms delay.\n";

  
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

  std::this_thread::sleep_for(test_duration);
  // signal to stop the threads
  keep_server_running = false;

  std::time_t end_date { get_current_date(std::chrono::system_clock::now()) };
  std::cerr << "Session stopped at: " << std::put_time(std::localtime(&end_date), "%Y-%m-%d %H:%M:%S") << "\n";

  measure_thread.join();
}


int main(int argc, char* argv[]) 
{
  if (argc-1 != 1 && argc-1 != 3)
  {
    std::cout << "Supply 1 argument for measurement only, 3 arguments for proxy. ./main <from> [<to> <via>]\n";
    exit(1);
  }

  bool is_proxy = argc-1 == 3 ? true : false;
  if (is_proxy)
  {
    run_as_proxy(argv);
  } else
  {
    run_as_util(argv);
  }

  // dump_jitter_histogram_styled();

  return 0;
}