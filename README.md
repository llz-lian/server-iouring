# 依赖
1. -std=c++20
2. liburing：[io_uring作者的liburing](https://github.com/axboe/liburing)
3. fmt：[gcc13才有std::format](https://fmt.dev/latest/index.html)
# 测试
## 测试机：
![aliyun](https://github.com/llz-lian/server-iouring/blob/main/imgs/aliyun.PNG)
## 测试结果：
![result](https://github.com/llz-lian/server-iouring/blob/main/imgs/result-link.PNG)
# 阿里云
链接==>[🔗(不一定开着)](http://47.113.149.181:8000/)
# 参考资料
## c++ 协程
图最好背下来
要明白任务类似这种：
1. accept listen_fd get client_fd
2. read client_fd
3. process
4. wirte client_fd
5. continue read?goto read or return
```
Task Work()
{
    http_request = new http_request;
    int client_fd = co_awiat Accept(listen_fd);// handle need new work
    // client_fd = await_resume(); cqe->res;
    while(true)
    {
        int read_num = co_awiat Read(client_fd,read_buffer);
        int keep_read = process(http_request);
        co_await Write(client_fd, write_buffer);
        if(!keep_read) break;
    }
    delete request;
    close()
}
```
1. 知乎：[C++20协程不完全指南](https://zhuanlan.zhihu.com/p/436133279)
2. cpp reference：[link](https://zh.cppreference.com/w/cpp/language/coroutines)
3. 知乎：[如何编写 C++ 20 协程(Coroutines)](https://zhuanlan.zhihu.com/p/355100152)
4. CSDN用户RzBu11d023r：[有点东西，虽然有点乱](https://blog.csdn.net/u010180372?type=blog)
## io_uring
1. 知乎：[一篇文章带你读懂 io_uring 的接口与实现](https://zhuanlan.zhihu.com/p/380726590)
1. c语言，包含cat、cp程序，小型webserver：[lord of the io_uring](https://unixism.net/loti/)
2. bound workers和unbound workers：[wokers](https://blog.cloudflare.com/missing-manuals-io_uring-worker-pool/)
3. [liburing issues](https://github.com/axboe/liburing/issues)
4. man liburing接口