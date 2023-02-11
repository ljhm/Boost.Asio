// ./boost_1_81_0/doc/html/boost_asio/example/cpp11/timeouts/async_tcp_client.cpp

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <functional>
#include <iostream>
#include <string>
#include <sanitizer/lsan_interface.h>

std::string client_tag; //test

struct session
  : public std::enable_shared_from_this<session>
{
  session(boost::asio::ip::tcp::socket socket)
    : socket(std::move(socket))
  { }

  void start() {
    start_read();
    start_write();
  }

  void start_read() {
    auto self(shared_from_this());
    memset(input_data, 0, sizeof(input_data));
    socket.async_read_some(
      boost::asio::buffer(input_data, sizeof(input_data)),
      [this, self](boost::system::error_code ec, std::size_t length) {
        handle_read(ec, length);
      }
    );
  }

  void handle_read(const boost::system::error_code& ec,
    std::size_t length) {
    if (!ec) {
      std::cout << input_data;
      start_read();
    } else {
      std::cout << ec.message() << "\n";
    }
  }

  void start_write() {
    auto self(shared_from_this());
    memset(output_data, 0, sizeof(output_data));
    snprintf(output_data, sizeof(output_data) - 1,
      "hello server %s %zu\n", client_tag.c_str(), cnt++);
    boost::asio::async_write(
      socket,
      boost::asio::buffer(output_data, sizeof(input_data)),
      [this, self](boost::system::error_code ec, std::size_t length)
      {
        handle_write(ec, length);
      }
    );
  }

  void handle_write(const boost::system::error_code& ec,
    std::size_t length)
  {
    if (!ec) {
      // sleep(1); //test
      start_write();
    } else {
      std::cout << ec.message() << "\n";
    }
  }

  boost::asio::ip::tcp::socket socket;
  enum { LEN = 1024 };
  char input_data[LEN];
  char output_data[LEN];
  size_t cnt = 0;
};

struct connector {
  connector(boost::asio::io_context& io_context)
    : socket(io_context)
  { }

  void start(boost::asio::ip::tcp::resolver::results_type endpoints)
  {
    endpoints_ = endpoints;
    start_connect(endpoints_.begin());
  }

  void start_connect(
    boost::asio::ip::tcp::resolver::results_type::iterator
    endpoint_iter)
  {
    if (endpoint_iter != endpoints_.end()) {
      socket.async_connect(
        endpoint_iter->endpoint(),
        [=](const boost::system::error_code ec)
        {
          handle_connect(ec, endpoint_iter);
        }
      );
    }
  }

  void handle_connect(const boost::system::error_code& ec,
    boost::asio::ip::tcp::resolver::results_type::iterator
    endpoint_iter)
  {
    if (!socket.is_open()) {
      std::cout << "Connect timed out\n";
      start_connect(++endpoint_iter);
    } else if (ec) {
      std::cout << "Connect error: " << ec.message() << "\n";
      socket.close();
      start_connect(++endpoint_iter);
    } else {
      std::cout << "Connected to " << endpoint_iter->endpoint() << "\n";
      std::make_shared<session>(std::move(socket))->start();
    }
  }

  boost::asio::ip::tcp::resolver::results_type endpoints_;
  boost::asio::ip::tcp::socket socket;
};

void handlerCont(int signum){
  printf("SIGCONT %d\n", signum);
#ifndef NDEBUG
  __lsan_do_recoverable_leak_check();
#endif
}

int main(int argc, char* argv[]) {
  if (argc != 4) {
    std::cerr << "Usage: client <host> <port> <tag>\n";
    return 1;
  }

  signal(SIGCONT, handlerCont); // $ man 7 signal
  client_tag = argv[3];

  boost::asio::io_context io_context;
  boost::asio::ip::tcp::resolver r(io_context);
  connector c(io_context);
  c.start(r.resolve(argv[1], argv[2]));
  io_context.run();

  return 0;
}
