#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windowsx.h>
#undef GetWindowFont // because imgui

#include <wrl/client.h>

#include <string>
#include <thread>

// "optimized" versions that reuse the storage of an existing string
void WideFromMultiByte(const char* s, std::wstring& dst);
void WideFromMultiByte(const std::string& s, std::wstring& dst);
void MultiByteFromWide(const wchar_t* s, std::string& dst);
void MultiByteFromWide(const std::wstring& s, std::string& dst);

std::wstring WideFromMultiByte(const char* s);
std::wstring WideFromMultiByte(const std::string& s);
std::string MultiByteFromWide(const wchar_t* s);
std::string MultiByteFromWide(const std::wstring& s);

std::string MultiByteFromHR(HRESULT hr);

bool detail_WinAssert(bool okay, const char* error, const char* expr, const char* file, const char* function, int line);
bool detail_CheckHR(HRESULT hr, const char* expr, const char* file, const char* function, int line);
bool detail_CheckWin32(bool okay, const char* expr, const char* file, const char* function, int line);

// If the expression is false, asserts on the expression.
#define WINASSERT(expr) detail_WinAssert((expr), 0, #expr, __FILE__, __FUNCSIG__, __LINE__)
// If the HRESULT is an error, reports it.
#define CHECKHR(hr_expr) detail_CheckHR((hr_expr), #hr_expr, __FILE__, __FUNCSIG__, __LINE__)
// If the expression is false, reports the GetLastError().
#define CHECKWIN32(bool_expr) detail_CheckWin32((bool_expr), #bool_expr, __FILE__, __FUNCSIG__, __LINE__)

// From https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
void SetThreadName(DWORD dwThreadID, const char* threadName);
void SetThreadName(std::thread& th, const char* threadName);