#include"include/Server.hpp"
#include"include/http/HttpProtocol.hpp"

	
#include <sys/types.h>
#include <unistd.h>


int main()
{
    if(::fork()>0)
        return 0;
    ::setsid();
    ::umask(0);
    auto proto = Http("HTTP");
    bool conn = DATABASE::database_conn.connect(password_database,"localhost",user_name,pass_word,0);
    if(!DATABASE::database_conn.connected())
    {
        // need log
        return 0;
    }
    IoUringServer server(DEFALUT_PORT,&proto);
    server.coroRun();
    // server.run();
}

