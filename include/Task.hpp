#include<coroutine>
#include"io_uring_stuff.hpp"
#include<sys/socket.h>
#include<netinet/in.h>

class Task
{
public:
    struct promise_type
    {
        std::suspend_never initial_suspend() noexcept{return {};}
        std::suspend_always final_suspend() noexcept{return {};}
        Task get_return_object(){
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        void unhandled_exception() noexcept{}
        void return_void(){}
        int value = 0;
        request * req = nullptr;
    };
    using handle_type  = std::coroutine_handle<promise_type>;

private:
    handle_type  __handle;
public:
    Task(){}
    Task(handle_type handle)
        :__handle(handle)
    {

    }
    void resume(io_uring_cqe * cqe)
    {
        __handle.promise().value = cqe->res;
        __handle.resume();
    }
    request * getRequest()
    {
        return __handle.promise().req;
    }
    handle_type getHandle()
    {
        return __handle;
    }
    bool done()
    {
        return __handle.done();
    }
    void destroy()
    {
        __handle.destroy();
    }
};

class Accept
{
    Task::handle_type __handle;
    ::sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(sockaddr_in);
    int __listen_fd;
    request * __request;
public:
    bool await_ready(){return false;}
    void await_suspend(Task::handle_type handle)
    {
        __handle = handle;
        handle.promise().req = __request;
        ::appAccept(&client_addr,&client_addr_len,__listen_fd, __request);
    }
    int await_resume()
    {
        return __handle.promise().value;
    }
    Accept(int listen_fd,request * req)
        :__listen_fd(listen_fd),__request(req)
    {}
    
};


class Read
{
    Task::handle_type __handle;
    request * __request;
public:
    bool await_ready(){return false;}
    void await_suspend(Task::handle_type handle)
    {
        __handle = handle;
        ::appRecv(__request);
    }
    int await_resume()
    {
        return __handle.promise().value;
    }
    Read(request * req)
        :__request(req)
    {}
};

class Write
{
    Task::handle_type __handle;
    request * __request;
public:
    bool await_ready(){return false;}
    void await_suspend(Task::handle_type handle)
    {
        __handle = handle;
        ::appSend(__request);
    }
    int await_resume()
    {
        return __handle.promise().value;
    }
    Write(request * req)
        :__request(req)
    {}
};