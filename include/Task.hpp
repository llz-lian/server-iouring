#include<coroutine>
#include"io_uring_stuff.hpp"



class Task
{
public:
    struct promise_type
    {
        std::suspend_never initial_suspend(){return {};}
        std::suspend_never final_suspend(){return {};}
        Task get_return_object(){

        }
        void return_void()noexcept {}
    };
private:
    using handle_type  = std::coroutine_handle<promise_type>;
    handle_type  __handle;
    request * __now_req;
public:
    Task(handle_type handle)
        :__handle(handle)
    {

    }
    void resume()
    {
        __handle.resume();
    }
    request * getReq()
    {
        return __now_req;
    }
};