#pragma once
#include<fmt/core.h>

#include"../Protocol.hpp"
#include"parse.hpp"
#include"io_uring_stuff.hpp"
class Http final:public Protocol// for not vptr->call()
{
private:
public:
    using Protocol::Protocol;
    virtual PRO_STATE work(request * r) override
    {
        // ParseHttpHead p(r);
        PRO_STATE ret;
        try
        {
            HTTP::Values v = HTTP::parse(r);
            // prepare body
            r->send_buffer.appendNext(4);
            r->send_buffer.next->mmapMem(v.path);
            // prepare head
            auto && head = fmt::format("HTTP/1.1 200 OK\r\nContent-type: {}\r\nContent-length: {}\r\n\r\n", v.file_type ,r->send_buffer.next->buffer_len);
            r->send_buffer.copy(head.data());
            // check conn
            ret = PRO_STATE::COMPLETE;
            if(v.heads["Connection"] == "keep-alive")
            {
                ret = PRO_STATE::CONTINUE;
            }
        }
        catch(const HTTP::ErrorExcp& e)
        {
            r->send_buffer.copy(http_404_content);
            r->send_buffer.dropTail();
            ret = PRO_STATE::COMPLETE;      
        }
        catch(const HTTP::MethodExcp& e)
        {
            r->send_buffer.copy(unimplemented_content);
            r->send_buffer.dropTail();
            ret = PRO_STATE::COMPLETE;
        }
        catch(const std::exception & e)
        {
            r->send_buffer.copy(http_404_content);
            r->send_buffer.dropTail();
            ret = PRO_STATE::COMPLETE;              
        }
        return ret;
    }
};