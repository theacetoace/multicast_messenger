#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <unistd.h>
#include "chat_message.hpp"

using boost::asio::ip::tcp;

bool is_valid_nick = false;

class chat_client {
public:
    chat_client(boost::asio::io_service &io_service,
                boost::asio::steady_timer &my_timer, 
                tcp::resolver::iterator endpoint_iterator)
    : io_service_(io_service), my_timer_(my_timer), socket_(io_service), nick_(nullptr) { 
        do_connect(endpoint_iterator); 
    }

    void write(const chat_message& msg) {
        io_service_.post(
          [this, msg]() {
              bool write_in_progress = !write_msgs_.empty();
              write_msgs_.push_back(msg);
              if (!write_in_progress) { do_write(); }
        });
    }

    void close() {
        io_service_.post([this]() { socket_.close(); });
    }

    void set_nick(char *nick) {
        nick_ = nick;
    }

private:
    void do_connect(tcp::resolver::iterator endpoint_iterator) {
        boost::asio::async_connect(socket_, endpoint_iterator,
            [this](boost::system::error_code ec, tcp::resolver::iterator) {
                if (!ec) { do_read_header(); }
            });
    }

    void do_read_header() {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.data(), chat_message::header_length + chat_message::type_length),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec && read_msg_.decode_header()) {
                    do_read_nick();
                } else {
                    socket_.close();
                }
            });
    }

    void do_read_nick() {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.nick(), chat_message::max_nick_length),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    do_read_body();
                } else {
                    socket_.close();
                }
            });
    }

    void do_read_body() {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    msg_type type = static_cast<msg_type>(*(read_msg_.type()));
                    switch(type) {
                    case MESSAGE:
                        if (nick_ && !std::memcmp(read_msg_.nick(), nick_, std::min(strlen(nick_), read_msg_.nick_length())) 
                            && strlen(nick_) == read_msg_.nick_length()) { break; }
                        std::cout.write(read_msg_.nick(), read_msg_.nick_length());
                        std::cout << ": ";
                        std::cout.write(read_msg_.body(), read_msg_.body_length());
                        std::cout << "\n" << std::flush;
                        break;
                    case NEGATIVE:
                        my_timer_.cancel();
                        break;
                    default:
                        break;
                    }
                    do_read_header();
                } else {
                    socket_.close();
                }
            });
    }

    void do_write() {
        boost::asio::async_write(socket_,
            boost::asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    write_msgs_.pop_front();
                    if (!write_msgs_.empty()) { do_write(); }
                } else  {
                    socket_.close();
                }
            });
    }

private:
    boost::asio::io_service &io_service_;
    boost::asio::steady_timer &my_timer_;
    tcp::socket socket_;
    chat_message read_msg_;
    std::deque<chat_message> write_msgs_;
    char *nick_;
};



void on_timeout(chat_client &c, char *nick, boost::asio::steady_timer &my_timer, const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        is_valid_nick = true;
    }
    if (e == boost::asio::error::operation_aborted) {
        std::cout << "Sorry this nickname is unvailable,\n Please choose nickname[max " <<
            chat_message::max_nick_length << " characters]: " << std::flush;
        std::cin.getline(nick, chat_message::max_nick_length + 1);
        c.write(create_msg("", nick, QUERY));
        my_timer.async_wait([&c, nick, &my_timer](const boost::system::error_code &ec) {
            on_timeout(c, nick, my_timer, ec);
        });
    }
}

int main(int argc, char* argv[]) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: chat_client <host> <port>\n";
            return 1;
        }

        boost::asio::io_service io_service;
        boost::asio::steady_timer my_timer(io_service, std::chrono::milliseconds(3000));

        tcp::resolver resolver(io_service);
        auto endpoint_iterator = resolver.resolve({ argv[1], argv[2] });
        chat_client c(io_service, my_timer, endpoint_iterator);

        std::thread t([&io_service](){ io_service.run(); });

        char *nick = new char[chat_message::max_nick_length + 1];
        
        std::cout << "Please choose nickname[max " << chat_message::max_nick_length << " characters]: " << std::flush;
        std::cin.getline(nick, chat_message::max_nick_length + 1);
        c.write(create_msg("", nick, QUERY));

        my_timer.async_wait([&c, nick, &my_timer](const boost::system::error_code &ec) {
            on_timeout(c, nick, my_timer, ec);
        });

        while(!is_valid_nick) { sleep(1); }

        std::cout << "Welcome to the chat =) Maximum message characters is " << chat_message::max_body_length << std::endl;

        c.set_nick(nick);

        char *line = new char[chat_message::max_body_length + 1];

        while (true) {
            if (!std::cin.getline(line, chat_message::max_body_length + 1)) { break; }
            c.write(create_msg(line, nick, MESSAGE));
        }

        c.close();

        delete[] line;
        delete[] nick;
        
        t.join();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}