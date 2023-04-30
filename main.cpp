#include<iostream>
#include<fmt/core.h>
#include<unordered_map>
#include<string_view>
#include<string>
#include<sstream>

#include<netinet/in.h>
#include<sys/socket.h>
#include<unistd.h>
#include<fcntl.h>
#include<liburing.h>
#include<sys/stat.h>
#include<memory.h>
#include<sys/mman.h>

constexpr inline int DEFALUT_PORT = 8000;
constexpr inline int QUEUE_DEPTH = 512;
constexpr inline int READ_SZ = 4096;
const char *unimplemented_content = \
        "HTTP/1.0 400 Bad Request\r\n"
        "Content-type: text/html\r\n"
        "\r\n"
        "<html>"
        "<head>"
        "<title>ZeroHTTPd: Unimplemented</title>"
        "</head>"
        "<body>"
        "<h1>Bad Request (Unimplemented)</h1>"
        "<p>Your client sent a request ZeroHTTPd did not understand and it is probably not your fault.</p>"
        "</body>"
        "</html>";

const char *http_404_content = \
        "HTTP/1.0 404 Not Found\r\n"
        "Content-type: text/html\r\n"
        "\r\n"
        "<html>"
        "<head>"
        "<title>ZeroHTTPd: Not Found</title>"
        "</head>"
        "<body>"
        "<h1>Not Found (404)</h1>"
        "<p>Your client is asking for an object that was not found on this server.</p>"
        "</body>"
        "</html>";

enum class EVENT_TYPE
{
    ACCEPT,
    READ,
    WRITE
};
class request
{
public:
    EVENT_TYPE event_type = EVENT_TYPE::ACCEPT;
    int client_fd = -1;
    std::string read_buffer;
    const void * send_buffer;
    size_t send_buffer_len;
    std::string http_head;
};

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


    size_t toNextBlank(std::string_view str,size_t index)
    {
        while(index<str.size()&&str[index]!=' ') index++;
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
    void handleGetBody(std::string_view body)
    {

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
    bool ok = false;
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
            if(::strcmp(e,"method is not implemented."))
            {
                req->send_buffer = static_cast<const void *>(unimplemented_content);
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

class IoUringServer
{
private:
    io_uring __ring;
    int __listen_fd;
    std::unordered_map<int,request*> __requests;
    void __acceptFd(sockaddr_in * client_addr,socklen_t * client_addr_len)
    {
        io_uring_sqe * sqe = io_uring_get_sqe(&__ring);
        io_uring_prep_accept(sqe,__listen_fd,reinterpret_cast<sockaddr *>(client_addr),client_addr_len,0);
        request * req = new request;
        io_uring_sqe_set_data(sqe,req);
        io_uring_submit(&__ring);
    }
    void __Handleread(int client_fd)
    {
        io_uring_sqe * sqe = io_uring_get_sqe(&__ring);
        request * req = new request;
        __requests[client_fd] = req;
        req->client_fd = client_fd;

        req->event_type = EVENT_TYPE::READ;
        req->read_buffer.resize(READ_SZ);
        io_uring_prep_read(sqe,client_fd,req->read_buffer.data(),req->read_buffer.size(),0);
        io_uring_sqe_set_data(sqe,req);
        io_uring_submit(&__ring);
    }
    void __handleProcess(request * req)
    {
        // echo back data
        // req->send_buffer = req->read_buffer;
        // or..
        // parse http head set req->buffer
        ParseHttpHead(req->read_buffer,req);
        // std::cout<<"send."<<std::endl;

    }
    void __handleWrite(request * req)
    {
        io_uring_sqe * sqe = io_uring_get_sqe(&__ring);
        req->event_type = EVENT_TYPE::WRITE;

        io_uring_prep_write(sqe,req->client_fd,req->http_head.data(),req->http_head.size(),0);
        // io_uring_sqe_set_data(sqe,req);
        // io_uring_submit(&__ring);
        // send body
        sqe = io_uring_get_sqe(&__ring);
        io_uring_prep_write(sqe,req->client_fd,req->send_buffer,req->send_buffer_len,0);
        io_uring_sqe_set_data(sqe,req);
        io_uring_submit(&__ring);
    }
public:
    IoUringServer(int port)
    {
        // init ring
        io_uring_queue_init(QUEUE_DEPTH,&__ring, 0);
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
        __acceptFd(&client_addr,&client_addr_len);
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
                    __acceptFd(&client_addr,&client_addr_len);// append accept
                    __Handleread(cqe->res);
                    break;
                case EVENT_TYPE::READ:
                // read complete
                    __handleProcess(req);
                    __handleWrite(req);
                    break;
                case EVENT_TYPE::WRITE:
                // write complete
                    int client_fd = req->client_fd;
                    close(client_fd);
                    auto munmap_ret = ::munmap(const_cast<void *>(req->send_buffer),req->send_buffer_len);
                    if(munmap_ret<0)
                    {
                        std::cerr<<fmt::format("munmap error: {}.",::strerror(errno))<<std::endl;
                    }
                    // __requests[client_fd] = nullptr;
                    __requests.erase(client_fd);
                    delete req;

            }
            io_uring_cqe_seen(&__ring,cqe);
        }
    }
};

int main()
{
    IoUringServer server(DEFALUT_PORT);
    server.run();
}

