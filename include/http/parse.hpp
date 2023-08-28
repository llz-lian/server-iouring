#pragma once
#include<string>
#include<functional>
#include<unordered_map>
#include<string_view>
#include<iostream>
#include<fmt/core.h>
#include <optional>
#include<map>

#include"../config.hpp"
#include"../Buffer.hpp"
#include"../io_uring_stuff.hpp"
#include"./Handler.hpp"


#include<sys/stat.h>
#include<stdio.h>
#include<fcntl.h>
#include<fmt/core.h>
#include<sys/mman.h>

enum class METHOD:int{GET,POST,NONE};



namespace HTTP
{

    struct Values
    {
        std::string http_ver;
        METHOD method;
        std::string url;// full url
        std::string path;// file path
        std::unordered_map<std::string_view,std::string_view> params;// this url's view
        std::map<std::string,std::string> heads;// do not use view of req->read
        std::string_view file_type;
    };
    class ErrorExcp:public std::exception{};
    class MethodExcp:public std::exception{};
    namespace
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
                throw ErrorExcp{};
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
                throw ErrorExcp{};
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
                throw MethodExcp{};
            }
            index++;
            return std::pair<size_t,METHOD>{index,ret_method};
        }
        std::pair<size_t,std::string_view> parseHttpUrl(std::string_view full_head,size_t start_index)
        {
            // find first ' '
            auto index = toNextBlank(full_head,start_index);
            std::string_view url = full_head.substr(start_index,index-start_index);// /a?a=x&b=x
            index++;
            return {index,url};
        }
        std::pair<size_t,std::string_view> parseHttpVersion(std::string_view full_head,size_t start_index)
        {
            // find \r \n
            auto index = toNextItem(full_head,start_index);
            std::string_view version = full_head.substr(start_index,index-start_index-2);
            // nop \r\n to item
            index = toNextItem(full_head,index);
            return {index,version};
        }
        void getParamByAnd(std::string_view full_head, size_t start_index, Values & v)
        {
            // a = xx & b = xx & c = xx & d....
            size_t left = 0;
            std::string_view key;
            auto arg_line = full_head.substr(start_index);
            for(size_t right = 0;right<=arg_line.size();right++)
            {
                if(arg_line[right] == '=')
                {
                    key = arg_line.substr(left,right-left);
                    left = right+1;
                }
                else if(right == arg_line.size()||arg_line[right] == '&')
                {
                    if(!key.empty())
                        v.params[key] = arg_line.substr(left,right-left);
                    left = right+1;
                }
            }
        }
        size_t parseHttpHead(std::string_view full_head,size_t start_index, Values & v)
        {
            // we need Connection: keep-alive,close 
            // Content-Length: xxx
            // Transfer-Encoding: chunked?
            // find C and T
            std::string_view connection = "Connection";
            auto conn_idx = full_head.find(connection,start_index);
            if(conn_idx)
            {
                // parse coonection
                // full_head[conn_idx] == "C"
                conn_idx += connection.size();
                while(!isalpha(full_head[conn_idx]))
                {
                    conn_idx++;
                }
                auto tail_index = toNextItem(full_head,conn_idx) - 3;// nop \r\n
                v.heads["Connection"] = full_head.substr(conn_idx,tail_index - conn_idx + 1);
            }
            // reach tail ignore other heads
            auto index = full_head.find("\r\n\r\n",conn_idx);
            if(index == std::string::npos)
            {
                throw ErrorExcp{};
            }
            return index + 4;
        }
        void getUrlParams(Values & v)
        {
            std::string_view url = v.url;
            // find first ?
            auto index = url.find('?');
            if(index == std::string::npos) return;
            index++;// nop ?
            getParamByAnd(url,index,v);
        }
    }
    void getPathByParam(Values & v)
    {
        auto index = v.url.find('?');
        std::string_view raw_path = v.url;
        if(index != std::string::npos)
        {
            raw_path = v.url.substr(0,index);
        }
        if(v.params.empty())
        {
            if(v.url == "/")
            {
                v.path = "public/index.html";
            }
            else
            {
                v.path = "public/";
                v.path += raw_path;
            }
            return;
        }
        // check
        if(raw_path == "/login.html")
        {
            v.path = Handler::htmlHandle<Handler::Login>( v.params.at("user"),v.params.at("password"));
        }
        else if(raw_path == "/sign_up.html")
        {
            v.path = Handler::htmlHandle<Handler::regist>(v.params.at("user"),v.params.at("password"));
        }
    }
    // return a Values struct
    // parse
    Values parse(request * req)// has exception
    {
        Values v;
        size_t start_index = 0;
        std::string_view full_head = static_cast<char *>(req->recv_buffer.mem);// char
        // first line
        auto [next_index,method] = parseHttpMehod(full_head,start_index);
        auto [next,url] = parseHttpUrl(full_head,next_index);
        auto [n,version] = parseHttpVersion(full_head,next);
        //parse req head
        n = parseHttpHead(full_head,n,v);
        // parse body
        getParamByAnd(full_head,n,v);
        v.method = method;
        v.url = url;
        // parse over
        // get url param
        getUrlParams(v);
        // check url param
        getPathByParam(v);
        // check file type
        v.file_type = getFileType(v.path);
        // over
        return v;
    }
}

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
        std::string_view file_name;
        for(int i = url.size()-1;i>=0;i--)
        {
            if(url[i] == '?')
            {
                arg_line = url.substr(i+1,url.size()-i-1);
                file_name = url.substr(0,i);
                break;
            }
        }
        if(arg_line.empty())
        {
            return fmt::format("public/{}",url);
        }
        auto args = getAndArgs(arg_line);
        if(file_name == "/login.html")
        {
            return Handler::htmlHandle<Handler::Login>( args.at("user"),args.at("password"));
        }
        else if(file_name == "/sign_up.html")
        {
            return Handler::htmlHandle<Handler::regist>(args.at("user"),args.at("password"));
        }
        return "public/index.html";
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