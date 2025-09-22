#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")

namespace {
    // 转换为UTF-8
    std::string ws_to_utf8(const std::wstring& w) {
        if (w.empty()) return {};
        const int sz = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        if (sz <= 0) return {};
        std::string out; out.resize(sz);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), out.data(), sz, nullptr, nullptr);
        return out;
    }

    // 在日志文件末尾追加一行UTF-8编码内容
    void write_log_line(const std::wstring& log_path_w, const std::wstring& text) {
        std::string line = ws_to_utf8(text);
        // ensure newline
        if (line.empty() || line.back() != '\n') line += '\n';
        std::ofstream ofs;
        ofs.open(ws_to_utf8(log_path_w), std::ios::binary | std::ios::app);
        if (ofs.is_open()) {
            ofs.write(line.data(), static_cast<std::streamsize>(line.size()));
            ofs.close();
        }
    }

	// 获取时间
    std::wstring now_timestamp() {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t buf[64];
        if (!swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond))
        {
            return L"";
        }
        return {buf};
    }

	// 日志函数
    void log(const std::wstring& log_path_w, const std::wstring& msg) {
        const std::wstring line = now_timestamp() + L"  " + msg;
        write_log_line(log_path_w, line);
    }

	// 格式化错误信息
    std::wstring format_last_error(const DWORD err) {
        wchar_t* msg_buf = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&msg_buf), 0, nullptr);
        std::wstring res = msg_buf ? msg_buf : L"(无法取得错误信息)";
        if (msg_buf) LocalFree(msg_buf);
        // trim trailing newlines/spaces
        while (!res.empty() && (res.back() == L'\r' || res.back() == L'\n')) res.pop_back();
        return res;
    }

	// 显示弹窗并记录日志
    void show_error_and_log(const std::wstring& log_path_w, const std::wstring& title, const std::wstring& prefix) {
        const DWORD err = GetLastError();
        const std::wstring fer_r = format_last_error(err);
        const std::wstring full_msg = prefix + L"\n\n" + fer_r;
        log(log_path_w, L"[ERROR] " + prefix);
        log(log_path_w, L"[ERROR] 系统消息: " + fer_r);
        MessageBoxW(nullptr, full_msg.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
    }
}

int main() {
	// 获取工作目录
    wchar_t full_path[MAX_PATH];
	if (DWORD n = GetModuleFileNameW(nullptr, full_path, MAX_PATH); n == 0 || n == MAX_PATH) {
        MessageBoxW(nullptr, L"无法获取当前工作目录路径。", L"错误", MB_ICONERROR);
        return 1;
    }
    // 格式化路径
    wchar_t* last_slash = wcsrchr(full_path, L'\\');
    if (!last_slash) {
        MessageBoxW(nullptr, L"路径格式异常。", L"错误", MB_ICONERROR);
        return 1;
    }
    *last_slash = L'\0';
    std::wstring dir = full_path;

    // 日志文件路径
    std::wstring log_path = dir + L"\\launcher.log";

    log(log_path, L"[INFO] MakerFrameLauncherV2 已启动");

    // 定义环境变量
    std::wstring lib_path = dir + L"\\bin\\";
    std::wstring msys_path = dir + L"\\bin\\MingW\\";
    std::wstring qt_path = dir + L"\\bin\\Qt5\\bin\\";

    log(log_path, L"[INFO] libPath: " + lib_path);
    log(log_path, L"[INFO] msysPath: " + msys_path);
    log(log_path, L"[INFO] qtPath: " + qt_path);

	// 读取旧的 PATH 环境变量
    DWORD needed = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    std::wstring old_path;
    if (needed > 0) {
        old_path.resize(needed);
        GetEnvironmentVariableW(L"PATH", old_path.data(), needed);
        if (!old_path.empty() && old_path.back() == L'\0') old_path.pop_back();
    }

    // 写入新的环境变量
    std::wstring new_path = msys_path + L";" + qt_path + L";" + lib_path;
    if (!old_path.empty()) {
        new_path += L";";
        new_path += old_path;
    }

    if (!SetEnvironmentVariableW(L"PATH", new_path.c_str())) {
	    show_error_and_log(log_path, L"错误", L"无法设置 PATH 环境变量");
        // 非致命错误，仍然尝试启动
    }
    else {
	    log(log_path, L"[INFO] 成功设置 PATH 环境变量");
    }

    // 启动程序
    std::wstring maker = dir + L"\\MakerFrame.exe";
    log(log_path, L"[INFO] 将启动: " + maker);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    BOOL ok = CreateProcessW(
        maker.c_str(),     // lpApplicationName
        nullptr,           // lpCommandLine
        nullptr,           // lpProcessAttributes
        nullptr,           // lpThreadAttributes
        FALSE,             // bInheritHandles
        0,                 // dwCreationFlags
        nullptr,           // lpEnvironment (NULL -> inherit)
        dir.c_str(),       // lpCurrentDirectory
        &si,
        &pi
    );

    if (!ok) {
	    show_error_and_log(log_path, L"错误", L"无法启动 MakerFrame.exe");
        log(log_path, L"[INFO] 启动器异常退出。");
        return 1;
    }

	// 输出程序 PID
    log(log_path, L"[INFO] 成功启动 MakerFrame.exe. PID=" + std::to_wstring(static_cast<long long>(pi.dwProcessId)));

	// 关闭句柄并退出启动器
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    log(log_path, L"[INFO] 启动器退出。");
    return 0;
}
