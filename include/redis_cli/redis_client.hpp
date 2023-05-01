#pragma once
#include<liburing.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<memory.h>
#include"../config.hpp"

#include<fmt/core.h>
#include<string_view>
class RedisClient
{
private:
    int client_fd = -1; 
public:
    std::string_view setKV(std::string_view k,std::string_view v)
    {
        return fmt::format("\r\nset {} {}\r\n",k,v);
    }

    std::string_view incrK(std::string_view k)
    {
        return fmt::format("\r\nincr {}\r\n",k);
    }
};