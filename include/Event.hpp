#pragma once
#include<string>
#include<string_view>
#include<unistd.h>

enum class EVENT_TYPE
{
    ACCEPT,
    READ,
    WRITE
};

class request
{
public:
    EVENT_TYPE event_type = EVENT_TYPE::ACCEPT;
    int client_fd = -1;
    std::string read_buffer;
    const void * send_buffer;
    size_t send_buffer_len;
    std::string http_head;
};