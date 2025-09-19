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
        int sz = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        if (sz <= 0) return {};
        std::string out; out.resize(sz);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), out.data(), sz, nullptr, nullptr);
        return out;
    }

    // 在日志文件末尾追加一行UTF-8编码内容
    void WriteLogLine(const std::wstring& logPathW, const std::wstring& text) {
        std::string line = ws_to_utf8(text);
        // ensure newline
        if (line.empty() || line.back() != '\n') line += '\n';
        std::ofstream ofs;
        ofs.open(ws_to_utf8(logPathW), std::ios::binary | std::ios::app);
        if (ofs.is_open()) {
            ofs.write(line.data(), static_cast<std::streamsize>(line.size()));
            ofs.close();
        }
    }

	// 获取时间
    std::wstring NowTimestamp() {
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
    void Log(const std::wstring& logPathW, const std::wstring& msg) {
        std::wstring line = NowTimestamp() + L"  " + msg;
        WriteLogLine(logPathW, line);
    }

	// 格式化错误信息
    std::wstring FormatLastError(DWORD err) {
        wchar_t* msgBuf = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&msgBuf), 0, nullptr);
        std::wstring res = msgBuf ? msgBuf : L"(无法取得错误信息)";
        if (msgBuf) LocalFree(msgBuf);
        // trim trailing newlines/spaces
        while (!res.empty() && (res.back() == L'\r' || res.back() == L'\n')) res.pop_back();
        return res;
    }

	// 显示弹窗并记录日志
    void ShowErrorAndLog(const std::wstring& logPathW, const std::wstring& title, const std::wstring& prefix) {
        DWORD err = GetLastError();
        std::wstring ferR = FormatLastError(err);
        std::wstring fullMsg = prefix + L"\n\n" + ferR;
        Log(logPathW, L"[ERROR] " + prefix);
        Log(logPathW, L"[ERROR] 系统消息: " + ferR);
        MessageBoxW(nullptr, fullMsg.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
    }
}

int main() {
	// 获取工作目录
    wchar_t fullPath[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, fullPath, MAX_PATH);
    if (n == 0 || n == MAX_PATH) {
        MessageBoxW(nullptr, L"无法获取当前工作目录路径。", L"错误", MB_ICONERROR);
        return 1;
    }
    // 格式化路径
    wchar_t* lastSlash = wcsrchr(fullPath, L'\\');
    if (!lastSlash) {
        MessageBoxW(nullptr, L"路径格式异常。", L"错误", MB_ICONERROR);
        return 1;
    }
    *lastSlash = L'\0';
    std::wstring dir = fullPath;

    // 日志文件路径
    std::wstring logPath = dir + L"\\launcher.log";

    Log(logPath, L"[INFO] MakerFrameLauncherV2 已启动");

    // 定义环境变量
    std::wstring libPath = dir + L"\\bin\\";
    std::wstring msysPath = dir + L"\\bin\\MingW\\";
    std::wstring qtPath = dir + L"\\bin\\Qt5\\bin\\";

    Log(logPath, L"[INFO] libPath: " + libPath);
    Log(logPath, L"[INFO] msysPath: " + msysPath);
    Log(logPath, L"[INFO] qtPath: " + qtPath);

	// 读取旧的 PATH 环境变量
    DWORD needed = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    std::wstring oldPath;
    if (needed > 0) {
        oldPath.resize(needed);
        GetEnvironmentVariableW(L"PATH", oldPath.data(), needed);
        if (!oldPath.empty() && oldPath.back() == L'\0') oldPath.pop_back();
    }

    // 写入新的环境变量
    std::wstring newPath = msysPath + L";" + qtPath + L";" + libPath;
    if (!oldPath.empty()) {
        newPath += L";";
        newPath += oldPath;
    }

    if (!SetEnvironmentVariableW(L"PATH", newPath.c_str())) {
	    ShowErrorAndLog(logPath, L"错误", L"无法设置 PATH 环境变量");
        // 非致命错误，仍然尝试启动
    }
    else {
	    Log(logPath, L"[INFO] 成功设置 PATH 环境变量");
    }

    // 启动程序
    std::wstring maker = dir + L"\\MakerFrame.exe";
    Log(logPath, L"[INFO] 将启动: " + maker);

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
	    ShowErrorAndLog(logPath, L"错误", L"无法启动 MakerFrame.exe");
        Log(logPath, L"[INFO] 启动器异常退出。");
        return 1;
    }

	// 输出程序 PID
    Log(logPath, L"[INFO] 成功启动 MakerFrame.exe. PID=" + std::to_wstring(static_cast<long long>(pi.dwProcessId)));

	// 关闭句柄并退出启动器
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    Log(logPath, L"[INFO] 启动器退出。");
    return 0;
}
