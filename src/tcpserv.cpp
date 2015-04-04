#include <iostream>
#include <deque>
#include <memory>
#include <set>
#include <map>
#include <utility>
#include <boost/asio.hpp>
#include "../include/chat_message.hpp"

/**
@mainpage Multicast Messenger
*/

/** 
@file tcpserv.cpp
Messenger server implementation
*/

using boost::asio::ip::tcp;

/** 
@class chat_participant
abstract class of messenger participant that allows to deliver message to participant
*/
class chat_participant {
public:
  /// destructor
  /// virtual destructor
    virtual ~chat_participant() {}

    /// method allows message sending to participant
    /// @param msg is message to sent
    virtual void deliver(const chat_message& msg) = 0;
};

//----------------------------------------------------------------------

/**
@class chat_room
room stores all messenger participants as shared pointers to them
and also stores nicknames and map from participant to it's nickname  
*/
class chat_room {
public:
  /// method adds new participant to the room
  /// @param participant is pointer to new messenger participant
    void join(std::shared_ptr<chat_participant> participant) {
      participants_.insert(participant);
    }

    /// method removes the participant from the room
    /// it frees its pointer and removes nickname
    /// @param participant is pointer to the participant to remove
    void leave(std::shared_ptr<chat_participant> participant) {
      auto it = nickname_map_.find(participant);
      auto its = nicknames_.find(it->second);
      if (its != nicknames_.end()) { nicknames_.erase(its); }
      if (it != nickname_map_.end()) { nickname_map_.erase(it); }
      participants_.erase(participant);
    }

    /// method broadcast message to all room participants
    /// @param msg is message to broadcast
    void deliver(const chat_message &msg) {
      recent_msgs_.push_back(msg);
      while (recent_msgs_.size() > max_recent_msgs) {
          recent_msgs_.pop_front();
        }

      for (auto participant: participants_) {
          participant->deliver(msg);
        }
    }

    /// method checks for nickname availability and if it is then
    /// stores new nickname and associates participant with its nickname
    /// also method sends recent messenger history to the new assigned participant
    /// @param msg is message that stores nickname
    /// @param participant is participant to associate nickname with
    bool is_available(const chat_message &msg, const std::shared_ptr<chat_participant> &participant) {
      char nick_[chat_message::max_nick_length + 1] = "";
      std::strncat(nick_, msg.nick(), msg.nick_length());
      std::string nick = std::string(nick_);
      if (nicknames_.find(nick) != nicknames_.end()) { return false; }
      nicknames_.insert(nick);
      nickname_map_.insert(std::make_pair(participant, nick));
      for (auto msg: recent_msgs_) {
          participant->deliver(msg);
        }
      return true;
    }

private:
    /// constant that defines maximum messenger history size
    static const size_t max_recent_msgs = 100;
    /// set of room participants
    std::set<std::shared_ptr<chat_participant> > participants_;
    /// set of room nicknames
    std::set<std::string> nicknames_;
    /// map from the participant to it's nickname
    std::map<std::shared_ptr<chat_participant>, std::string> nickname_map_;
    /// container of room messages
    std::deque<chat_message> recent_msgs_;
};

//----------------------------------------------------------------------

/**
@class chat_session
inherit from chat_participant, implements connecting, reading and writing messages;
also allows to creating shared_pointer from this
*/
class chat_session
    : public chat_participant,
      public std::enable_shared_from_this<chat_session> 
{
public:
    /// Constructor
    /// @param socket is tcp socket to connect
    /// @param room is room associate with this participant
    chat_session(tcp::socket socket, chat_room &room) 
    : socket_(std::move(socket)), room_(room) {}

    /// method adds participant to the room and starts reading
    void start() {
      room_.join(shared_from_this());
      do_read_header();
    }

    /// method delivers message to participant
    /// @param msg is message to deliver
    void deliver(const chat_message &msg) {
      bool write_in_progress = !write_msgs_.empty();
      write_msgs_.push_back(msg);
      if (!write_in_progress) { do_write(); }
    }

private:
    /// method starts reading from message header if something is in the socket
    /// if header is read then method starts reading nickname
    void do_read_header() {
      auto self(shared_from_this());
      boost::asio::async_read(socket_,
          boost::asio::buffer(read_msg_.data(), chat_message::header_length + chat_message::type_length),
          [this, self](boost::system::error_code ec, std::size_t /*length*/) {
              if (!ec && read_msg_.decode_header()) {
                do_read_nick();
              } else {
                room_.leave(shared_from_this());
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
    /// then it is sent to all room participants, if message type is query 
    /// then room is asked if nickname from the query is available and if it is not
    /// then message of nickname unavailability is sent
    void do_read_body() {
      auto self(shared_from_this());
      boost::asio::async_read(socket_,
          boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
          [this, self](boost::system::error_code ec, std::size_t /*length*/) {
              if (!ec) {
                msg_type type = static_cast<msg_type>(*(read_msg_.type()));
                //std::cout << "serv_msg: \"" << std::string(read_msg_.data()) << "\"" << std::endl;
                switch(type) {
                case MESSAGE:
                  room_.deliver(read_msg_);
                  break;
                case QUERY:
                  if (!room_.is_available(read_msg_, self)) {
                    deliver(create_msg("", "", NEGATIVE));
                  }
                  break;
                default:
                  break;
                }
                do_read_header();
              } else {
                room_.leave(shared_from_this());
              }
          });
    }

    /// method writes message to the socket
    void do_write() {
      auto self(shared_from_this());
      boost::asio::async_write(socket_,
          boost::asio::buffer(write_msgs_.front().data(),
            write_msgs_.front().length()),
          [this, self](boost::system::error_code ec, std::size_t /*length*/) {
              if (!ec) {
                write_msgs_.pop_front();
                if (!write_msgs_.empty()) {
                    do_write();
                  }
              } else {
                room_.leave(shared_from_this());
              }
          });
    }

    /// i/o socket of the participant
    tcp::socket socket_;
    /// room associated with participant
    chat_room &room_;
    /// local cantainer for the read message
    chat_message read_msg_;
    /// local cantainer for the send messages
    std::deque<chat_message> write_msgs_;
};

//----------------------------------------------------------------------

/**
@class chat_server
class implements mechanism of new sockets accepting
*/
class chat_server
{
public:
    /// Constructor
    /// starts accepting
    /// @param io_service is boost::asio io_service that runs in separate thread
    /// @param endpoint is server parameters such as ip version and port number
    chat_server(boost::asio::io_service &io_service, const tcp::endpoint &endpoint)
    : acceptor_(io_service, endpoint), socket_(io_service) { do_accept(); }

private:
    /// method that handles new connections accepting
    /// starts new session in the concrete room
    void do_accept() {
      acceptor_.async_accept(socket_,
          [this](boost::system::error_code ec) {
              if (!ec)  {
                std::make_shared<chat_session>(std::move(socket_), room_)->start();
              }

              do_accept();
          });
    }

    /// new connections tcp acceptor
    tcp::acceptor acceptor_;
    /// current socket
    tcp::socket socket_;
    /// server's room
    chat_room room_;
};

//----------------------------------------------------------------------

/**
@function main
starts server
@param argv is chat_server <port>
*/
int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: chat_server <port>\n";
            return 1;
        }

        boost::asio::io_service io_service;

        tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[1]));
        chat_server server = chat_server(io_service, endpoint);

        io_service.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}