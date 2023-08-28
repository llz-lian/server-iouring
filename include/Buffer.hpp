#pragma once

#include<memory>
#include<sys/mman.h>
#include<sys/stat.h>
#include<string_view>
#include<fcntl.h>

#include<io_uring_stuff.hpp>
struct Buffer
{
    Buffer * next = nullptr;
    void * mem;
    bool need_munmap = false;
    size_t buffer_len;
    
    Buffer(size_t buffer_len)
        :buffer_len(buffer_len)
        {
            mem = ::malloc(buffer_len * sizeof(unsigned char));
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
            if(mem) ::free(mem);
        }
        mem = nullptr;
        buffer_len = 0u;
    }
    void dropTail()
    {
        if(next)
        {
            delete next;
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
        deleteAll();
        mem = ::malloc(len);
        buffer_len = len;

        ::memset(mem,0,sizeof(unsigned char) * buffer_len);
        ::memcpy(mem,buf,len);
    }
};




// namespace BufferRing
// {
//     io_uring_buf_ring * buf_ring = nullptr;
//     Buffer * buffer;

//     void initBuffer()
//     {
//         buffer = new Buffer(READ_SZ);
//         for(size_t i = 0;i<BUF_RING_LEN;i++)
//         {
//             buffer->appendNext(READ_SZ);
//         }
//     }
//     io_uring_buf_ring* initBufferRing()
//     {
//         // init buffer ring
//         io_uring_buf_reg reg = { };
//         if (posix_memalign((void **) &buf_ring, 4096,BUF_RING_LEN * sizeof(struct io_uring_buf_ring)))
// 		    return nullptr;
//         reg.bgid = 0;
//         reg.ring_addr = reinterpret_cast<unsigned long long>(buf_ring);
//         reg.ring_entries = BUF_RING_LEN;

//         if(int ret = io_uring_register_buf_ring(&__ring,&reg,0);ret<0)
//         {
//             //err
//             return nullptr;
//         }
//         io_uring_buf_ring_init(buf_ring);
//         // append buffers
//         initBuffer();
//         for(size_t i = 0 ;i<BUF_RING_LEN;i++)
//         {
//             io_uring_buf_ring_add(
//                 buf_ring,
//                 buffer->mem,
//                 buffer->buffer_len,
//                 i,
//                 io_uring_buf_ring_mask(BUF_RING_LEN),
//                 i
//             );
//             buffer = buffer->next;
//         }
//         // visible to kernel
//         io_uring_buf_ring_advance(buf_ring,BUF_RING_LEN);
//         return buf_ring;
//     }
// };




