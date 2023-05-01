#pragma once

#include<liburing.h>
#include"Buffer.hpp"
#include"Event.hpp"

inline io_uring __ring;

class request
{
public:
    EVENT_TYPE event_type = EVENT_TYPE::ACCEPT;
    int client_fd = -1;
    Buffer * recv_buffer = new Buffer(4096);
    Buffer * send_buffer = new Buffer(4096);
    ~request()
    {
        delete recv_buffer;
        delete send_buffer;
    }
};

inline void appRead(request * req)
{
    io_uring_sqe * sqe = io_uring_get_sqe(&__ring);
    req->event_type = EVENT_TYPE::READ;
    io_uring_prep_read(sqe,req->client_fd,req->recv_buffer->mem,req->recv_buffer->buffer_len,0);
    io_uring_sqe_set_data(sqe,req);
    io_uring_submit(&__ring);
}
inline void appRecv(request * req)
{
    io_uring_sqe * sqe = io_uring_get_sqe(&__ring);
    req->event_type = EVENT_TYPE::READ;
    io_uring_prep_recv(sqe,req->client_fd,req->recv_buffer->mem,req->recv_buffer->buffer_len,0);
    io_uring_sqe_set_data(sqe,req);
    io_uring_submit(&__ring);   
}

inline void appWrite(request * req)
{
    auto buffer_ptr = req->send_buffer;
    req->event_type = EVENT_TYPE::WRITE;
    io_uring_sqe * sqe;
    while(buffer_ptr!=nullptr)
    {
        sqe = io_uring_get_sqe(&__ring);
        auto flag = buffer_ptr->next==nullptr?0u:IOSQE_IO_LINK;
        io_uring_sqe_set_flags(sqe,flag);
        io_uring_prep_write(sqe,req->client_fd,buffer_ptr->mem,buffer_ptr->buffer_len,0);
        buffer_ptr = buffer_ptr->next;
    }
    io_uring_sqe_set_data(sqe,req);
    io_uring_submit(&__ring);
}
inline void appSend(request * req)
{
    auto buffer_ptr = req->send_buffer;
    req->event_type = EVENT_TYPE::WRITE;
    io_uring_sqe * sqe;
    while(buffer_ptr!=nullptr)
    {
        sqe = io_uring_get_sqe(&__ring);
        io_uring_sqe_set_flags(sqe,IOSQE_IO_LINK);
        io_uring_prep_send(sqe,req->client_fd,buffer_ptr->mem,buffer_ptr->buffer_len,0);
    }
    io_uring_sqe_set_data(sqe,req);
    io_uring_submit(&__ring);
}
inline void appAccept(sockaddr_in * client_addr,socklen_t * client_addr_len,int listen_fd)
{
    io_uring_sqe * sqe = io_uring_get_sqe(&__ring);
    io_uring_prep_accept(sqe, listen_fd, reinterpret_cast<sockaddr *>(client_addr), client_addr_len, 0);
    request * req = new request;
    io_uring_sqe_set_data(sqe,req);
    io_uring_submit(&__ring);
}