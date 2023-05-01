#pragma once
#include<string>
#include<functional>
#include<unordered_map>
#include<string_view>
#include<iostream>
#include"../config.hpp"
#include"../Event.hpp"

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
        }
        //check?a=...&b=...&c=...
        // find first '?'
        try
        {
            auto arg_start = toNextBlank(url,0,'?');// if not has '?' throw an error
            auto args = url.substr(arg_start+1);
            url = url.substr(1,arg_start);
            path = http_get_handles.getResult(url,args);// throw error
        }
        catch(char const * e){}
        


        // check path is exists
        struct stat path_stat;
        if(::stat(path.c_str(),&path_stat) == -1)
            throw "file not exists";
        auto block_num = path_stat.st_size/path_stat.st_blksize + (path_stat.st_size%path_stat.st_blksize != 0);
        
        int file_fd = ::open(path.c_str(),O_RDONLY);
        auto map_addr = ::mmap(nullptr, block_num * path_stat.st_blksize,PROT_READ,MAP_SHARED,file_fd,0);
        if(map_addr == MAP_FAILED)
        {
            std::cerr<<::strerror(errno)<<std::endl;
            throw "map failed.";
        }
            
        req->send_buffer = map_addr;
        req->send_buffer_len = path_stat.st_size;

        close(file_fd);
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
    ParseHttpHead(std::string_view full_head,request * req)
    {
        try
        {
            size_t start_index = 0;
            start_index = parseHttpMehod(full_head,start_index);
            auto [index,type] = parseHttpUrl(full_head,start_index,req);
            start_index = index;
            start_index = parseHttpVersion(full_head,start_index);

            // prepare head
            req->http_head = "HTTP/1.0 200 OK\r\n";
            req->http_head += fmt::format("Content-type: {}\r\n",type);
            req->http_head += fmt::format("Content-length: {}\r\n\r\n",req->send_buffer_len);
        }
        catch(char const* e)
        {
            // 404
            if(::strcmp(e,"method is not implemented.")!=0)
            {
                req->send_buffer = static_cast<const void *>(http_404_content);
                req->send_buffer_len = ::strlen(http_404_content);
            }
            else
            {
                req->send_buffer = static_cast<const void *>(unimplemented_content);
                req->send_buffer_len = ::strlen(unimplemented_content);
            }
        }
    }
};