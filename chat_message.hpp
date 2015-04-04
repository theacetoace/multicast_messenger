#pragma once
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>

enum msg_type {
    QUERY = 'q',
    MESSAGE = 'm',
    NEGATIVE = 'n'
};

class chat_message
{
public:
    static const int header_length = 6;
    static const int type_length = 1;
    static const int max_body_length = 1024;
    static const int max_nick_length = 16;

    chat_message() : body_length_(0), nick_length_(0) {}

    const char* data() const {
        return data_;
    }

    char* data() {
        return data_;
    }

    size_t length() const {
        return header_length + type_length + max_nick_length + body_length_;
    }

    const char* body() const {
        return data_ + header_length + type_length + max_nick_length;
    }

    char* body() {
        return data_ + header_length + type_length + max_nick_length;
    }

    const char* nick() const {
        return data_ + header_length + type_length;
    }

    char* nick() {
        return data_ + header_length + type_length;
    }

    const char *type() const {
      return data_ + header_length;
    }

    char *type() {
      return data_ + header_length;
    }

    size_t body_length() const {
        return body_length_;
    }

    size_t nick_length() const {
        return nick_length_;
    }

    void body_length(size_t new_length) {
        body_length_ = new_length;
        if (body_length_ > max_body_length) {
            body_length_ = max_body_length;
        }
    }

    void nick_length(size_t new_length) {
        nick_length_ = new_length;
        if (nick_length_ > max_nick_length) {
            nick_length_ = max_nick_length;
        }
    }

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

    void encode_header() {
        char header[header_length + 1] = "";
        std::sprintf(header, "%04d%02d", body_length_, nick_length_);
        std::memcpy(data_, header, header_length);
    }

private:
    char data_[header_length + max_body_length + max_nick_length + type_length];
    size_t body_length_, nick_length_;
};

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