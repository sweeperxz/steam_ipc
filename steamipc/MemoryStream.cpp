#include "MemoryStream.h"

#include <cwchar>
#include <stdexcept>

// WARNING: handle leaks on exceptions
memory_stream::memory_stream(const wchar_t* name_format)
{
    wchar_t name[MAX_PATH];

    if (swprintf(name, ARRAYSIZE(name), name_format, L"mem") == -1)
        throw std::invalid_argument("Name format is invalid.");

    file_mapping_handle_ = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, name);

    if (file_mapping_handle_ == nullptr)
        throw std::runtime_error("Failed to open a handle to the file mapping object.");

    swprintf(name, ARRAYSIZE(name), name_format, L"avail");
    available_handle_ = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, name);

    if (available_handle_ == nullptr)
        throw std::runtime_error("Failed to open a handle to the available event object.");

    swprintf(name, ARRAYSIZE(name), name_format, L"written");
    written_handle_ = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, name);

    if (written_handle_ == nullptr)
        throw std::runtime_error("Failed to open a handle to the written event object.");

    swprintf(name, ARRAYSIZE(name), name_format, L"mutex");
    mutex_handle_ = OpenMutexW(SYNCHRONIZE, FALSE, name);

    if (mutex_handle_ == nullptr)
        throw std::runtime_error("Failed to open a handle to the mutex object.");

    file_mapping_view_ = MapViewOfFile(file_mapping_handle_, FILE_MAP_READ | FILE_MAP_WRITE, NULL, NULL, NULL);

    if (file_mapping_view_ == nullptr)
        throw std::runtime_error("Failed to map a view of the file mapping.");

    header_ = static_cast<stream_header*>(file_mapping_view_);
    buffer_ = static_cast<char*>(file_mapping_view_) + sizeof(stream_header);
}

memory_stream::~memory_stream()
{
    CloseHandle(file_mapping_handle_);
    CloseHandle(available_handle_);
    CloseHandle(written_handle_);
    CloseHandle(mutex_handle_);

    UnmapViewOfFile(file_mapping_view_);
}

void memory_stream::put_front(const void* source, DWORD size) const
{
    if (source == nullptr)
        throw std::invalid_argument("Source is null.");

    if (size > get_unused_capacity())
        throw std::out_of_range("Size exceeds the unused capacity of the buffer.");

    char* destination;

    header_->buffer_size += size;

    // write is wrapped
    if (size > header_->buffer_start_index)
    {
        DWORD postwrapSize = size - header_->buffer_start_index;
        // update size
        size = header_->buffer_start_index;
        // wrap buffer start index
        header_->buffer_start_index = header_->buffer_capacity - postwrapSize;

        memcpy(buffer_ + header_->buffer_start_index, source, postwrapSize);
        // update source
        source = (char*)source + postwrapSize;
        // finish write at start
        destination = buffer_;
    }

    else
    {
        header_->buffer_start_index -= size;
        destination = buffer_ + header_->buffer_start_index;
    }

    memcpy(destination, source, size);
}

void memory_stream::get(void* destination, DWORD size) const
{
    if (destination == nullptr)
        throw std::invalid_argument("Destination is null.");

    if (size > header_->buffer_size)
        throw std::out_of_range("Size is out of range of the buffer.");

    const char* source = buffer_ + header_->buffer_start_index;

    // stream is wrapped
    if (header_->buffer_start_index > header_->buffer_end_index)
    {
        // get is wrapped
        if (const DWORD prewrap_size = header_->buffer_capacity - header_->buffer_start_index; size > prewrap_size)
        {
            memcpy(destination, source, prewrap_size);
            // update args
            destination = static_cast<char*>(destination) + prewrap_size;
            size -= prewrap_size;
            // wrap source
            source = buffer_;
        }
    }

    memcpy(destination, source, size);
}

HANDLE memory_stream::get_available_handle() const
{
    return available_handle_;
}

HANDLE memory_stream::get_written_handle() const
{
    return written_handle_;
}

HANDLE memory_stream::get_mutex_handle() const
{
    return mutex_handle_;
}

DWORD memory_stream::get_unused_capacity() const
{
    return header_->buffer_size >= header_->buffer_capacity
               ? NULL
               : header_->buffer_capacity - header_->buffer_size;
}

DWORD memory_stream::get_capacity() const
{
    return header_->buffer_capacity;
}

DWORD memory_stream::get_size() const
{
    return header_->buffer_size;
}
