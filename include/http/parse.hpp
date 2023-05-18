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

enum class METHOD:int{GET,POST,NONE};





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
    
    std::pair<size_t,METHOD> parseHttpMehod(std::string_view full_head,size_t start_index)
    {
        // find first ' '
        auto index = toNextBlank(full_head,start_index);
        std::string_view method = full_head.substr(start_index,index-start_index);
        METHOD ret_method = METHOD::NONE;
        if(method == "Get"||method == "GET"||method == "get")
        {
            ret_method = METHOD::GET;
        }
        else if (method == "POST"||method == "Post"||method == "post")
        {
            ret_method = METHOD::POST;
        }
        else
        {
            throw "method is not implemented.";
        }
        index++;
        return std::pair<size_t,METHOD>{index,ret_method};
    }

    std::unordered_map<std::string,std::string> getAndArgs(std::string_view arg_line)
    {
        // split by &
        std::unordered_map<std::string,std::string> ret;
        // reach = get key
        // reach & or tail get value
        size_t left = 0;
        std::string_view key;
        for(size_t right = 0;right<=arg_line.size();right++)
        {
            if(arg_line[right] == '=')
            {
                key = arg_line.substr(left,right-left);
                left = right+1;
            }
            else if(right == arg_line.size()||arg_line[right] == '&')
            {
                ret[std::string(key)] = arg_line.substr(left,right-left);
                left = right+1;
            }
        }
        return ret;
    }
    std::string getPath(std::string_view url)
    {
        if(url == "/")
        {
            return "public/index.html";
        }
        // find ?
        std::string_view arg_line;
        for(int i = url.size()-1;i>=0;i--)
        {
            if(url[i] == '?')
            {
                arg_line = url.substr(i+1,url.size()-i-1);
                break;
            }
        }
        if(arg_line.empty())
        {
            return fmt::format("public{}",url);
        }
        auto args = getAndArgs(arg_line);
        bool islogin = login(args["user"],args["password"]);
        if(islogin)
        {
            return "afterlogin/login_succ.html";
        }
        return "afterlogin/login_fail.html";
    }

    std::pair<size_t,std::string_view> parseHttpUrl_GET(std::string_view full_head,size_t start_index,request * req)
    {
        // find first ' '
        auto index = toNextBlank(full_head,start_index);
        std::string_view url = full_head.substr(start_index,index-start_index);// /a?a=x&b=x
        std::string path = getPath(url);
        // check path is exists
        req->send_buffer.appendNext(4);
        req->send_buffer.next->mmapMem(path);
        index++;
        return {index,getFileType(path)};
    }
    size_t parseHttpVersion(std::string_view full_head,size_t start_index)
    {
        // find \r \n
        auto index = toNextItem(full_head,start_index);
        // std::string_view version = full_head.substr(start_index,index-start_index);
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

        // std::string_view key = kv.substr(0,split_index);
        // std::string_view value = kv.substr(split_index+1,kv.size()-split_index);

        return index;
    }
    void parseGet(std::string_view full_head,size_t start_index,request * req)
    {
        auto [index,type] = parseHttpUrl_GET(full_head,start_index,req);
        start_index = index;
        start_index = parseHttpVersion(full_head,start_index);
        // prepare head
        auto && head = fmt::format("HTTP/1.0 200 OK\r\nContent-type: {}\r\nContent-length: {}\r\n\r\n",type ,req->send_buffer.next->buffer_len);
        req->send_buffer.copy(head.data());
    }
    void parsePost()
    {
        throw "method is not implemented.";
    }
public:
    ParseHttpHead(request * req)
    {
        std::string_view full_head = static_cast<char *>(req->recv_buffer.mem);
        try
        {
            size_t start_index = 0;
            auto ret = parseHttpMehod(full_head,start_index);
            start_index = ret.first;
            auto method = ret.second;
            switch (method)
            {
                case METHOD::GET:parseGet(full_head,start_index,req);
                    break;
                case METHOD::POST:parsePost();
                    break;
                default:
                    throw "method is not implemented.";
            }
        }
        catch(char const* e)
        {
            // 404
            if(::strcmp(e,"method is not implemented.")!=0)
            {
                req->send_buffer.copy(http_404_content);
                req->send_buffer.dropTail();
            }
            else
            {
                req->send_buffer.copy(unimplemented_content);
                req->send_buffer.dropTail();
            }
        }
    }
};