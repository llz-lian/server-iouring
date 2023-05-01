#include"include/Server.hpp"
#include"include/http/HttpProtocol.hpp"




int main()
{
    auto proto = Http("HTTP");
    IoUringServer server(DEFALUT_PORT,&proto);
    server.run();
}

