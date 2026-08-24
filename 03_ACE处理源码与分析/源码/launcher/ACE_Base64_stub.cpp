#include <windows.h>
#include <stdint.h>
#include <stdio.h>

static void logmsg(const char* s) {
    FILE* f = nullptr;
    fopen_s(&f, "D:\\vs\\ACE boli\\noace_launcher\\ace_stub.log", "ab");
    if (f) { SYSTEMTIME st; GetLocalTime(&st); fprintf(f, "[%02u:%02u:%02u.%03u] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, s); fclose(f); }
}

static bool is_writable(void* p, size_t need) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    uintptr_t start=(uintptr_t)p, end=start+need, rstart=(uintptr_t)mbi.BaseAddress, rend=rstart+mbi.RegionSize;
    if (start < rstart || end > rend) return false;
    DWORD prot = mbi.Protect & 0xff;
    return prot==PAGE_READWRITE || prot==PAGE_WRITECOPY || prot==PAGE_EXECUTE_READWRITE || prot==PAGE_EXECUTE_WRITECOPY;
}

extern "C" __declspec(dllexport) int __cdecl AceStubNoop() { logmsg("AceStubNoop"); return 0; }
extern "C" __declspec(dllexport) int __cdecl AceStubNoop1(void*) { logmsg("AceStubNoop1"); return 0; }
extern "C" __declspec(dllexport) int __cdecl AceStubNoop2(void*, void*) { logmsg("AceStubNoop2"); return 0; }
extern "C" __declspec(dllexport) int __cdecl AceStubNoop3(void*, void*, void*) { logmsg("AceStubNoop3"); return 0; }
extern "C" __declspec(dllexport) int __cdecl AceStubNoop4(void*, void*, void*, void*) { logmsg("AceStubNoop4"); return 0; }

static int init_common(void* client_obj_ptr, const char* which) {
    logmsg(which);
    if (is_writable(client_obj_ptr, sizeof(uintptr_t) * 10)) {
        uintptr_t* p = (uintptr_t*)client_obj_ptr;
        p[0] = 0; // handle_
        p[1] = 0; // _deprecated1
        p[2] = (uintptr_t)&AceStubNoop;  // log_out_
        p[3] = (uintptr_t)&AceStubNoop;  // deinit_
        p[4] = (uintptr_t)&AceStubNoop4; // get_packet_
        p[5] = (uintptr_t)&AceStubNoop3; // on_packet_received_
        p[6] = (uintptr_t)&AceStubNoop4; // log_in_
        p[7] = (uintptr_t)&AceStubNoop4; // get_light_feature_packet_
        p[8] = (uintptr_t)&AceStubNoop3; // on_light_feature_packet_received_
        p[9] = 0;
        logmsg("client object filled");
    } else {
        logmsg("client object not writable or absent");
    }
    return 0; // ACE_SDK_RESULT_SUCCESS
}

extern "C" __declspec(dllexport) int __cdecl InitAceClient(void* p)  { return init_common(p, "InitAceClient"); }
extern "C" __declspec(dllexport) int __cdecl InitAceClient0(void* p) { return init_common(p, "InitAceClient0"); }
extern "C" __declspec(dllexport) int __cdecl InitAceClient2(void* p) { return init_common(p, "InitAceClient2"); }
extern "C" __declspec(dllexport) int __cdecl InitAceClient3(void* p) { return init_common(p, "InitAceClient3"); }
extern "C" __declspec(dllexport) int __cdecl InitAceClient4(void* p) { return init_common(p, "InitAceClient4"); }
extern "C" __declspec(dllexport) int __cdecl InitAceClient5(void* p) { return init_common(p, "InitAceClient5"); }
extern "C" __declspec(dllexport) int __cdecl NullExportFunction() { logmsg("NullExportFunction"); return 0; }

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID) {
    if (r == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); logmsg("ACE-Base64 stub attach"); }
    if (r == DLL_PROCESS_DETACH) { logmsg("ACE-Base64 stub detach"); }
    return TRUE;
}
