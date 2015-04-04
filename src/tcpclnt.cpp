#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <unistd.h>
#include "../include/chat_message.hpp"

using boost::asio::ip::tcp;

/** 
@file tcpclnt.cpp
Messenger client implementation
*/

/// global variable that shows if current nick is valid
bool is_valid_nick = false;

/**
@class chat_client
class stores client messages, socket, nick and implements
writing and reading methods
*/
class chat_client {
public:
    /// Constructor
    /// starts connection
    /// @param io_service is boost::asio io_service that runs in separate thread
    /// @param my_timer is timer that is neccasary to check nick availability
    /// @param endpoint_iterator iterates through server addresses until connect
    chat_client(boost::asio::io_service &io_service,
                boost::asio::steady_timer &my_timer, 
                tcp::resolver::iterator endpoint_iterator)
    : io_service_(io_service), my_timer_(my_timer), socket_(io_service), nick_(nullptr) { 
        do_connect(endpoint_iterator); 
    }

    /// method starts writing message through connection
    /// @param msg is message to send
    void write(const chat_message& msg) {
        io_service_.post(
          [this, msg]() {
              bool write_in_progress = !write_msgs_.empty();
              write_msgs_.push_back(msg);
              if (!write_in_progress) { do_write(); }
        });
    }

    /// method that closes connection
    void close() {
        io_service_.post([this]() { socket_.close(); });
    }

    /// method that sets pointer to nickname
    /// @param nick is pointer to nickname
    void set_nick(char *nick) {
        nick_ = nick;
    }

private:
    /// method tries to establish connection
    /// if connection is established thah starts reading
    /// @param endpoint_iterator iterator thats points to server parameters
    void do_connect(tcp::resolver::iterator endpoint_iterator) {
        boost::asio::async_connect(socket_, endpoint_iterator,
            [this](boost::system::error_code ec, tcp::resolver::iterator) {
                if (!ec) { do_read_header(); }
            });
    }

    /// method starts reading from message header if something is in the socket
    /// if header is read then method starts reading nickname
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

    /// method continues reading by reading nickname from the socket
    /// if nickname is read the method starts reading message body
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

    /// method implements the last reading part of the message
    /// if body is read method analyzes message type; if message is ususal
    /// then if nick isn't the same as client's one than message is printed to stdout, 
    /// then room is asked if nickname from the query is available and if it is not
    /// if message type is query then if message type is negative, i.e. nickname is unavailable
    /// method cancels timer that caused next iteration of nickname setting
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

    /// method writes message to the socket
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
    /// boost::asio io_service that maintains connection
    boost::asio::io_service &io_service_;
    /// timer that maintains nickname setting
    boost::asio::steady_timer &my_timer_;
    /// i/o socket of the client
    tcp::socket socket_;
    /// local cantainer for the read message
    chat_message read_msg_;
    /// local cantainer for the send messages
    std::deque<chat_message> write_msgs_;
    /// pointer to the nickname
    char *nick_;
};

//----------------------------------------------------------------------
/**
@function on_timeout
timer handler that either asks for new nickname if timer was canceled or
sets global variable is_valid_nick to true that starts process of writing to the messanger
@param c is client to write query
@param nick is pointer to c string that was entered by user
@param my_timer timer that restarts if it was canceled
@param e is error code that stores if timer was canceled 
*/
void on_timeout(chat_client &c, char *nick, boost::asio::steady_timer &my_timer, const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        is_valid_nick = true;
    }
    if (e == boost::asio::error::operation_aborted) {
        std::cout << "Sorry this nickname is unavailable,\nPlease choose nickname[max " <<
            chat_message::max_nick_length << " characters]: " << std::flush;
        std::cin.getline(nick, chat_message::max_nick_length + 1);
        c.write(create_msg("", nick, QUERY));
        my_timer.async_wait([&c, nick, &my_timer](const boost::system::error_code &ec) {
            on_timeout(c, nick, my_timer, ec);
        });
    }
}

//----------------------------------------------------------------------

/**
@function main
handles user writing and starts connection
@param argv is chat_client <host> <port>
*/
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