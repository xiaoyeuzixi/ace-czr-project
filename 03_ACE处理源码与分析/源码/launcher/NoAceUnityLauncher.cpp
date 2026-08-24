#define UNICODE
#define _UNICODE
#include <windows.h>
#include <string>
#include <vector>
#include <fstream>

static std::wstring QuoteArg(const std::wstring& value) {
    std::wstring result = L"\"";
    for (wchar_t c : value) {
        if (c == L'\"') result += L"\\\"";
        else result += c;
    }
    result += L"\"";
    return result;
}

static void AppendArg(std::wstring& command, const std::wstring& value) {
    if (!command.empty()) command += L" ";
    command += QuoteArg(value);
}

static std::wstring GetErrorText(DWORD error) {
    wchar_t* buffer = nullptr;
    DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring result = length && buffer ? std::wstring(buffer, length) : L"unknown error";
    if (buffer) LocalFree(buffer);
    return result;
}

static std::wstring ModuleDirectory() {
    wchar_t buffer[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(nullptr, buffer, ARRAYSIZE(buffer));
    if (!length || length >= ARRAYSIZE(buffer)) return L".";
    std::wstring path(buffer, length);
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

static bool IsFile(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring ResolveGameRoot() {
    wchar_t buffer[MAX_PATH] = {};
    DWORD length = GetEnvironmentVariableW(L"PRETERNATURAL_GAME_ROOT", buffer, ARRAYSIZE(buffer));
    if (length && length < ARRAYSIZE(buffer) && IsFile(std::wstring(buffer, length) + L"\\UnityPlayer.dll")) {
        return std::wstring(buffer, length);
    }
    const wchar_t* candidates[] = {
        L"C:\\Program Files (x86)\\preternatural",
        L"C:\\Program Files\\preternatural"
    };
    for (const wchar_t* candidate : candidates) {
        if (IsFile(std::wstring(candidate) + L"\\UnityPlayer.dll")) return candidate;
    }
    return L"";
}

static std::wstring FindDataDirectory(const std::wstring& gameRoot) {
    std::wstring pattern = gameRoot + L"\\*_Data";
    WIN32_FIND_DATAW data = {};
    HANDLE handle = FindFirstFileW(pattern.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) return L"";
    std::wstring result;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            wcscmp(data.cFileName, L".") != 0 && wcscmp(data.cFileName, L"..") != 0) {
            result = gameRoot + L"\\" + data.cFileName;
            break;
        }
    } while (FindNextFileW(handle, &data));
    FindClose(handle);
    return result;
}

static std::wstring Join(const std::wstring& left, const wchar_t* right) {
    return left + L"\\" + right;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR commandLine, int showCommand) {
    std::wstring packageDir = ModuleDirectory();
    std::wstring logDir = Join(packageDir, L"logs");
    CreateDirectoryW(logDir.c_str(), nullptr);
    std::wofstream bootLog(Join(logDir, L"launcher_boot.log"), std::ios::out | std::ios::trunc);
    bootLog << L"portable launcher start\n";

    std::wstring gameRoot = ResolveGameRoot();
    if (gameRoot.empty()) {
        MessageBoxW(nullptr, L"UnityPlayer.dll was not found. Set PRETERNATURAL_GAME_ROOT.", L"NoACE launcher", MB_ICONERROR);
        return 100;
    }
    std::wstring dataDir = FindDataDirectory(gameRoot);
    if (dataDir.empty()) {
        MessageBoxW(nullptr, L"Unity *_Data directory was not found.", L"NoACE launcher", MB_ICONERROR);
        return 101;
    }
    SetCurrentDirectoryW(gameRoot.c_str());
    SetDllDirectoryW(gameRoot.c_str());

    const wchar_t* stubNames[] = {
        L"ACE-Base64.dll", L"ACE-SDK.dll", L"ACE-TP.dll", L"tersafe.dll", L"tersafe2.dll",
        L"TP2.dll", L"tp2_stub.dll", L"TPHelper.dll", L"tsssdk.dll", L"tss_sdk.dll",
        L"TenProtect.dll", L"TenProtect64.dll", L"TPShell64.dll"
    };
    std::wstring stubDir = Join(packageDir, L"AntiCheatExpert");
    for (const wchar_t* name : stubNames) {
        std::wstring path = Join(stubDir, name);
        HMODULE module = LoadLibraryW(path.c_str());
        bootLog << path << L" => " << reinterpret_cast<void*>(module) << L" err=" << GetLastError() << L"\n";
        if (!module) {
            MessageBoxW(nullptr, (L"Stub load failed: " + path + L"\n" + GetErrorText(GetLastError())).c_str(), L"NoACE launcher", MB_ICONERROR);
            return 102;
        }
    }
    bootLog.flush();

    HMODULE unity = LoadLibraryW(Join(gameRoot, L"UnityPlayer.dll").c_str());
    if (!unity) {
        MessageBoxW(nullptr, (L"UnityPlayer.dll load failed: " + GetErrorText(GetLastError())).c_str(), L"NoACE launcher", MB_ICONERROR);
        return 103;
    }
    using UnityMain = int (WINAPI *)(HINSTANCE, HINSTANCE, LPWSTR, int);
    UnityMain entry = reinterpret_cast<UnityMain>(GetProcAddress(unity, "UnityMain"));
    if (!entry) {
        MessageBoxW(nullptr, (L"UnityMain export missing: " + GetErrorText(GetLastError())).c_str(), L"NoACE launcher", MB_ICONERROR);
        return 104;
    }

    std::wstring args;
    AppendArg(args, L"-dataFolder");
    AppendArg(args, dataDir);
    if (commandLine && *commandLine) {
        args += L" ";
        args += commandLine;
    }
    std::vector<wchar_t> mutableArgs(args.begin(), args.end());
    mutableArgs.push_back(L'\0');
    bootLog << L"gameRoot=" << gameRoot << L" dataDir=" << dataDir << L"\n";
    bootLog.flush();
    // Unity derives the application folder from the HINSTANCE passed to
    // UnityMain. Passing the launcher module makes it search beside this
    // package for NoAceUnityLauncher_Data. Use the real game executable as
    // the identity module while keeping the launcher process and DLL stubs.
    HMODULE gameIdentity = LoadLibraryExW(
        Join(gameRoot, L"preternatural.exe").c_str(), nullptr,
        DONT_RESOLVE_DLL_REFERENCES);
    HINSTANCE unityInstance = gameIdentity ? reinterpret_cast<HINSTANCE>(gameIdentity) : instance;
    bootLog << L"unityIdentity=" << reinterpret_cast<void*>(unityInstance)
            << L" fallback=" << (gameIdentity ? 0 : 1) << L"\n";
    bootLog.flush();
    int result = entry(unityInstance, previous, mutableArgs.data(), showCommand);
    if (gameIdentity) FreeLibrary(gameIdentity);
    return result;
}
