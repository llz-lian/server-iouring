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
#include"Protocol.hpp"
#include"io_uring_stuff.hpp"

class IoUringServer
{
private:
    int __listen_fd;
    Protocol * todo;
    std::unordered_map<int,request*> __requests;
    void __Handleread(int client_fd)
    {
        request * req = new request;
        __requests[client_fd] = req;
        req->client_fd = client_fd;
        // non-block?
        // int flags = fcntl(client_fd, F_GETFL, 0);
        // fcntl(client_fd, F_SETFL, flags|O_NONBLOCK);

        appRead(req);
    }
    void __handleProcess(request * req)
    {
        // echo back data
        // req->send_buffer = req->read_buffer;
        // or..
        // parse http head set req->buffer
        // ParseHttpHead(req->read_buffer,req);
        todo->work(req);
        // std::cout<<"send."<<std::endl;
    }
    void __handleWrite(request * req)
    {
        appWrite(req);
    }

    void __afterWrite(request * req)
    {
        int client_fd = req->client_fd;
        close(client_fd);
        __requests.erase(client_fd);
        delete req;
    }

public:
    IoUringServer(int port,Protocol * p)
        :todo(p)
    {
        // init ring
        io_uring_queue_init(QUEUE_DEPTH,&__ring, IORING_SETUP_SQPOLL);
        unsigned int max_workers []= {1,1};
        auto worker_ret = io_uring_register_iowq_max_workers(&__ring,max_workers);
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
        appAccept(&client_addr,&client_addr_len,__listen_fd);
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
                    appAccept(&client_addr,&client_addr_len,__listen_fd);// append accept
                    __Handleread(cqe->res);
                    break;
                case EVENT_TYPE::READ:
                // read complete
                    __handleProcess(req);
                    __handleWrite(req);
                    break;
                case EVENT_TYPE::WRITE:
                // write complete
                    __afterWrite(req);
            }
            io_uring_cqe_seen(&__ring,cqe);
        }
    }
};
