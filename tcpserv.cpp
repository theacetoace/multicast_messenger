#include <iostream>
#include <deque>
#include <memory>
#include <set>
#include <map>
#include <utility>
#include <boost/asio.hpp>
#include "chat_message.hpp"

using boost::asio::ip::tcp;

class chat_participant {
public:
  	virtual ~chat_participant() {}
  	virtual void deliver(const chat_message& msg) = 0;
};

class chat_room {
public:
  	void join(std::shared_ptr<chat_participant> participant) {
    	participants_.insert(participant);
  	}

  	void leave(std::shared_ptr<chat_participant> participant) {
  		auto it = nickname_map_.find(participant);
  		auto its = nicknames_.find(it->second);
  		if (its != nicknames_.end()) { nicknames_.erase(its); }
  		if (it != nickname_map_.end()) { nickname_map_.erase(it); }
    	participants_.erase(participant);
  	}

  	void deliver(const chat_message &msg) {
    	recent_msgs_.push_back(msg);
    	while (recent_msgs_.size() > max_recent_msgs) {
      		recent_msgs_.pop_front();
      	}

    	for (auto participant: participants_) {
      		participant->deliver(msg);
      	}
  	}

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
  	std::set<std::shared_ptr<chat_participant> > participants_;
  	std::set<std::string> nicknames_;
  	std::map<std::shared_ptr<chat_participant>, std::string> nickname_map_;
  	static const size_t max_recent_msgs = 100;
  	std::deque<chat_message> recent_msgs_;
};

//----------------------------------------------------------------------

class chat_session
  	: public chat_participant,
      public std::enable_shared_from_this<chat_session> 
{
public:
  	chat_session(tcp::socket socket, chat_room &room) 
  	: socket_(std::move(socket)), room_(room) {}

  	void start() {
    	room_.join(shared_from_this());
    	do_read_header();
  	}

  	void deliver(const chat_message &msg) {
    	bool write_in_progress = !write_msgs_.empty();
    	write_msgs_.push_back(msg);
    	if (!write_in_progress) { do_write(); }
  	}

private:
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

  	tcp::socket socket_;
  	chat_room &room_;
  	chat_message read_msg_;
  	std::deque<chat_message> write_msgs_;
};

//----------------------------------------------------------------------

class chat_server
{
public:
  	chat_server(boost::asio::io_service &io_service, const tcp::endpoint &endpoint)
    : acceptor_(io_service, endpoint), socket_(io_service) { do_accept(); }

private:
  	void do_accept() {
    	acceptor_.async_accept(socket_,
        	[this](boost::system::error_code ec) {
          		if (!ec)  {
            		std::make_shared<chat_session>(std::move(socket_), room_)->start();
          		}

          		do_accept();
        	});
  	}

  	tcp::acceptor acceptor_;
  	tcp::socket socket_;
  	chat_room room_;
};

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
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