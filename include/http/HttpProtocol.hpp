#pragma once

#include"../Protocol.hpp"
#include"parse.hpp"
#include"io_uring_stuff.hpp"
class Http:public Protocol
{
public:
    using Protocol::Protocol;
    virtual void work(request * r) override
    {
        ParseHttpHead p(r);
    }
};