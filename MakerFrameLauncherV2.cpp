#include <windows.h>
#include <fstream>
#include <sstream>
#include <chrono>

namespace
{
    // 获取当前时间
    std::wstring get_timestamp() {
        SYSTEMTIME st;
        // 获取本地时间
        GetLocalTime(&st);
        wchar_t buf[64];
        // 格式化时间
        if (!swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond))
        {
            return L"";
        }
        return { buf };
    }

    // 转换为UTF-8
    std::string ws_to_utf8(const std::wstring& w) {
        // 为空直接返回
        if (w.empty()) return {};
        // 获取所需缓冲区大小
        const int sz = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        // 失败
        if (sz <= 0) return {};
        // 分配缓冲区
        std::string out; out.resize(sz);
        // 转换
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), out.data(), sz, nullptr, nullptr);
        return out;
    }

    // 写日志
    void write_log(const std::wstring& message) {
        // 格式化日志行
        std::string line = ws_to_utf8(L"["+get_timestamp()+L"] "+message);
        // 确保以换行符结尾
        if (line.empty() || line.back() != '\n') line += '\n';
        std::ofstream ofs;
        // 以二进制追加模式打开日志文件
        ofs.open(ws_to_utf8(L"launcher.log"), std::ios::binary | std::ios::app);
        if (ofs.is_open()) {
            // 写入日志
            ofs.write(line.data(), static_cast<std::streamsize>(line.size()));
            ofs.close();
        }
    }

    // 显示错误信息
    void show_error(const std::wstring& title, const std::wstring& message, const std::wstring& fer_r) {
        // 组合完整信息
        const std::wstring full_msg = message + L"\n\n" + fer_r;
        // 显示消息框
        MessageBoxW(nullptr, full_msg.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
        // 记录日志
        write_log(L"[ERROR] " + message + L" (" + fer_r + L")");
    }

    // 格式化错误信息
    std::wstring format_last_error(const DWORD err) {
        wchar_t* msg_buf = nullptr;
        // 从系统获取错误消息
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&msg_buf), 0, nullptr);
        // 如果获取失败，返回默认消息
        std::wstring res = msg_buf ? msg_buf : L"无法取得错误信息。";
        // 释放缓冲区
        if (msg_buf) LocalFree(msg_buf);
        // 去除尾随换行符/空格
        while (!res.empty() && (res.back() == L'\r' || res.back() == L'\n')) res.pop_back();
        return res;
    }
}

int main(HINSTANCE, HINSTANCE, LPSTR, int) {
    write_log(L"[INFO] MakerFrameLauncherV2 已启动 Powered By TASA-Ed.");
    // 设置 PATH 环境变量
    const wchar_t* paths[] = {
        L".\\bin\\MingW",
        L".\\bin\\Qt5\\bin",
        L".\\bin"
    };
    // 读取当前 PATH
    wchar_t current_path[MAX_PATH] = {};
    GetEnvironmentVariableW(L"PATH", current_path, MAX_PATH);
    std::wstring new_path = current_path;
    // 添加新路径
    for (const auto& path : paths) {
        new_path = std::wstring(path).append(L";").append(new_path);
    }
    // 设置新的 PATH
    if (!SetEnvironmentVariableW(L"PATH", new_path.c_str())) {
        // 失败
        const DWORD err = GetLastError();
        const std::wstring fer_r = format_last_error(err);
        show_error(L"环境错误", L"无法设置 PATH 环境变量.", fer_r);
        return 1;
    }
    
    write_log(L"[INFO] 成功设置 PATH 环境变量.");
    // 启动 MakerFrame.exe
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    if (!CreateProcessW(L"MakerFrame.exe", nullptr, nullptr, nullptr, FALSE,
        0, nullptr, nullptr, &si, &pi)) {
        const DWORD err = GetLastError();
        const std::wstring fer_r = format_last_error(err);
        show_error(L"启动错误", L"无法启动 MakerFrame.exe.", fer_r);
        return 1;
    }

    write_log(L"[INFO] 成功启动 MakerFrame.exe. (PID=" + std::to_wstring(static_cast<long long>(pi.dwProcessId)) + L")");
    // 退出
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    write_log(L"[INFO] 启动器正常退出.");
    return 0;
}