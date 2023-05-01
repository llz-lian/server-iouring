#pragma once

#include<memory>
#include<sys/mman.h>
#include<sys/stat.h>
#include<string_view>
#include<fcntl.h>
struct Buffer
{
    Buffer * next = nullptr;
    void * mem;
    bool need_munmap = false;
    size_t buffer_len;
    
    Buffer(size_t buffer_len)
        :buffer_len(buffer_len),mem(::malloc(buffer_len * sizeof(unsigned char))){
            ::memset(mem,0,sizeof(unsigned char) * buffer_len);
        }
    void mmapMem(std::string file_path)
    {
        struct stat path_stat;
        if(::stat(file_path.c_str(),&path_stat) == -1)
            throw "file not exists";
        auto block_num = path_stat.st_size/path_stat.st_blksize + (path_stat.st_size%path_stat.st_blksize != 0);
        int file_fd = ::open(file_path.c_str(),O_RDONLY);
        auto map_addr = ::mmap(nullptr, block_num * path_stat.st_blksize,PROT_READ,MAP_SHARED,file_fd,0);
        close(file_fd);
        if(map_addr == MAP_FAILED)
        {
            throw "map failed.";
        }
        deleteAll();
        need_munmap = true;
        mem = map_addr;
        buffer_len = path_stat.st_size;
    }
    void deleteAll()
    {
        if(need_munmap)
        {
            ::munmap(mem,buffer_len);
            need_munmap = false;
        }
        else
        {
            if(mem) delete[] static_cast<unsigned char *>(mem);
        }
        mem = nullptr;
        buffer_len = 0u;
    }
    void dropTail()
    {
        if(next)
        {
            next->~Buffer();
        }
        next = nullptr;
    }
    ~Buffer()
    {
        deleteAll();
        dropTail();
    }
    void appendNext(size_t buffer_len)
    {
        auto ptr_pre = this;
        auto ptr = next;
        while(ptr!=nullptr)
        {
            ptr_pre = ptr;
            ptr = ptr->next;
        }
        ptr_pre->next = new Buffer(buffer_len);
    }
    
    void copy(const char * buf)
    {
        size_t len = ::strlen(buf);
        if(len!=buffer_len)
        {
            ::realloc(mem,len+1);
        }
        buffer_len = len;
        ::memset(mem,0,sizeof(unsigned char) * buffer_len);
        ::memcpy(mem,buf,len);
    }
};





