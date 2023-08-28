#pragma once
#include<string>
#include<string_view>
#include"io_uring_stuff.hpp"
#include"Event.hpp"
class Protocol
{
public:
    std::string_view protocol_name;
    Protocol(std::string_view name)
        :protocol_name(name)
    {}
    virtual PRO_STATE work(request * req)=0;
};