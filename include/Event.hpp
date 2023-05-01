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