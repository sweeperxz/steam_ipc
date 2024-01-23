#include <Windows.h>
#include <iostream>
#include <TlHelp32.h>
#include "MemoryStream.h"

class render_command {
public:
    virtual ~render_command() = default;
    virtual void execute(memory_stream& render_stream) = 0;
};

class textured_rect_command final : public render_command {
public:
    textured_rect_command(const int x0, const int y0, const int x1, const int y1, const float u0, const float v0, const float u1, const float v1, const float uk4,
                          const DWORD color_start, const DWORD color_end, const DWORD gradient_direction, const DWORD texture_id)
        : x0_(x0), y0_(y0), x1_(x1), y1_(y1), u0_(u0), v0_(v0), u1_(u1), v1_(v1), uk4_(uk4), color_start_(color_start),
          color_end_(color_end), gradient_direction_(gradient_direction), texture_id_(texture_id) {}

    virtual void execute(memory_stream& render_stream) override {
        struct render_command_data {
            DWORD render_command;
            int x0, y0, x1, y1;
            float u0, v0, u1, v1, uk4;
            DWORD color_start, color_end, gradient_direction, texture_id;
        };

        const render_command_data data = {
            3, // render command
            x0_, y0_, x1_, y1_,
            u0_, v0_, u1_, v1_, uk4_,
            color_start_, color_end_, gradient_direction_, texture_id_
        };
        render_stream.put_front(&data, sizeof(data));
    }

private:
    int x0_, y0_, x1_, y1_;
    float u0_, v0_, u1_, v1_, uk4_;
    DWORD color_start_, color_end_, gradient_direction_, texture_id_;
};

class texture_loading_command final : public render_command {
public:
    texture_loading_command(const DWORD texture_id, DWORD& texture_version)
        : texture_id_(texture_id), version_(++texture_version), full_update_(1),
          size_(400 * 400 * 4), width_(400), height_(400), x_(0), y_(0) {}

    virtual void execute(memory_stream& render_stream) override {
        struct render_command_data {
            DWORD render_command, texture_id, version, full_update, size, width, height, x, y;
        };

        const render_command_data data = {
            1, // render command
            texture_id_, version_, full_update_,
            size_, width_, height_, x_, y_
        };
        render_stream.put_front(&data, sizeof(data));
    }

private:
    DWORD texture_id_, version_, full_update_, size_, width_, height_, x_, y_;
};

DWORD get_process_id_by_name(const wchar_t* process_name) {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W process_entry;
    process_entry.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot, &process_entry))
    {
        do
        {
            if (_wcsicmp(process_entry.szExeFile, process_name) == 0)
            {
                CloseHandle(snapshot);
                return process_entry.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &process_entry));
    }

    CloseHandle(snapshot);
    return 0;
}

class renderer {
public:
    explicit renderer(const DWORD process_id) : render_stream_(create_render_stream(process_id)), input_stream_(create_input_stream(process_id)) {}

    ~renderer() {
        CloseHandle(render_stream_.get_mutex_handle());
        CloseHandle(input_stream_.get_mutex_handle());
    }

    void run()
    {
        HANDLE handles[] =
        {
            render_stream_.get_mutex_handle(),
            input_stream_.get_mutex_handle(),
            input_stream_.get_available_handle()
        };

        bool loaded = false;
        DWORD texture_version = 0;

        while (true)
        {
            WaitForMultipleObjects(ARRAYSIZE(handles), handles, TRUE, INFINITE);

            const std::unique_ptr<render_command> render_command = create_render_command(loaded, texture_version);
            render_command->execute(render_stream_);

            ReleaseMutex(render_stream_.get_mutex_handle());
            ReleaseMutex(input_stream_.get_mutex_handle());
        }
    }

private:
    memory_stream render_stream_;
    memory_stream input_stream_;

    static memory_stream create_render_stream(const DWORD process_id) {
        wchar_t name_format[MAX_PATH];
        swprintf(name_format, ARRAYSIZE(name_format), L"GameOverlayRender_PaintCmdStream_%d_%%s-IPCWrapper", process_id);
        return {name_format};
    }

    static memory_stream create_input_stream(const DWORD process_id) {
        wchar_t name_format[MAX_PATH];
        swprintf(name_format, ARRAYSIZE(name_format), L"GameOverlay_InputEventStream_%d_%%s-IPCWrapper", process_id);
        return {name_format};
    }

    static std::unique_ptr<render_command> create_render_command(bool& loaded, DWORD& texture_version) {
        if (!loaded)
        {
            loaded = true;
            return std::make_unique<texture_loading_command>(1337, texture_version);
        }
        else
        {
            return std::make_unique<textured_rect_command>(0, 0, 400, 400, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
                                                         0xFFFFFFFF, 0xFFFFFFFF, 3, 1337);
        }
    }
};

int main(int argc, char* argv[]) {
    const auto process_name = L"cs2.exe";
    const DWORD process_id = get_process_id_by_name(process_name);

    if (process_id == 0) {
        std::wcerr << "Unable to find process ID for: " << process_name << '\n';
        return 1;
    }

    renderer renderer(process_id);
    renderer.run();

    return 0;
}
