#define LIB_NAME "DNS"
#define MODULE_NAME "dns"

#ifdef DM_PLATFORM_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <netdb.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>    // pthread_kill on Linux
#endif

#include <dmsdk/sdk.h>
#include "luautils.h"


#define REQUEST_CAPACITY 8


struct dns_AddrInfoRequest {
    dns_AddrInfoRequest() {
        m_AddrInfo = 0;
        m_HostName = 0;
        m_Resolved = 0;
        m_Result = 0;
        m_ThreadId = 0;
    }
    lua_Listener m_Listener;
    struct addrinfo* m_AddrInfo;
    const char* m_HostName;
    int m_Resolved;
    int m_Result;

    // platform specific thread handle/id
    #ifdef DM_PLATFORM_WINDOWS
    HANDLE m_ThreadId;
    #else
    pthread_t m_ThreadId;
    #endif
};


// array of current getaddrinfo requests
static dmArray<dns_AddrInfoRequest> g_AddrInfoRequests;


// from inet_global_getaddrinfo in inet.c in LuaSocket
int lua_pushaddrinfo(lua_State* L, addrinfo* addrInfo) {
    int top = lua_gettop(L);

    struct addrinfo *iterator = NULL;
    int i = 1;

    lua_newtable(L);
    for (iterator = addrInfo; iterator; iterator = iterator->ai_next) {
        char hbuf[NI_MAXHOST];
        int ret = getnameinfo(iterator->ai_addr, (socklen_t) iterator->ai_addrlen,
        hbuf, (socklen_t) sizeof(hbuf), NULL, 0, NI_NUMERICHOST);
        if (ret) {
            lua_pop(L, 1);
            lua_pushnil(L);
            //lua_pushstring(L, socket_gaistrerror(ret));
            lua_pushstring(L, "Error resolving address info");
            assert(top + 2 == lua_gettop(L));
            return 2;
        }
        lua_pushnumber(L, i);
        lua_newtable(L);
        switch (iterator->ai_family) {
            case AF_INET:
            lua_pushliteral(L, "family");
            lua_pushliteral(L, "inet");
            lua_settable(L, -3);
            break;
            case AF_INET6:
            lua_pushliteral(L, "family");
            lua_pushliteral(L, "inet6");
            lua_settable(L, -3);
            break;
            case AF_UNSPEC:
            lua_pushliteral(L, "family");
            lua_pushliteral(L, "unspec");
            lua_settable(L, -3);
            break;
            default:
            lua_pushliteral(L, "family");
            lua_pushliteral(L, "unknown");
            lua_settable(L, -3);
            break;
        }
        lua_pushliteral(L, "addr");
        lua_pushstring(L, hbuf);
        lua_settable(L, -3);
        lua_settable(L, -3);
        i++;
    }
    assert(top + 1 == lua_gettop(L));
    return 1;
}


// Get the request associated with the current thread
static dns_AddrInfoRequest* dns_GetCurrentAddrInfoThread() {
    #ifdef DM_PLATFORM_WINDOWS
    HANDLE id = GetCurrentThread();
    for (int i = g_AddrInfoRequests.Size() - 1; i >= 0; i--) {
        struct dns_AddrInfoRequest *request = &g_AddrInfoRequests[i];
        if (id == request->m_ThreadId) {
            return request;
        }
    }
    #else
    pthread_t id = pthread_self();
    for (int i = g_AddrInfoRequests.Size() - 1; i >= 0; i--) {
        struct dns_AddrInfoRequest *request = &g_AddrInfoRequests[i];
        if (pthread_equal(id, request->m_ThreadId)) {
            return request;
        }
    }
    #endif
    return NULL;
}


// perform the addrinfo lookup (in a thread)
void* dns_GetAddrInfo(void *arg) {
    dns_AddrInfoRequest* request = dns_GetCurrentAddrInfoThread();
    if (!request) {
        dmLogInfo("dns_GetAddrInfo - Unable to find request for current thread");
        return NULL;
    }
    // from inet_global_getaddrinfo() in inet.c in LuaSocket
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    request->m_Result = getaddrinfo(request->m_HostName, NULL, &hints, &request->m_AddrInfo);
    request->m_Resolved = 1;
    return NULL;
}


#ifdef DM_PLATFORM_WINDOWS
DWORD WINAPI dns_GetAddrInfoWin(LPVOID lpParameter) {
    return (DWORD)dns_GetAddrInfo(NULL);
}
#endif


// create a thread and associate it with a request
static void dns_CreateGetAddrInfoThread(dns_AddrInfoRequest* request) {
    #ifdef DM_PLATFORM_WINDOWS
    request->m_ThreadId = CreateThread(NULL, 0, dns_GetAddrInfoWin, NULL, 0, NULL);
    #else
    pthread_create(&request->m_ThreadId, NULL, &dns_GetAddrInfo, NULL);
    #endif
}


// kill the thread associated with a request
static void dns_KillGetAddrInfoThread(dns_AddrInfoRequest* request) {
    #ifdef DM_PLATFORM_WINDOWS
    TerminateThread(request->m_ThreadId, 1);
    CloseHandle(request->m_ThreadId);
    #else
    // pthread_cancel doesn't exist in older Android NDKs
    pthread_kill(request->m_ThreadId, 1);
    #endif
    request->m_ThreadId = 0;
}


// Lua binding
static int dns_GetAddrInfoAsync(lua_State* L) {
    int top = lua_gettop(L);

    // create a request struct
    struct dns_AddrInfoRequest request;
    request.m_HostName = luaL_checkstring(L, 1);
    luaL_checklistener(L, 2, request.m_Listener);
    dns_CreateGetAddrInfoThread(&request);

    // add request to list
    if (g_AddrInfoRequests.Full()) {
        g_AddrInfoRequests.SetCapacity(g_AddrInfoRequests.Capacity() + REQUEST_CAPACITY);
    }
    g_AddrInfoRequests.Push(request);

    assert(top + 0 == lua_gettop(L));
    return 0;
}


static const luaL_reg Module_methods[] = {
    {"getaddrinfo_a", dns_GetAddrInfoAsync},
    {0, 0}
};

static void LuaInit(lua_State* L) {
    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, Module_methods);
    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeDNSExtension(dmExtension::AppParams* params) {
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeDNSExtension(dmExtension::Params* params) {
    LuaInit(params->m_L);
    return dmExtension::RESULT_OK;
}

dmExtension::Result UpdateDNSExtension(dmExtension::Params* params) {
    // check for finished requests
    for (int i = g_AddrInfoRequests.Size() - 1; i >= 0; i--) {
        struct dns_AddrInfoRequest request = g_AddrInfoRequests[i];
        if (request.m_Resolved) {
            g_AddrInfoRequests.EraseSwap(i);
            lua_State* L = request.m_Listener.m_L;
            lua_pushlistener(L, request.m_Listener);
            int args = lua_pushaddrinfo(L, request.m_AddrInfo);
            freeaddrinfo(request.m_AddrInfo);
            int ret = lua_pcall(L, 1 + args, 0, 0); // nargs = self + addrinfo
            if (ret != 0) {
                dmLogError("Problem when calling addrinfo callback: %s\n", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        }
    }
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeDNSExtension(dmExtension::AppParams* params) {
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeDNSExtension(dmExtension::Params* params) {
    // cancel active requests
    for (int i = g_AddrInfoRequests.Size() - 1; i >= 0; i--) {
        struct dns_AddrInfoRequest request = g_AddrInfoRequests[i];
        dmLogInfo("Killing active thread");
        dns_KillGetAddrInfoThread(&request);
    }
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(DNS, LIB_NAME, AppInitializeDNSExtension, AppFinalizeDNSExtension, InitializeDNSExtension, UpdateDNSExtension, 0, FinalizeDNSExtension)
