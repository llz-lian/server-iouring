#pragma once
#include<string>
#include<string_view>
#include<unistd.h>
enum class EVENT_TYPE
{
    ACCEPT,
    READ,
    WRITE
};
enum class PRO_STATE
{
    COMPLETE,
    ERROR,
    CONTINUE
};