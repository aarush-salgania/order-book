// merkle_logger_main.cpp - Standalone Verifiable Audit Logger Child Process
// Completely decoupled from the core matching engine process.
// Consumes ExecutionEvents via Shared Memory (with pipe fallback), generates
// in-place Merkle trees, persists an immutable binary audit trail, and emits
// cryptographic Merkle State Roots.
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "merkle.hpp"
#include "shm_ring_buffer.hpp"

namespace {

std::string hash_to_hex(const Hash& h) {
  std::ostringstream oss;
  oss << "0x";
  for (int i = 0; i < 4; ++i) {
    oss << std::hex << std::setw(16) << std::setfill('0') << h.w[i];
  }
  return oss.str();
}

}  // namespace

int main(int argc, char* argv[]) {
  bool pipe_mode = false;
  size_t batch_size = 64;  // Emit a Merkle root every 64 events (or on timeout)

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--pipe") pipe_mode = true;
    if (arg.rfind("--batch=", 0) == 0) {
      batch_size = std::stoul(arg.substr(8));
    }
  }

  std::cerr << "[merkle_logger] Starting Verifiable Audit Logger (batch=" << batch_size 
            << ", mode=" << (pipe_mode ? "pipe" : "shared_memory") << ")" << std::endl;

  // Open append-only binary audit log file
  std::ofstream audit_file("audit_trail.bin", std::ios::binary | std::ios::app);
  if (!audit_file.is_open()) {
    std::cerr << "[merkle_logger] Warning: Could not open audit_trail.bin for writing" << std::endl;
  }

  ob::ShmConsumer consumer;
  if (!pipe_mode) {
    // Retry connecting to shared memory if engine hasn't started yet
    for (int retry = 0; retry < 20 && !consumer.is_shared_memory(); ++retry) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      consumer.try_connect();
    }
  }

  std::vector<Hash> leaves(batch_size + 1);
  size_t leaf_count = 0;
  uint64_t batch_index = 1;
  uint64_t start_seq = 0;
  uint64_t current_seq = 0;

  auto flush_merkle_batch = [&]() {
    if (leaf_count == 0) return;
    const Hash root = merkle_root(leaves.data(), leaf_count);
    const std::string root_hex = hash_to_hex(root);
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();

    std::cout << "MERKLE_ROOT: {"
              << "\"batch\":" << batch_index++
              << ",\"count\":" << leaf_count
              << ",\"start_seq\":" << start_seq
              << ",\"end_seq\":" << current_seq
              << ",\"root\":\"" << root_hex << "\""
              << ",\"timestamp\":" << now_ms
              << "}" << std::endl;

    if (audit_file.is_open()) audit_file.flush();
    leaf_count = 0;
  };

  auto process_event = [&](const ExecutionEvent& ev) {
    current_seq = ev.ingress_id;
    if (leaf_count == 0) start_seq = current_seq;

    // Append raw event to binary audit file
    if (audit_file.is_open()) {
      audit_file.write(reinterpret_cast<const char*>(&ev), sizeof(ExecutionEvent));
    }

    // Hash leaf into Merkle batch
    leaves[leaf_count++] = hash_leaf(ev);

    if (leaf_count >= batch_size) {
      flush_merkle_batch();
    }
  };

  std::cerr << "[merkle_logger] Ready and listening for execution state transitions" << std::endl;

  auto last_flush = std::chrono::steady_clock::now();

  while (!consumer.is_shutdown_requested()) {
    size_t consumed = 0;

    if (consumer.is_shared_memory()) {
      consumed = consumer.consume(process_event, 256);
    } else if (pipe_mode) {
      // Pipe fallback: read binary ExecutionEvent from stdin
      ExecutionEvent ev{};
      if (std::cin.read(reinterpret_cast<char*>(&ev), sizeof(ExecutionEvent))) {
        process_event(ev);
        consumed = 1;
      }
    } else {
      // Shared memory not found yet, retry
      if (consumer.try_connect()) {
        continue;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (consumed > 0) {
      last_flush = std::chrono::steady_clock::now();
    } else {
      // Flush partial batch if idle for > 250ms
      if (leaf_count > 0) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_flush).count() > 250) {
          flush_merkle_batch();
        }
      }
      cpu_pause();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  // Final flush on shutdown
  if (leaf_count > 0) {
    flush_merkle_batch();
  }

  std::cerr << "[merkle_logger] Shutdown complete" << std::endl;
  return 0;
}
