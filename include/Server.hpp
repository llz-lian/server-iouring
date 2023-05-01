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
#include"http/parse.hpp"
#include"redis_cli/redis_client.hpp"

inline io_uring __ring;
class IoUringServer
{
private:
    int __listen_fd;
    std::unordered_map<int,request*> __requests;
    void __acceptFd(sockaddr_in * client_addr,socklen_t * client_addr_len)
    {
        io_uring_sqe * sqe = io_uring_get_sqe(&__ring);
        io_uring_prep_accept(sqe,__listen_fd,reinterpret_cast<sockaddr *>(client_addr),client_addr_len,0);
        request * req = new request;
        io_uring_sqe_set_data(sqe,req);
        io_uring_submit(&__ring);
    }
    void __Handleread(int client_fd)
    {
        io_uring_sqe * sqe = io_uring_get_sqe(&__ring);
        request * req = new request;
        __requests[client_fd] = req;
        req->client_fd = client_fd;

        req->event_type = EVENT_TYPE::READ;
        req->read_buffer.resize(READ_SZ);
        io_uring_prep_read(sqe,client_fd,req->read_buffer.data(),req->read_buffer.size(),0);
        io_uring_sqe_set_data(sqe,req);
        io_uring_submit(&__ring);
    }
    void __handleProcess(request * req)
    {
        // echo back data
        // req->send_buffer = req->read_buffer;
        // or..
        // parse http head set req->buffer
        ParseHttpHead(req->read_buffer,req);
        // std::cout<<"send."<<std::endl;
    }
    void __handleWrite(request * req)
    {
        io_uring_sqe * sqe = io_uring_get_sqe(&__ring);
        req->event_type = EVENT_TYPE::WRITE;

        // send header
        sqe->flags |= IOSQE_IO_LINK;
        io_uring_prep_write(sqe,req->client_fd,req->http_head.data(),req->http_head.size(),0);
        io_uring_sqe_set_data(sqe,req);

        // send body
        sqe = io_uring_get_sqe(&__ring);
        io_uring_prep_write(sqe,req->client_fd,req->send_buffer,req->send_buffer_len,0);
        io_uring_sqe_set_data(sqe,req);

        io_uring_submit(&__ring);
    }
public:
    IoUringServer(int port)
    {
        // init ring
        io_uring_queue_init(QUEUE_DEPTH,&__ring, 0);
        // set listen_fd
        __listen_fd = ::socket(PF_INET,SOCK_STREAM,0); 
        if(__listen_fd<0) throw std::runtime_error(fmt::format("socket error."));
        
        if(int enable = 1;
        ::setsockopt(__listen_fd,SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&enable,sizeof(int))<0)
        {
            throw std::runtime_error(fmt::format("setsockopt error."));
        }

        sockaddr_in server_addr;
        memset(&server_addr,0,sizeof server_addr);
        server_addr.sin_family = PF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if(bind(__listen_fd,reinterpret_cast<const sockaddr *>(&server_addr),sizeof(server_addr))<0)
        {
            std::cout<<strerror(errno)<<std::endl;
            throw std::runtime_error(fmt::format("bind error."));
        }

        if(listen(__listen_fd,10)<0) throw std::runtime_error(fmt::format("listen error."));
        __requests.reserve(10000);
    }
    void run()
    {
        io_uring_cqe * cqe;
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(sockaddr_in);
        // loop
        __acceptFd(&client_addr,&client_addr_len);
        while(true)
        {
            int ret = io_uring_wait_cqe(&__ring,&cqe);
            if(ret<0) 
            {
                io_uring_cqe_seen(&__ring,cqe);
                continue;
            }
            request * req = reinterpret_cast<request*>(cqe->user_data);
            if(cqe->res<0||req == nullptr)
            {
                io_uring_cqe_seen(&__ring,cqe);
                continue;
            }
            switch(req->event_type)
            {
                case EVENT_TYPE::ACCEPT:
                // accept complete
                    __acceptFd(&client_addr,&client_addr_len);// append accept
                    __Handleread(cqe->res);
                    break;
                case EVENT_TYPE::READ:
                // read complete
                    __handleProcess(req);
                    __handleWrite(req);
                    break;
                case EVENT_TYPE::WRITE:
                // write complete

                    int client_fd = req->client_fd;
                    close(client_fd);
                    auto munmap_ret = ::munmap(const_cast<void *>(req->send_buffer),req->send_buffer_len);
                    if(munmap_ret<0)
                    {
                        std::cerr<<fmt::format("munmap error: {}.",::strerror(errno))<<std::endl;
                    }
                    // __requests[client_fd] = nullptr;
                    __requests.erase(client_fd);
                    delete req;
            }
            io_uring_cqe_seen(&__ring,cqe);
        }
    }
};


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