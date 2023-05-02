#include"include/Server.hpp"
#include"include/http/HttpProtocol.hpp"

	
#include <sys/types.h>
#include <unistd.h>


int main()
{
    // if(::fork()>0)
    //     return 0;
    // ::setsid();
    // ::umask(0);
    auto proto = Http("HTTP");
    IoUringServer server(DEFALUT_PORT,&proto);
    server.run();
}

