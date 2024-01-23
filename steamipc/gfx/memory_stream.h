#pragma once

#include <Windows.h>

class memory_stream
{
public:
    memory_stream(const wchar_t* name_format);
    ~memory_stream();

    void put_front(const void* source, DWORD size) const;
    void get(void* destination, DWORD size) const;

    HANDLE get_available_handle() const;
    HANDLE get_written_handle() const;
    HANDLE get_mutex_handle() const;

    DWORD get_unused_capacity() const;
    DWORD get_capacity() const;
    DWORD get_size() const;

private:
    struct stream_header
    {
        DWORD buffer_start_index;
        DWORD buffer_end_index;
        DWORD buffer_capacity;
        DWORD buffer_size;
    };

    HANDLE file_mapping_handle_;
    HANDLE available_handle_;
    HANDLE written_handle_;
    HANDLE mutex_handle_;

    LPVOID file_mapping_view_;

    stream_header* header_;
    char* buffer_;
};
