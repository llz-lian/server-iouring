#include<string>
#include<string_view>
#define MYSQLPP_MYSQL_HEADERS_BURIED
#include<mysql++/mysql++.h>
#include<fmt/core.h>

class Handler
{
private:
    mysqlpp::Connection database_conn{false};
public:
    

};




inline std::string loginHandler(std::string_view user_name,std::string_view password)
{
    if(user_name == "llz" &&password == "123")
        return "afterlogin/login_succ.html";
    return "afterlogin/login_fail.html";
}

inline std::string registerHandler(std::string_view user_name,std::string_view password)
{
    return "";
}