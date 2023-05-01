#include"include/Server.hpp"





int main()
{
    IoUringClient redis_client(REDIS_IP,REDIS_PORT);


    IoUringServer server(DEFALUT_PORT);



    server.run();
}

