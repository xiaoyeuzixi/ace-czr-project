#define UNICODE
#define _UNICODE
#include <windows.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <cstdint>

static std::wofstream g_log;
static std::mutex g_mu;
static bool g_patching=false;

typedef HMODULE (WINAPI *LoadLibraryW_t)(LPCWSTR);
typedef HMODULE (WINAPI *LoadLibraryA_t)(LPCSTR);
typedef HMODULE (WINAPI *LoadLibraryExW_t)(LPCWSTR,HANDLE,DWORD);
typedef HMODULE (WINAPI *LoadLibraryExA_t)(LPCSTR,HANDLE,DWORD);
typedef FARPROC (WINAPI *GetProcAddress_t)(HMODULE,LPCSTR);
static LoadLibraryW_t RealLoadLibraryW=nullptr;
static LoadLibraryA_t RealLoadLibraryA=nullptr;
static LoadLibraryExW_t RealLoadLibraryExW=nullptr;
static LoadLibraryExA_t RealLoadLibraryExA=nullptr;
static GetProcAddress_t RealGetProcAddress=nullptr;

static void logw(const std::wstring& s){ std::lock_guard<std::mutex> lk(g_mu); if(g_log.is_open()){ SYSTEMTIME st; GetLocalTime(&st); g_log<<L"["<<st.wHour<<L":"<<st.wMinute<<L":"<<st.wSecond<<L"."<<st.wMilliseconds<<L"] "<<s<<std::endl; g_log.flush(); }}
static std::wstring a2w(const char* s){ if(!s) return L"<null>"; int n=MultiByteToWideChar(CP_UTF8,0,s,-1,nullptr,0); if(n<=1) n=MultiByteToWideChar(CP_ACP,0,s,-1,nullptr,0); std::wstring w(n? n-1:0,L'\0'); if(n>1){ if(!MultiByteToWideChar(CP_UTF8,0,s,-1,w.data(),n)) MultiByteToWideChar(CP_ACP,0,s,-1,w.data(),n);} return w; }

static bool protect(void* p, SIZE_T n, DWORD prot, DWORD* oldp){ return VirtualProtect(p,n,prot,oldp)!=0; }
static void PatchModuleIAT(HMODULE mod);
static void PatchAllModules(){ if(g_patching) return; g_patching=true; HMODULE mods[2048]; DWORD cb=0; if(EnumProcessModules(GetCurrentProcess(),mods,sizeof(mods),&cb)){ unsigned count=cb/sizeof(HMODULE); for(unsigned i=0;i<count;i++) PatchModuleIAT(mods[i]); } g_patching=false; }

extern "C" HMODULE WINAPI Hook_LoadLibraryW(LPCWSTR);
extern "C" HMODULE WINAPI Hook_LoadLibraryA(LPCSTR);
extern "C" HMODULE WINAPI Hook_LoadLibraryExW(LPCWSTR,HANDLE,DWORD);
extern "C" HMODULE WINAPI Hook_LoadLibraryExA(LPCSTR,HANDLE,DWORD);
extern "C" FARPROC WINAPI Hook_GetProcAddress(HMODULE,LPCSTR);
static void* HookFor(const char* name){
    if(!lstrcmpiA(name,"LoadLibraryW")) return (void*)&Hook_LoadLibraryW;
    if(!lstrcmpiA(name,"LoadLibraryA")) return (void*)&Hook_LoadLibraryA;
    if(!lstrcmpiA(name,"LoadLibraryExW")) return (void*)&Hook_LoadLibraryExW;
    if(!lstrcmpiA(name,"LoadLibraryExA")) return (void*)&Hook_LoadLibraryExA;
    if(!lstrcmpiA(name,"GetProcAddress")) return (void*)&Hook_GetProcAddress;
    return nullptr;
}

static void PatchModuleIAT(HMODULE mod){
    if(!mod) return;
    auto base=(uint8_t*)mod;
    auto dos=(IMAGE_DOS_HEADER*)base;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE) return;
    auto nt=(IMAGE_NT_HEADERS64*)(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE) return;
    auto dir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if(!dir.VirtualAddress || !dir.Size) return;
    auto imp=(IMAGE_IMPORT_DESCRIPTOR*)(base+dir.VirtualAddress);
    for(; imp->Name; imp++){
        const char* dll=(const char*)(base+imp->Name);
        if(lstrcmpiA(dll,"KERNEL32.dll") && lstrcmpiA(dll,"KERNELBASE.dll")) continue;
        auto thunk=(IMAGE_THUNK_DATA64*)(base+imp->OriginalFirstThunk);
        auto iat=(IMAGE_THUNK_DATA64*)(base+imp->FirstThunk);
        if(!thunk) continue;
        for(; thunk->u1.AddressOfData; thunk++,iat++){
            if(thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) continue;
            auto ibn=(IMAGE_IMPORT_BY_NAME*)(base+thunk->u1.AddressOfData);
            void* hook=HookFor((const char*)ibn->Name);
            if(!hook) continue;
            if((void*)iat->u1.Function==hook) continue;
            DWORD old=0;
            if(protect(&iat->u1.Function,sizeof(void*),PAGE_READWRITE,&old)){
                iat->u1.Function=(ULONGLONG)hook;
                DWORD tmp; VirtualProtect(&iat->u1.Function,sizeof(void*),old,&tmp);
            }
        }
    }
}

extern "C" HMODULE WINAPI Hook_LoadLibraryW(LPCWSTR lp){ logw(L"LoadLibraryW req: "+std::wstring(lp?lp:L"<null>")); HMODULE h=RealLoadLibraryW(lp); DWORD e=GetLastError(); logw(L"LoadLibraryW ret: "+std::to_wstring((uintptr_t)h)+L" err="+std::to_wstring(e)); if(h) PatchAllModules(); SetLastError(e); return h; }
extern "C" HMODULE WINAPI Hook_LoadLibraryA(LPCSTR lp){ logw(L"LoadLibraryA req: "+a2w(lp)); HMODULE h=RealLoadLibraryA(lp); DWORD e=GetLastError(); logw(L"LoadLibraryA ret: "+std::to_wstring((uintptr_t)h)+L" err="+std::to_wstring(e)); if(h) PatchAllModules(); SetLastError(e); return h; }
extern "C" HMODULE WINAPI Hook_LoadLibraryExW(LPCWSTR lp,HANDLE hf,DWORD fl){ logw(L"LoadLibraryExW req: "+std::wstring(lp?lp:L"<null>")+L" flags="+std::to_wstring(fl)); HMODULE h=RealLoadLibraryExW(lp,hf,fl); DWORD e=GetLastError(); logw(L"LoadLibraryExW ret: "+std::to_wstring((uintptr_t)h)+L" err="+std::to_wstring(e)); if(h) PatchAllModules(); SetLastError(e); return h; }
extern "C" HMODULE WINAPI Hook_LoadLibraryExA(LPCSTR lp,HANDLE hf,DWORD fl){ logw(L"LoadLibraryExA req: "+a2w(lp)+L" flags="+std::to_wstring(fl)); HMODULE h=RealLoadLibraryExA(lp,hf,fl); DWORD e=GetLastError(); logw(L"LoadLibraryExA ret: "+std::to_wstring((uintptr_t)h)+L" err="+std::to_wstring(e)); if(h) PatchAllModules(); SetLastError(e); return h; }
extern "C" FARPROC WINAPI Hook_GetProcAddress(HMODULE m,LPCSTR n){ std::wstring ns; if(((uintptr_t)n>>16)==0) ns=L"#"+std::to_wstring((uintptr_t)n); else ns=a2w(n); FARPROC p=RealGetProcAddress(m,n); logw(L"GetProcAddress "+std::to_wstring((uintptr_t)m)+L" "+ns+L" -> "+std::to_wstring((uintptr_t)p)); return p; }

static std::wstring QuoteArg(const std::wstring& s){ std::wstring o=L"\""; for(wchar_t c:s){ if(c==L'\"') o+=L"\\\""; else o+=c; } o+=L"\""; return o; }
static void AppendArg(std::wstring& cmd,const std::wstring& arg){ if(!cmd.empty()) cmd+=L" "; cmd+=QuoteArg(arg); }

int WINAPI wWinMain(HINSTANCE hInst,HINSTANCE hPrev,LPWSTR lpCmdLine,int nShow){
    const wchar_t* gameRoot=L"C:\\Program Files (x86)\\preternatural";
    const wchar_t* dataDir=L"C:\\Program Files (x86)\\preternatural\\超自然行动组_Data";
    CreateDirectoryW(L"D:\\vs\\ACE boli\\trace_launcher",nullptr);
    g_log.open(L"D:\\vs\\ACE boli\\trace_launcher\\load_trace.log", std::ios::out|std::ios::trunc);
    HMODULE k=GetModuleHandleW(L"kernel32.dll");
    RealLoadLibraryW=(LoadLibraryW_t)::GetProcAddress(k,"LoadLibraryW");
    RealLoadLibraryA=(LoadLibraryA_t)::GetProcAddress(k,"LoadLibraryA");
    RealLoadLibraryExW=(LoadLibraryExW_t)::GetProcAddress(k,"LoadLibraryExW");
    RealLoadLibraryExA=(LoadLibraryExA_t)::GetProcAddress(k,"LoadLibraryExA");
    RealGetProcAddress=(GetProcAddress_t)::GetProcAddress(k,"GetProcAddress");
    SetCurrentDirectoryW(gameRoot); SetDllDirectoryW(gameRoot);
    logw(L"trace launcher start"); PatchAllModules();
    HMODULE unity=RealLoadLibraryW(L"C:\\Program Files (x86)\\preternatural\\UnityPlayer.dll");
    if(!unity){ logw(L"Unity load failed"); return 100; }
    PatchAllModules();
    typedef int (WINAPI *UnityMain_t)(HINSTANCE,HINSTANCE,LPWSTR,int);
    UnityMain_t UnityMain=(UnityMain_t)RealGetProcAddress(unity,"UnityMain");
    if(!UnityMain){ logw(L"UnityMain missing"); return 101; }
    std::wstring cmd; AppendArg(cmd,L"-dataFolder"); AppendArg(cmd,dataDir); if(lpCmdLine&&*lpCmdLine){ cmd+=L" "; cmd+=lpCmdLine; }
    std::vector<wchar_t> mut(cmd.begin(),cmd.end()); mut.push_back(0);
    int r=UnityMain(hInst,hPrev,mut.data(),nShow); logw(L"UnityMain returned "+std::to_wstring(r)); return r;
}

