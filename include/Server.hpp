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
#include<unordered_set>
#include<string_view>
#include<string>
#include<sstream>
#include<functional>

#include"config.hpp"
#include"Event.hpp"
#include"Protocol.hpp"
#include"io_uring_stuff.hpp"
#include"Task.hpp"

class IoUringServer
{
private:
    int __listen_fd;
    Protocol * todo;
    std::unordered_set<request*> __requests;
    void __Handleread(request * req)
    {
        // __requests.insert(req);
        // req->client_fd = client_fd;
        // non-block?

        setNonBlock(req->client_fd);
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
        __requests.erase(req);
        delete req;
    }

    std::unordered_map<request *, Task> handles;
    Task coroWork()
    {
        request *req = new request;
        Accept acpt(__listen_fd,req);
        Read rd(req);
        Write wt(req);

        int client_fd = co_await acpt;// handle need new work
        req->client_fd = client_fd;
        // std::cout<<fmt::format("accept fd: {}",client_fd)<<std::endl;
        // int max_service_time = 2;
        while(true)
        {
            int read_num = co_await rd;
            // std::cout<<fmt::format("read data: {} bytes.",read_num)<<std::endl;
            // read 0 need close ref: man recv
            if(read_num == 0) break;
            todo->work(req);
            int write_num = co_await wt;
            // std::cout<<fmt::format("write data: {} bytes.",write_num)<<std::endl;
        }
        // std::cout<<fmt::format("close fd: {}",client_fd)<<std::endl;
        close(client_fd);
        delete req;
        // done
    }

public:
    IoUringServer(int port,Protocol * p)
        :todo(p)
    {
        // init ring
        io_uring_queue_init(QUEUE_DEPTH,&__ring, IORING_SETUP_SQPOLL);
        unsigned int max_workers []= {2,2};
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
        handles.reserve(10000);
    }
    
    void coroRun()
    {
        io_uring_cqe * cqe;
        auto h = coroWork();
        handles[h.getRequest()] = h;
        while(true)
        {
            int ret = io_uring_wait_cqe(&__ring,&cqe);
            if(ret<0) 
            {
                io_uring_cqe_seen(&__ring,cqe);
                continue;
            }
            request * req = reinterpret_cast<request*>(cqe->user_data);
            if(cqe->res<0||req == nullptr||handles.find(req)==handles.end())
            {
                io_uring_cqe_seen(&__ring,cqe);
                continue;
            }
            if(req->event_type==EVENT_TYPE::ACCEPT)
            {
                h = coroWork();
                handles[h.getRequest()] = h;
            }
            //need get handle by req
            // and resume()
            auto task = handles.at(req);
            if(!task.done()) 
                task.resume(cqe);
            else
            {
                task.destroy();
                handles.erase(req);
            }
            io_uring_cqe_seen(&__ring,cqe);
        }
    }
    
    void run()
    {
        io_uring_cqe * cqe;
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(sockaddr_in);
        // loop
        auto new_request = new request;
        appAccept(&client_addr,&client_addr_len,__listen_fd,new_request);
        __requests.insert(new_request);
        while(true)
        {
            int ret = io_uring_wait_cqe(&__ring,&cqe);
            if(ret<0) 
            {
                io_uring_cqe_seen(&__ring,cqe);
                continue;
            }
            request * req = reinterpret_cast<request*>(cqe->user_data);
            if(cqe->res<0||req == nullptr||__requests.find(req)==__requests.end())
            {
                io_uring_cqe_seen(&__ring,cqe);
                continue;
            }
            switch(req->event_type)
            {
                case EVENT_TYPE::ACCEPT:
                    new_request = new request;
                    appAccept(&client_addr,&client_addr_len,__listen_fd,new_request);// append accept
                    __requests.insert(new_request);
                    req->client_fd = cqe->res;
                    __Handleread(req);
                    break;
                case EVENT_TYPE::READ:
                // read complete
                    __handleProcess(req);
                    __handleWrite(req);
                    break;
                case EVENT_TYPE::WRITE:
                // write complete
                    __afterWrite(req);
            /*
                1. accept listen_fd get client_fd
                2. read client_fd
                3. process
                4. wirte client_fd
                5. continue read?goto read or return
                Task Work()
                {
                    http_request = new http_request;
                    int client_fd = co_awiat Accept(listen_fd);// handle need new work
                    // client_fd = await_resume(); cqe->res;
                    while(true)
                    {
                        int read_num = co_awiat Read(client_fd,read_buffer);
                        int keep_read = process(http_request);
                        co_await Write(client_fd, write_buffer);
                        if(!keep_read) break;
                    }
                    delete request;
                    close()
                }
                void IoUringServer::run()
                {
                    Work();
                    while(true)
                    {
                        int ret = io_uring_wait_cqe(&__ring,&cqe);
                        //get handle from cqe
                        h = cqe->user_data;
                        if(h.need_new_work) Work();
                        h.resume(cqe);// task::resume(cqe){ h.resume();}
                    }
                }

            */
            }
            io_uring_cqe_seen(&__ring,cqe);
        }
    }
};
