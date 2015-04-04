#pragma once
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/**
@file chat_message.hpp
message interface
*/

/// message type
enum msg_type {
    QUERY = 'q',
    MESSAGE = 'm',
    NEGATIVE = 'n'
};

/**
@class chat_message
message interface
*/
class chat_message
{
public:
    /// header length constant
    static const int header_length = 6;
    /// type length constant
    static const int type_length = 1;
    /// maximum body length constant
    static const int max_body_length = 1024;
    /// maximum nickname length constant
    static const int max_nick_length = 16;

    /// Constructor
    chat_message() : body_length_(0), nick_length_(0) {}

    /// getter of constant pointer to the message data 
    const char* data() const {
        return data_;
    }

    /// getter of mutable pointer to the message data 
    char* data() {
        return data_;
    }

    /// getter of the whole message length 
    size_t length() const {
        return header_length + type_length + max_nick_length + body_length_;
    }

    /// getter of constant pointer to the body part
    const char* body() const {
        return data_ + header_length + type_length + max_nick_length;
    }

    /// getter of mutable pointer to the body part
    char* body() {
        return data_ + header_length + type_length + max_nick_length;
    }

    /// getter of constant pointer to the nickname part
    const char* nick() const {
        return data_ + header_length + type_length;
    }

    /// getter of mutable pointer to the nickname part
    char* nick() {
        return data_ + header_length + type_length;
    }

    /// getter of constant pointer to the type part
    const char *type() const {
      return data_ + header_length;
    }

    /// getter of mutable pointer to the type part
    char *type() {
      return data_ + header_length;
    }

    /// getter of body part length
    size_t body_length() const {
        return body_length_;
    }

    /// getter of nickname part length
    size_t nick_length() const {
        return nick_length_;
    }

    /// setter of body part length
    /// @param new_length is new body part length
    void body_length(size_t new_length) {
        body_length_ = new_length;
        if (body_length_ > max_body_length) {
            body_length_ = max_body_length;
        }
    }

    /// setter of nickname part length
    /// @param new_length is new nickname part length
    void nick_length(size_t new_length) {
        nick_length_ = new_length;
        if (nick_length_ > max_nick_length) {
            nick_length_ = max_nick_length;
        }
    }

    /// method decodes message body and nickname parts length
    /// returns if ok true, else false
    bool decode_header() {
        char header[header_length + 1] = "";
        std::strncat(header, data_, header_length);
        body_length_ = std::atoi(header);
        nick_length_ = body_length_ % 100;
        body_length_ = body_length_ / 100;
        if (body_length_ > max_body_length) {
            body_length_ = 0;
            return false;
        }
        if (nick_length_ > max_nick_length) {
            body_length_ = 0;
            return false;
        }
        return true;
    }

    /// method encodes message body and nickname parts length
    void encode_header() {
        char header[header_length + 1] = "";
        std::sprintf(header, "%04d%02d", body_length_, nick_length_);
        std::memcpy(data_, header, header_length);
    }

private:
    /// message storage
    char data_[header_length + max_body_length + max_nick_length + type_length];
    /// current body part length
    size_t body_length_;
    /// current nickname part length
    size_t nick_length_;
};

/**
@function create_msg
creates new message to deliver from the message, the nickname and message type
@param line is c string that stores actual message
@param nick is c string that stores nickname
@param type is message type
*/
chat_message create_msg(const char *line, const char *nick, msg_type type) {
    chat_message msg;
    msg.body_length(std::strlen(line));
    msg.nick_length(std::strlen(nick));
    *(msg.type()) = static_cast<char>(type);
    std::memcpy(msg.body(), line, msg.body_length());
    std::memcpy(msg.nick(), nick, msg.nick_length());
    msg.encode_header();
    return msg;
}