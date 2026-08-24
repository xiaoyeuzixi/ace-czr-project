#include <windows.h>
#include <stdint.h>
#include <stdio.h>

static void logline(const char* s) {
    FILE* f=nullptr;
    fopen_s(&f, "D:\\vs\\ACE boli\\noace_launcher\\tp2_stub.log", "ab");
    if(f){ SYSTEMTIME st; GetLocalTime(&st); fprintf(f,"[%02u:%02u:%02u.%03u] %s\n",st.wHour,st.wMinute,st.wSecond,st.wMilliseconds,s); fclose(f); }
}

extern "C" __declspec(dllexport) int __cdecl tp2_sdk_init_ex(const char* appKey, const char* dataPath, const char* apkPath, int useDataDir) { logline("tp2_sdk_init_ex"); return 0; }
extern "C" __declspec(dllexport) int __cdecl tp2_setuserinfo(int accountType, const char* worldId, const char* openId) { logline("tp2_setuserinfo"); return 0; }
extern "C" __declspec(dllexport) int __cdecl tp2_setoptions(int opt, const char* val) { logline("tp2_setoptions"); return 0; }
extern "C" __declspec(dllexport) int __cdecl tp2_sdk_ioctl(int req, void* inbuf, int inlen, void* outbuf, int* outlen) { logline("tp2_sdk_ioctl"); if(outlen) *outlen=0; return 0; }
extern "C" __declspec(dllexport) void __cdecl tp2_free_anti_data(void* p) { logline("tp2_free_anti_data"); }
extern "C" __declspec(dllexport) int __cdecl tss_get_report_data(void** data, int* data_len) { logline("tss_get_report_data"); if(data) *data=nullptr; if(data_len) *data_len=0; return 0; }
extern "C" __declspec(dllexport) int __cdecl tss_del_report_data(void* data) { logline("tss_del_report_data"); return 0; }
extern "C" __declspec(dllexport) int __cdecl tss_get_report_data2(void** data, int* data_len) { logline("tss_get_report_data2"); if(data) *data=nullptr; if(data_len) *data_len=0; return 0; }
extern "C" __declspec(dllexport) int __cdecl tss_get_report_data4(void** data, int* data_len) { logline("tss_get_report_data4"); if(data) *data=nullptr; if(data_len) *data_len=0; return 0; }
extern "C" __declspec(dllexport) int __cdecl tss_del_report_data4(void* data) { logline("tss_del_report_data4"); return 0; }
extern "C" __declspec(dllexport) int __cdecl tss_recv_sec_signature(const void* data, int data_len) { logline("tss_recv_sec_signature"); return 0; }
extern "C" __declspec(dllexport) int __cdecl TssSdkInit() { logline("TssSdkInit"); return 0; }
extern "C" __declspec(dllexport) int __cdecl TssSdkSetUserInfo() { logline("TssSdkSetUserInfo"); return 0; }
extern "C" __declspec(dllexport) int __cdecl TssSdkIoctl() { logline("TssSdkIoctl"); return 0; }
extern "C" __declspec(dllexport) int __cdecl TssSdkGetReportData() { logline("TssSdkGetReportData"); return 0; }

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){ DisableThreadLibraryCalls(h); logline("tp2 stub attach"); }
    if(reason==DLL_PROCESS_DETACH){ logline("tp2 stub detach"); }
    return TRUE;
}
