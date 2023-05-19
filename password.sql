use server_uring;
/* drop table user_password;
create table user_password(
    user_name char(128) unique not null,
    password char(128) not null,
    primary key(user_name,password)
); */

select * from user_password;
/* create user 'password_keeper'@'localhost' identified by 'Password_!23';
grant select,alter,insert on server_uring.user_password to 'password_keeper'@'localhost'; */