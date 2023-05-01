#pragma once
#include<string>
#include<functional>
#include<unordered_map>
#include<string_view>
#include<iostream>
#include<fmt/core.h>

#include"../config.hpp"
#include"../Buffer.hpp"
#include"../io_uring_stuff.hpp"

#include<sys/stat.h>
#include<stdio.h>
#include<fcntl.h>
#include<fmt/core.h>
#include<sys/mman.h>

class HandleHtppGetArgs
{
private:
    using handle = std::function<std::string_view(std::string_view)>;
    std::unordered_map<std::string_view,handle> handles;

public:
    void appendHandle(std::string_view path, handle && handle)
    {
        handles.insert({path,std::move(handle)});
    }
    std::string_view getResult(std::string_view path,std::string_view args)
    {
        if(handles.find(path)==handles.end())
            throw "no args.";
        return handles[path](args);
    }
};
inline HandleHtppGetArgs http_get_handles;

class ParseHttpHead
{
    std::string_view getFileType(std::string_view path)
    {
        // find first .
        size_t index = path.size()-1;
        for(;index>=0;index--)
        {
            if(path[index] == '.')break;
        }
        index++;
        auto type = path.substr(index);
        std::string_view ret_type;
        if(type == "png"||type == "PNG")
        {
            ret_type = "image/png";
        }
        else if(type == "jpg"||type == "JPG"||type == "jpeg"||type == "jpeg")
        {
            ret_type = "image/jpeg";
        }
        else if(type == "gif"||type == "gif")
        {
            ret_type = "image/gif";
        }
        else if(type == "htm"||type == "html")
        {
            ret_type = "text/html";
        }
        else if(type == "js")
        {
            ret_type = "application/javascript";
        }
        else if(type == "css")
        {
            ret_type = "text/css";
        }
        else if(type == "txt")
        {
            ret_type = "text/plain";
        }
        return ret_type;
    }
    size_t toNextBlank(std::string_view str,size_t index, char target = ' ')
    {
        while(index<str.size()&&str[index]!=target) index++;
        if(index >= str.size()) 
            throw "parse error.";
        return index;
    }
    size_t toNextItem(std::string_view str, size_t index)
    {
        while(index<str.size()&& str[index]!='\r')
        {
            index++;
        }
        // s[index] == '\r'
        index++;
        // s[index]=='\n'
        if(index >= str.size()||str[index]!='\n') 
            throw "parse error.";
        index++;
        return index;
    }
    size_t parseHttpMehod(std::string_view full_head,size_t start_index)
    {
        // find first ' '
        auto index = toNextBlank(full_head,start_index);
        std::string_view method = full_head.substr(start_index,index-start_index);

        if(!(method == "Get"||method == "GET"||method == "get")) 
            throw "method is not implemented.";
        index++;
        return index;
    }
    std::pair<size_t,std::string_view> parseHttpUrl(std::string_view full_head,size_t start_index,request * req)
    {
        // find first ' '
        auto index = toNextBlank(full_head,start_index);
        std::string_view url = full_head.substr(start_index,index-start_index);
        std::string path;
        if(url == "/")
        {
            path = "public/index.html";
        }
        else
        {
            path = fmt::format("public{}",url);
            try
            {
                auto arg_start = toNextBlank(url,0,'?');// if not has '?' throw an error
                auto args = url.substr(arg_start+1);
                url = url.substr(1,arg_start);
                path = http_get_handles.getResult(url,args);// throw error
            }
            catch(char const * e)
            {

            }
        }
        //check?a=...&b=...&c=...
        // find first '?'
        // check path is exists
        req->send_buffer->appendNext(0);
        req->send_buffer->next->mmapMem(path);
        index++;
        return {index,getFileType(path)};
    }
    size_t parseHttpVersion(std::string_view full_head,size_t start_index)
    {
        // find \r \n
        auto index = toNextItem(full_head,start_index);
        std::string_view version = full_head.substr(start_index,index-start_index);
        // nop \r\n to item
        index = toNextItem(full_head,index);
        return index;
    }
    size_t parseHttpKeyValue(std::string_view full_head,size_t start_index)
    {
        // find \r\n
        auto index = toNextItem(full_head,start_index);
        std::string_view kv = full_head.substr(start_index,index-start_index);
        // split by ':'
        size_t split_index = 0;
        for(;split_index<kv.size();split_index++)
        {
            if(kv[split_index]==':') break;
        }

        std::string_view key = kv.substr(0,split_index);
        std::string_view value = kv.substr(split_index+1,kv.size()-split_index);

        return index;
    }
public:
    ParseHttpHead(request * req)
    {
        std::string_view full_head = static_cast<char *>(req->recv_buffer->mem);
        try
        {
            size_t start_index = 0;
            start_index = parseHttpMehod(full_head,start_index);
            auto [index,type] = parseHttpUrl(full_head,start_index,req);
            start_index = index;
            start_index = parseHttpVersion(full_head,start_index);
            // prepare head
            auto && head = fmt::format("HTTP/1.0 200 OK\r\nContent-type: {}\r\nContent-length: {}\r\n\r\n",type ,req->send_buffer->next->buffer_len);
            req->send_buffer->copy(head.data());
        }
        catch(char const* e)
        {
            // 404
            if(::strcmp(e,"method is not implemented.")!=0)
            {
                req->send_buffer->copy(http_404_content);
                req->send_buffer->dropTail();
            }
            else
            {
                req->send_buffer->copy(unimplemented_content);
                req->send_buffer->dropTail();
            }
        }
    }
};