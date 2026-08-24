#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Combined ACE/TSS/TP2 research stub v2
// Fix target: the previous generic callbacks returned OK but never produced any ACE packet data.
// The game receives ACE challenge packets through on_packet_received_ and polls get_packet_.
// v2 records incoming packets and returns bounded synthetic response packets so the managed side
// no longer sees a permanently empty ACE data stream.

static CRITICAL_SECTION g_cs;
static bool g_cs_ready = false;
static uintptr_t g_client_obj[16] = {0};
static unsigned long g_get_packet_count = 0;
static unsigned long g_on_packet_count = 0;
static unsigned long g_get_light_count = 0;
static unsigned long g_on_light_count = 0;
static unsigned long g_ioctl_count = 0;
static HMODULE g_module = nullptr;
static char g_log_path[MAX_PATH] = "ace_stub.log";

struct PacketSlot {
    uint8_t data[2048];
    int len;
};
static PacketSlot g_queue[32];
static PacketSlot g_return_slot;
static int g_q_head = 0;
static int g_q_tail = 0;
static uint32_t g_seq = 1;
static uint8_t g_report_buf[512];
static DWORD g_last_heartbeat_ms = 0;
static unsigned long g_heartbeat_count = 0;

static void lock_cs(){ if(g_cs_ready) EnterCriticalSection(&g_cs); }
static void unlock_cs(){ if(g_cs_ready) LeaveCriticalSection(&g_cs); }

static void logline(const char* s) {
    FILE* f=nullptr;
    fopen_s(&f, g_log_path, "ab");
    if(f){
        SYSTEMTIME st; GetLocalTime(&st);
        fprintf(f,"[%02u:%02u:%02u.%03u] %s\n",st.wHour,st.wMinute,st.wSecond,st.wMilliseconds,s);
        fclose(f);
    }
}

static void init_log_path(HMODULE module) {
    char path[MAX_PATH] = {};
    DWORD length = GetModuleFileNameA(module, path, sizeof(path));
    if (!length || length >= sizeof(path)) return;
    char* slash = strrchr(path, '\\');
    if (!slash) return;
    *slash = 0;
    slash = strrchr(path, '\\');
    if (!slash) return;
    slash[1] = 0;
    strcat_s(path, sizeof(path), "logs");
    CreateDirectoryA(path, nullptr);
    strcat_s(path, sizeof(path), "\\ace_stub.log");
    strcpy_s(g_log_path, sizeof(g_log_path), path);
}

static bool region_has(void* p, size_t n, DWORD wantWrite){
    if(!p || n==0) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if(!VirtualQuery(p,&mbi,sizeof(mbi))) return false;
    uintptr_t s=(uintptr_t)p,e=s+n,bs=(uintptr_t)mbi.BaseAddress,be=bs+mbi.RegionSize;
    if(s<bs||e>be) return false;
    if(mbi.State!=MEM_COMMIT || (mbi.Protect & PAGE_GUARD) || (mbi.Protect & PAGE_NOACCESS)) return false;
    DWORD prot=mbi.Protect&0xff;
    if(wantWrite){
        return prot==PAGE_READWRITE||prot==PAGE_WRITECOPY||prot==PAGE_EXECUTE_READWRITE||prot==PAGE_EXECUTE_WRITECOPY;
    }
    return prot==PAGE_READONLY||prot==PAGE_READWRITE||prot==PAGE_WRITECOPY||prot==PAGE_EXECUTE_READ||prot==PAGE_EXECUTE_READWRITE||prot==PAGE_EXECUTE_WRITECOPY;
}
static bool readable(void* p, size_t n){ return region_has(p,n,0); }
static bool writable(void* p, size_t n){ return region_has(p,n,1); }

static void hex_preview(char* out, size_t outsz, const void* p, int n){
    if(!out || outsz==0) return;
    out[0]=0;
    if(!p || n<=0 || !readable((void*)p, (size_t)((n<32)?n:32))){ sprintf_s(out,outsz,"<unreadable>"); return; }
    const uint8_t* b=(const uint8_t*)p;
    int lim=n<32?n:32;
    size_t pos=0;
    for(int i=0;i<lim && pos+4<outsz;i++) pos += sprintf_s(out+pos,outsz-pos,"%02X",b[i]);
    if(n>lim && pos+4<outsz) sprintf_s(out+pos,outsz-pos,"...");
}

static void make_report(uint8_t* out, int* outLen, const uint8_t* in, int inLen, const char* tag){
    uint32_t seq = g_seq++;
    const char* magic = "ACE_STUB_V2";
    int pos=0;
    memcpy(out+pos, magic, 11); pos+=11;
    out[pos++] = 0;
    memcpy(out+pos, &seq, 4); pos+=4;
    uint32_t ilen = (uint32_t)((in && inLen>0) ? inLen : 0);
    memcpy(out+pos, &ilen, 4); pos+=4;
    uint32_t h=2166136261u;
    if(in && inLen>0){
        int lim = inLen > 1536 ? 1536 : inLen;
        for(int i=0;i<lim;i++){ h ^= in[i]; h *= 16777619u; }
    }
    memcpy(out+pos, &h, 4); pos+=4;
    int tagLen=(int)strlen(tag);
    if(pos+tagLen+1 < 2048){ memcpy(out+pos, tag, tagLen); pos+=tagLen; out[pos++]=0; }
    if(in && inLen>0){
        int copy = inLen;
        if(copy > 2048-pos) copy = 2048-pos;
        memcpy(out+pos, in, copy); pos += copy;
    }
    *outLen = pos;
}

static void enqueue_packet(const uint8_t* data, int len, const char* reason){
    if(!data || len<=0) return;
    PacketSlot tmp{};
    tmp.len = len > (int)sizeof(tmp.data) ? (int)sizeof(tmp.data) : len;
    memcpy(tmp.data, data, tmp.len);
    lock_cs();
    int next=(g_q_tail+1)%32;
    if(next==g_q_head) g_q_head=(g_q_head+1)%32;
    g_queue[g_q_tail]=tmp;
    g_q_tail=next;
    unlock_cs();
    char buf[160]; sprintf_s(buf,"enqueue_packet reason=%s in_len=%d out_len=%d",reason,len,tmp.len); logline(buf);
}

static int dequeue_packet(void* out, uintptr_t maxLen, const char* name){
    if(!out || maxLen==0 || !writable(out,1)) return 0;
    PacketSlot tmp{}; bool ok=false;
    lock_cs();
    if(g_q_head!=g_q_tail){ tmp=g_queue[g_q_head]; g_q_head=(g_q_head+1)%32; ok=true; }
    unlock_cs();
    if(!ok) return 0;
    int n=tmp.len;
    if((uintptr_t)n > maxLen) n=(int)maxLen;
    if(!writable(out,(size_t)n)) return 0;
    memcpy(out,tmp.data,n);
    char hex[96]; hex_preview(hex,sizeof(hex),out,n);
    char buf[256]; sprintf_s(buf,"%s return_len=%d max=%llu preview=%s",name,n,(unsigned long long)maxLen,hex); logline(buf);
    return n;
}

extern "C" __declspec(dllexport) int __cdecl AceStubNoop(){ logline("AceStubNoop"); return 0; }
extern "C" __declspec(dllexport) int __cdecl AceStubNoop1(void* a){ char buf[160]; sprintf_s(buf,"AceStubNoop1 args=%p writable=%d",a,writable(a,sizeof(uintptr_t))?1:0); logline(buf); return 0; }
extern "C" __declspec(dllexport) int __cdecl AceStubNoop2(void* a,void* b){ char buf[220]; sprintf_s(buf,"AceStubNoop2 args=%p %p writable=%d/%d",a,b,writable(a,sizeof(uintptr_t))?1:0,writable(b,sizeof(uintptr_t))?1:0); logline(buf); return 0; }
extern "C" __declspec(dllexport) int __cdecl AceStubNoop3(void* a,void* b,void* c){ char buf[220]; sprintf_s(buf,"AceStubNoop3 args=%p %p %p",a,b,c); logline(buf); return 0; }
extern "C" __declspec(dllexport) int __cdecl AceStubNoop4(void* a,void* b,void* c,void* d){ char buf[260]; sprintf_s(buf,"AceStubNoop4 args=%p %p %p %p",a,b,c,d); logline(buf); return 0; }

struct AceSdkClientPacketNative {
    void* buffer;
    uint32_t len;
    uint32_t reserved;
};

static int dequeue_packet_native(AceSdkClientPacketNative* packet, const char* name){
    if(!packet || !writable(packet,sizeof(*packet))) return -1;
    packet->buffer=nullptr;
    packet->len=0;
    packet->reserved=0;
    bool ok=false;
    lock_cs();
    if(g_q_head!=g_q_tail){
        g_return_slot=g_queue[g_q_head];
        g_q_head=(g_q_head+1)%32;
        ok=true;
    }
    unlock_cs();
    if(!ok) return 0;
    packet->buffer=g_return_slot.data;
    packet->len=(uint32_t)g_return_slot.len;
    char hex[96]; hex_preview(hex,sizeof(hex),packet->buffer,(int)packet->len);
    char buf[260]; sprintf_s(buf,"%s status=0 packet=%p data=%p len=%u preview=%s",name,packet,packet->buffer,packet->len,hex); logline(buf);
    return 0;
}

static int __cdecl AceGetPacket(void* client, AceSdkClientPacketNative* packet){
    unsigned long n=++g_get_packet_count;
    int r=dequeue_packet_native(packet,"get_packet_");
    if((n<=20 || (n%500)==0) && packet && packet->len==0){
        char buf[220]; sprintf_s(buf,"get_packet_#%lu status=%d empty client=%p packet=%p",n,r,client,packet); logline(buf);
    }
    return r;
}

static int __cdecl AceOnPacketReceived(void* client, void* data, uintptr_t len){
    unsigned long n=++g_on_packet_count;
    char hex[96]; hex_preview(hex,sizeof(hex),data,(int)len);
    char buf[320]; sprintf_s(buf,"on_packet_received_#%lu client=%p data=%p len=%llu readable=%d preview=%s",n,client,data,(unsigned long long)len,readable(data,(size_t)((len<32)?len:32))?1:0,hex); logline(buf);
    if(data && len>0 && len<=65536 && readable(data,(size_t)((len<2048)?len:2048))){
        int copy=(int)((len>1536)?1536:len);
        enqueue_packet((const uint8_t*)data,copy,"packet");
    }
    return 0;
}

static int __cdecl AceGetLightFeaturePacket(void* client, AceSdkClientPacketNative* packet){
    unsigned long n=++g_get_light_count;
    int r=dequeue_packet_native(packet,"get_light_feature_packet_");
    if(r==0 && (n<=20 || (n%100)==0)){
        char buf[240]; sprintf_s(buf,"get_light_feature_packet_#%lu status=0 client=%p packet=%p len=%u",n,client,packet,packet?packet->len:0); logline(buf);
    }
    return r;
}

static int __cdecl AceOnLightFeaturePacketReceived(void* client, void* data, uintptr_t len){
    unsigned long n=++g_on_light_count;
    char hex[96]; hex_preview(hex,sizeof(hex),data,(int)len);
    char buf[320]; sprintf_s(buf,"on_light_feature_packet_received_#%lu client=%p data=%p len=%llu preview=%s",n,client,data,(unsigned long long)len,hex); logline(buf);
    if(data && len>0 && len<=65536 && readable(data,(size_t)((len<2048)?len:2048))){
        int copy=(int)((len>1536)?1536:len);
        enqueue_packet((const uint8_t*)data,copy,"light");
    }
    return 0;
}

static void fill_obj(uintptr_t* p){
    // Verified managed layout for this build:
    // p[4] and p[5] were called continuously in the old working stub.
    p[0]=0; p[1]=0;
    p[2]=(uintptr_t)&AceStubNoop;       // log_out_
    p[3]=(uintptr_t)&AceStubNoop;       // deinit_
    p[4]=(uintptr_t)&AceGetPacket;      // get_packet_
    p[5]=(uintptr_t)&AceOnPacketReceived; // on_packet_received_
    p[6]=(uintptr_t)&AceStubNoop;       // log_in_
    p[7]=(uintptr_t)&AceGetLightFeaturePacket; // get_light_feature_packet_
    p[8]=(uintptr_t)&AceOnLightFeaturePacketReceived; // on_light_feature_packet_received_
    p[9]=0;
}

static void fill_obj_legacy(uintptr_t* p){
    // Older attempt used an extra leading reserved slot. Fill both layouts to tolerate the actual marshalled struct.
    p[0]=0; p[1]=0;
    p[2]=(uintptr_t)&AceStubNoop;
    p[3]=(uintptr_t)&AceStubNoop;
    p[4]=(uintptr_t)&AceGetPacket;
    p[5]=(uintptr_t)&AceOnPacketReceived;
    p[6]=(uintptr_t)&AceStubNoop;
    p[7]=(uintptr_t)&AceGetLightFeaturePacket;
    p[8]=(uintptr_t)&AceOnLightFeaturePacketReceived;
    p[9]=0;
}

static int init_common(void* obj, const char* name){
    char buf[256]; sprintf_s(buf,"%s init arg=%p static=%p", name, obj, g_client_obj); logline(buf);
    memset(g_client_obj,0,sizeof(g_client_obj));
    fill_obj(g_client_obj);
    // Seed one hello packet so the first poll after Init has non-empty data if the managed side expects it.
    const uint8_t seed[] = { 'A','C','E','_','S','T','U','B','_','H','E','L','L','O',0,2 };
    enqueue_packet(seed,sizeof(seed),"init");
    if(writable(obj,sizeof(uintptr_t)*10)){
        uintptr_t* p=(uintptr_t*)obj;
        fill_obj((uintptr_t*)obj);
        logline("AceSdkClientObject direct filled v2");
    } else if(writable(obj,sizeof(uintptr_t))){
        *(uintptr_t*)obj=(uintptr_t)g_client_obj;
        logline("AceSdkClientObject pointer filled v2");
    } else {
        logline("AceSdkClientObject not writable; using static only");
    }
    return 0;
}

extern "C" __declspec(dllexport) int __cdecl InitAceClient(void* p){ return init_common(p,"InitAceClient"); }
extern "C" __declspec(dllexport) int __cdecl InitAceClient0(void* p){ return init_common(p,"InitAceClient0"); }
extern "C" __declspec(dllexport) int __cdecl InitAceClient2(void* p){ return init_common(p,"InitAceClient2"); }
extern "C" __declspec(dllexport) int __cdecl InitAceClient3(void* p){ return init_common(p,"InitAceClient3"); }
extern "C" __declspec(dllexport) int __cdecl InitAceClient4(void* p){ return init_common(p,"InitAceClient4"); }
extern "C" __declspec(dllexport) int __cdecl InitAceClient5(uint32_t ver, void* opt, void** clientObjOut){
    char buf[320]; sprintf_s(buf,"InitAceClient5 ABI probe ver=%u opt=%p out=%p static=%p",ver,opt,clientObjOut,g_client_obj); logline(buf);
    memset(g_client_obj,0,sizeof(g_client_obj)); fill_obj(g_client_obj);
    if(clientObjOut && writable(clientObjOut,sizeof(void*))){
        *clientObjOut=(void*)g_client_obj;
        logline("InitAceClient5 wrote client object pointer");
    } else {
        logline("InitAceClient5 output pointer is not writable");
    }
    return 0;
}

extern "C" __declspec(dllexport) int __cdecl NullExportFunction(){ logline("NullExportFunction"); return 0; }
extern "C" __declspec(dllexport) int __cdecl tp2_sdk_init_ex(const char* a, const char* b, const char* c, int d){ char buf[256]; sprintf_s(buf,"tp2_sdk_init_ex a=%p b=%p c=%p d=%d",a,b,c,d); logline(buf); return 0; }
extern "C" __declspec(dllexport) int __cdecl tp2_setuserinfo(int accountType,const char* openid,const char* worldId){ char buf[256]; sprintf_s(buf,"tp2_setuserinfo accountType=%d openid=%p worldId=%p",accountType,openid,worldId); logline(buf); return 0; }
extern "C" __declspec(dllexport) int __cdecl tp2_setoptions(int k,const char* v){ char buf[192]; sprintf_s(buf,"tp2_setoptions k=%d v=%p",k,v); logline(buf); return 0; }
extern "C" __declspec(dllexport) int __cdecl tp2_sdk_ioctl(int cmd,void* inbuf,int inlen,void* outbuf,int* outlen){
    unsigned long n=++g_ioctl_count;
    char hex[96]; hex_preview(hex,sizeof(hex),inbuf,inlen);
    char buf[320]; sprintf_s(buf,"tp2_sdk_ioctl#%lu cmd=%d in=%p inlen=%d out=%p outlen=%p in_preview=%s",n,cmd,inbuf,inlen,outbuf,outlen,hex); logline(buf);
    if(outbuf && outlen && writable(outlen,sizeof(int))){
        int max=*outlen;
        if(max>0 && max<=4096 && writable(outbuf,(size_t)max)){
            uint8_t tmp[512]; int tl=0; make_report(tmp,&tl,(const uint8_t*)inbuf,(inbuf&&inlen>0&&readable(inbuf,(size_t)((inlen<256)?inlen:256)))?inlen:0,"ioctl");
            if(tl>max) tl=max; memcpy(outbuf,tmp,tl); *outlen=tl; return 0;
        }
        *outlen=0;
    }
    return 0;
}
extern "C" __declspec(dllexport) void __cdecl tp2_free_anti_data(void*){ logline("tp2_free_anti_data"); }

static int get_report_common(const char* name, void** data, int* len){
    logline(name);
    if(!data || !len || !writable(data,sizeof(void*)) || !writable(len,sizeof(int))) return 0;
    int l=0; const uint8_t seed[]={'A','C','E','_','R','E','P','O','R','T',0}; make_report(g_report_buf,&l,seed,sizeof(seed),name);
    *data=(void*)g_report_buf; *len=l;
    char buf[160]; sprintf_s(buf,"%s -> data=%p len=%d",name,g_report_buf,l); logline(buf);
    return 0;
}
extern "C" __declspec(dllexport) int __cdecl tss_get_report_data(void** data,int* len){ return get_report_common("tss_get_report_data",data,len); }
extern "C" __declspec(dllexport) int __cdecl tss_del_report_data(void*){ logline("tss_del_report_data"); return 0; }
extern "C" __declspec(dllexport) int __cdecl tss_get_report_data2(void** data,int* len){ return get_report_common("tss_get_report_data2",data,len); }
extern "C" __declspec(dllexport) int __cdecl tss_get_report_data4(void** data,int* len){ return get_report_common("tss_get_report_data4",data,len); }
extern "C" __declspec(dllexport) int __cdecl tss_del_report_data4(void*){ logline("tss_del_report_data4"); return 0; }
extern "C" __declspec(dllexport) int __cdecl tss_recv_sec_signature(const void* p,int n){ char hex[96]; hex_preview(hex,sizeof(hex),p,n); char buf[180]; sprintf_s(buf,"tss_recv_sec_signature len=%d preview=%s",n,hex); logline(buf); return 0; }

BOOL WINAPI DllMain(HINSTANCE h,DWORD r,LPVOID){
    if(r==DLL_PROCESS_ATTACH){ g_module=h; init_log_path(h); DisableThreadLibraryCalls(h); if(!g_cs_ready){ InitializeCriticalSection(&g_cs); g_cs_ready=true; } logline("combined stub ABI v3 attach"); }
    if(r==DLL_PROCESS_DETACH){ logline("combined stub ABI v3 detach"); if(g_cs_ready){ DeleteCriticalSection(&g_cs); g_cs_ready=false; } }
    return TRUE;
}


