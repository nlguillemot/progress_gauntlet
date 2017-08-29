#include "winutil.h"

#include <comdef.h>
#include <set>
#include <tuple>
#include <memory>
#include <mutex>

// allows you to ignore an assert then not be bothered by it in the future
static std::set<std::tuple<std::string, std::string, int>> g_IgnoredAsserts;
static std::mutex g_IgnoredAssertsMutex;

void WideFromMultiByte(const char* s, std::wstring& dst)
{
    int bufSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
    CHECKWIN32(bufSize != 0);

    dst.resize(bufSize, 0);
    CHECKWIN32(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, &dst[0], bufSize) != 0);
    dst.pop_back(); // remove null terminator
}

void WideFromMultiByte(const std::string& s, std::wstring& dst)
{
    WideFromMultiByte(s.c_str(), dst);
}

void MultiByteFromWide(const wchar_t* ws, std::string& dst)
{
    int bufSize = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws, -1, NULL, 0, NULL, NULL);
    CHECKWIN32(bufSize != 0);

    dst.resize(bufSize, 0);
    CHECKWIN32(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws, -1, &dst[0], bufSize, NULL, NULL) != 0);
    dst.pop_back(); // remove null terminator
}

void MultiByteFromWide(const std::wstring& ws, std::string& dst)
{
    MultiByteFromWide(ws.c_str(), dst);
}

std::wstring WideFromMultiByte(const char* s)
{
    std::wstring ws;
    WideFromMultiByte(s, ws);
    return ws;
}

std::wstring WideFromMultiByte(const std::string& s)
{
    return WideFromMultiByte(s.c_str());
}

std::string MultiByteFromWide(const wchar_t* ws)
{
    std::string s;
    MultiByteFromWide(ws, s);
    return s;
}

std::string MultiByteFromWide(const std::wstring& ws)
{
    return MultiByteFromWide(ws.c_str());
}

std::string MultiByteFromHR(HRESULT hr)
{
    _com_error err(hr);
    return MultiByteFromWide(err.ErrorMessage());
}

bool detail_WinAssert(bool okay, const char* error, const char* expr, const char* file, const char* function, int line)
{
    if (okay)
    {
        return true;
    }

    std::lock_guard<std::mutex> lock(g_IgnoredAssertsMutex);

    if (g_IgnoredAsserts.find(std::make_tuple(file, function, line)) != g_IgnoredAsserts.end())
    {
        return false;
    }

    std::wstring werror = WideFromMultiByte(error ? error : "Assertion failed");
    std::wstring wexpr = WideFromMultiByte(expr);
    std::wstring wfile = WideFromMultiByte(file);
    std::wstring wfunction = WideFromMultiByte(function);

    std::wstring msg = std::wstring() +
        L"Expr: " + wexpr + L"\n" +
        L"File: " + wfile + L"\n" +
        L"Function: " + wfunction + L"\n" +
        L"Line: " + std::to_wstring(line) + L"\n" +
        L"ErrorMessage: " + werror + L"\n";

    int result = MessageBoxW(NULL, msg.c_str(), L"Error", MB_ABORTRETRYIGNORE);
    if (result == IDABORT)
    {
        ExitProcess(-1);
    }
    else if (result == IDRETRY)
    {
        DebugBreak();
    }
    else if (result == IDIGNORE)
    {
        g_IgnoredAsserts.insert(std::make_tuple(file, function, line));
    }

    return false;
}

bool detail_CheckHR(HRESULT hr, const char* expr, const char* file, const char* function, int line)
{
    if (SUCCEEDED(hr))
    {
        return true;
    }

    _com_error err(hr);
    std::string mberr = MultiByteFromWide(err.ErrorMessage());

    detail_WinAssert(false, mberr.c_str(), expr, file, function, line);

    return false;
}

bool detail_CheckWin32(bool okay, const char* expr, const char* file, const char* function, int line)
{
    if (okay)
    {
        return true;
    }

    return detail_CheckHR(HRESULT_FROM_WIN32(GetLastError()), expr, file, function, line);
}

// From https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
//  
// Usage: SetThreadName ((DWORD)-1, "MainThread");  
//  
const DWORD MS_VC_EXCEPTION = 0x406D1388;  
#pragma pack(push,8)  
typedef struct tagTHREADNAME_INFO  
{  
    DWORD dwType; // Must be 0x1000.  
    LPCSTR szName; // Pointer to name (in user addr space).  
    DWORD dwThreadID; // Thread ID (-1=caller thread).  
    DWORD dwFlags; // Reserved for future use, must be zero.  
 } THREADNAME_INFO;  
#pragma pack(pop)  
void SetThreadName(DWORD dwThreadID, const char* threadName) {  
    THREADNAME_INFO info;  
    info.dwType = 0x1000;  
    info.szName = threadName;  
    info.dwThreadID = dwThreadID;  
    info.dwFlags = 0;  
#pragma warning(push)  
#pragma warning(disable: 6320 6322)  
    __try{  
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);  
    }  
    __except (EXCEPTION_EXECUTE_HANDLER){  
    }  
#pragma warning(pop)  
}  

void SetThreadName(std::thread& th, const char* threadName)
{
    SetThreadName(GetThreadId(static_cast<HANDLE>(th.native_handle())), threadName);
}