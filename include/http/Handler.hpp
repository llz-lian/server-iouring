#define MYSQLPP_MYSQL_HEADERS_BURIED

#include<string>
#include<string_view>
#include<mysql++/mysql++.h>
#include<fmt/core.h>
#include<unordered_map>
namespace DATABASE
{
    inline mysqlpp::Connection database_conn;
    // bool conn = database_conn.connect("server_uring","localhost","root","llzllz123",0);
};
namespace Handler
{
    enum {Login,regist};
    template<int i,class ... Args>
    decltype(auto) htmlHandle(Args & ...args);
    template<int i,class ... Args>
    decltype(auto) htmlHandle(Args && ...args);
    template<>
    decltype(auto) htmlHandle<Login,std::string, std::string>
    (std::string & user_name, std::string & password)
    {
        // auto select_req = fmt::format("select password from user_password where user_name = '{}'",user_name);
        mysqlpp::Query query = DATABASE::database_conn.query(fmt::format("select password from user_password where user_name = '{}'",user_name));
        if(auto res = query.store())
        {
            if(res.empty())
            {
                return std::string("afterlogin/login_fail.html");
            }
            auto row = *res.begin();
            std::string real_password = row[0].c_str();
            if(real_password == password)
            {
                return std::string("afterlogin/login_succ.html");
            }
        }
        return std::string("afterlogin/login_fail.html");        
    }
    template<>
    decltype(auto) htmlHandle<regist,std::string, std::string>
    (std::string & user_name, std::string & password)
    {
        try
        {
            mysqlpp::Query query = DATABASE::database_conn.query(fmt::format("insert into user_password values(\"{}\",\"{}\")",user_name,password));
            query.execute();
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            return std::string("public/sign_up.html");
        }
        return std::string("public/sign_up.html");
    }

};

// namespace Handler
// {
//     enum{next = regist+1};//....
//     template<>
//     decltype(auto) htmlHandle<next ,......>(.....);
// };