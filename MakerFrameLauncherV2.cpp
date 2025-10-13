#include <windows.h>
#include <fstream>
#include <sstream>
#include <chrono>

namespace
{
    std::wstring get_timestamp() {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t buf[64];
        if (!swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond))
        {
            return L"";
        }
        return { buf };
    }

    // 转换为UTF-8
    std::string ws_to_utf8(const std::wstring& w) {
        if (w.empty()) return {};
        const int sz = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        if (sz <= 0) return {};
        std::string out; out.resize(sz);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), out.data(), sz, nullptr, nullptr);
        return out;
    }

    void write_log(const std::wstring& message) {
        std::string line = ws_to_utf8(L"["+get_timestamp()+L"] "+message);
        // ensure newline
        if (line.empty() || line.back() != '\n') line += '\n';
        std::ofstream ofs;
        ofs.open(ws_to_utf8(L"launcher.log"), std::ios::binary | std::ios::app);
        if (ofs.is_open()) {
            ofs.write(line.data(), static_cast<std::streamsize>(line.size()));
            ofs.close();
        }
    }

    void show_error(const std::wstring& title, const std::wstring& message, const std::wstring& fer_r) {
        const std::wstring full_msg = message + L"\n\n" + fer_r;
        MessageBoxW(nullptr, full_msg.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
        write_log(L"[ERROR] " + message + L" (" + fer_r + L")");
    }

    // 格式化错误信息
    std::wstring format_last_error(const DWORD err) {
        wchar_t* msg_buf = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&msg_buf), 0, nullptr);
        std::wstring res = msg_buf ? msg_buf : L"无法取得错误信息。";
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

    wchar_t current_path[MAX_PATH] = {};
    GetEnvironmentVariableW(L"PATH", current_path, MAX_PATH);
    std::wstring new_path = current_path;

    for (const auto& path : paths) {
        new_path = std::wstring(path).append(L";").append(new_path);
    }

    if (!SetEnvironmentVariableW(L"PATH", new_path.c_str())) {
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