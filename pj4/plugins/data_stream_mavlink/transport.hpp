/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <array>
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <pj_base/expected.hpp>
#include <pj_plugins/sdk/streaming_source.hpp>
#include <string>
#include <thread>
#include <vector>

namespace mavlink_detail {

/// Bytes read from the link plus the wall-clock time (ns since epoch) they were received at.
struct Chunk {
  std::vector<uint8_t> data;
  int64_t timestamp_ns = 0;
};

inline int64_t nowNs() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

/// Receive-only MAVLink link (UDP, TCP client or serial). A background io thread pushes what arrives into `queue`;
/// the owner drains it from onPoll().
class Transport {
 public:
  virtual ~Transport() = default;

  virtual PJ::Status open() = 0;
  virtual void close() = 0;

  PJ::sdk::DrainQueue<Chunk> queue;
};

/// Shared io_context + thread plumbing for the three links.
class AsioTransport : public Transport {
 public:
  // No destructor work here: the derived transports call stopIo() in their own destructors, while their socket still
  // exists (a virtual call from this destructor would hit the already-destroyed derived part).
  void close() override {
    stopIo();
  }

 protected:
  void startIo() {
    io_thread_ = std::thread([this] { io_.run(); });
  }

  // Idempotent. Stops and joins the io thread first, so the handle is closed while no handler is running.
  void stopIo() {
    stopped_ = true;
    io_.stop();
    if (io_thread_.joinable()) {
      io_thread_.join();
    }
    asio::error_code ec;
    closeHandle(ec);
  }

  void push(const uint8_t* data, size_t size) {
    queue.push({std::vector<uint8_t>(data, data + size), nowNs()});
  }

  virtual void closeHandle(asio::error_code& ec) = 0;

  asio::io_context io_;
  std::atomic<bool> stopped_{false};
  std::array<uint8_t, 65536> buffer_{};

 private:
  std::thread io_thread_;
};

/// UDP: binds AnyIPv4:port and reads datagrams.
class UdpTransport : public AsioTransport {
 public:
  explicit UdpTransport(uint16_t port) : port_(port), socket_(io_) {}
  ~UdpTransport() override {
    stopIo();
  }

  PJ::Status open() override {
    asio::error_code ec;
    socket_.open(asio::ip::udp::v4(), ec);
    if (!ec) {
      socket_.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), port_), ec);
    }
    if (ec) {
      return PJ::unexpected(ec.message());
    }
    receive();
    startIo();
    return PJ::okStatus();
  }

 private:
  void receive() {
    socket_.async_receive_from(asio::buffer(buffer_), sender_, [this](asio::error_code ec, size_t n) {
      if (ec || stopped_) {
        return;
      }
      if (n > 0) {
        push(buffer_.data(), n);
      }
      receive();
    });
  }

  void closeHandle(asio::error_code& ec) override {
    socket_.close(ec);
  }

  uint16_t port_;
  asio::ip::udp::socket socket_;
  asio::ip::udp::endpoint sender_;
};

/// TCP client: connects to host:port (5 s timeout) and reads the byte stream.
class TcpTransport : public AsioTransport {
 public:
  TcpTransport(std::string host, uint16_t port) : host_(std::move(host)), port_(port), socket_(io_) {}
  ~TcpTransport() override {
    stopIo();
  }

  PJ::Status open() override {
    const std::string target = host_ + ":" + std::to_string(port_);
    asio::error_code ec;
    asio::ip::tcp::resolver resolver(io_);
    const auto endpoints = resolver.resolve(host_, std::to_string(port_), ec);
    if (ec) {
      return PJ::unexpected("Cannot connect to " + target + ": " + ec.message());
    }

    asio::error_code connect_ec = asio::error::would_block;
    asio::async_connect(socket_, endpoints, [&](asio::error_code result, const asio::ip::tcp::endpoint&) {
      connect_ec = result;
    });
    io_.restart();
    io_.run_for(std::chrono::seconds(5));
    if (connect_ec == asio::error::would_block) {
      connect_ec = asio::error::timed_out;
    }
    if (connect_ec) {
      asio::error_code ignored;
      socket_.close(ignored);
      return PJ::unexpected("Cannot connect to " + target + ": " + connect_ec.message());
    }

    io_.restart();
    receive();
    startIo();
    return PJ::okStatus();
  }

 private:
  void receive() {
    socket_.async_read_some(asio::buffer(buffer_.data(), 4096), [this](asio::error_code ec, size_t n) {
      if (ec || stopped_) {
        return;
      }
      if (n > 0) {
        push(buffer_.data(), n);
      }
      receive();
    });
  }

  void closeHandle(asio::error_code& ec) override {
    socket_.close(ec);
  }

  std::string host_;
  uint16_t port_;
  asio::ip::tcp::socket socket_;
};

/// Serial port, 8N1, no flow control.
class SerialTransport : public AsioTransport {
 public:
  SerialTransport(std::string system_location, std::string display_name, unsigned baud)
      : system_location_(std::move(system_location)),
        display_name_(std::move(display_name)),
        baud_(baud),
        port_(io_) {}
  ~SerialTransport() override {
    stopIo();
  }

  PJ::Status open() override {
    asio::error_code ec;
    port_.open(system_location_, ec);
    if (!ec) {
      port_.set_option(asio::serial_port_base::baud_rate(baud_), ec);
    }
    if (!ec) {
      port_.set_option(asio::serial_port_base::character_size(8), ec);
    }
    if (!ec) {
      port_.set_option(asio::serial_port_base::parity(asio::serial_port_base::parity::none), ec);
    }
    if (!ec) {
      port_.set_option(asio::serial_port_base::stop_bits(asio::serial_port_base::stop_bits::one), ec);
    }
    if (!ec) {
      port_.set_option(asio::serial_port_base::flow_control(asio::serial_port_base::flow_control::none), ec);
    }
    if (ec) {
      asio::error_code ignored;
      port_.close(ignored);
      return PJ::unexpected("Cannot open " + display_name_ + ": " + ec.message());
    }
    receive();
    startIo();
    return PJ::okStatus();
  }

 private:
  void receive() {
    port_.async_read_some(asio::buffer(buffer_.data(), 4096), [this](asio::error_code ec, size_t n) {
      if (ec || stopped_) {
        return;
      }
      if (n > 0) {
        push(buffer_.data(), n);
      }
      receive();
    });
  }

  void closeHandle(asio::error_code& ec) override {
    port_.close(ec);
  }

  std::string system_location_;
  std::string display_name_;
  unsigned baud_;
  asio::serial_port port_;
};

}  // namespace mavlink_detail
