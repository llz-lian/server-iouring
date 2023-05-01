#pragma once
#include<liburing.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<memory.h>
#include<sys/mman.h>
#include<arpa/inet.h>

#include<iostream>
#include<fmt/core.h>
#include<unordered_map>
#include<string_view>
#include<string>
#include<sstream>
#include<functional>

#include"config.hpp"
#include"Event.hpp"


class IoUringClient
{
private:
    int __fd;
    void __handleWrite(request * req)
    {
        io_uring_sqe * sqe = io_uring_get_sqe(&__ring);
        io_uring_prep_write(sqe,req->client_fd,req->send_buffer,req->send_buffer_len,0);
        io_uring_sqe_set_data(sqe,req);
        io_uring_submit(&__ring);
    }
public:
    IoUringClient(std::string_view ip, int port)
    {
        sockaddr_in server_addr;
        memset(&server_addr,0,sizeof server_addr);
        server_addr.sin_family = PF_INET;
        server_addr.sin_port = ::htons(port);
        ::inet_pton(PF_INET,ip.data(),&server_addr.sin_addr.s_addr);
        __fd = ::socket(PF_INET,SOCK_STREAM,0);
        // connect
        if(connect(__fd,reinterpret_cast<const sockaddr*>(&server_addr),sizeof(socklen_t))<0)
        {
        }
    }
    void sendRequest(request * req)
    {
        __handleWrite(req);
    }
};