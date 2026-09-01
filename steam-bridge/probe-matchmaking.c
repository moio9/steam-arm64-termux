#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef void *(__cdecl *create_interface_fn)(const char *, int *);
typedef int (__thiscall *create_pipe_fn)(void *);
typedef int (__thiscall *connect_user_fn)(void *, int);
typedef void *(__thiscall *get_generic_fn)(void *, int, int, const char *);
typedef void *(__thiscall *request_internet_fn)(
    void *, uint32_t, void *, uint32_t, void *);
typedef int32_t (__thiscall *server_count_fn)(void *, void *);
typedef void (__thiscall *release_request_fn)(void *, void *);
typedef int (__cdecl *get_callback_fn)(int32_t, void *, int32_t *);
typedef void (__cdecl *free_callback_fn)(int32_t);

typedef struct callback_msg {
    int32_t user, id;
    uint8_t *data;
    int32_t size;
} callback_msg;

typedef struct list_response { void **vtable; } list_response;
typedef struct matchmaking_filter {
    char key[256];
    char value[256];
} matchmaking_filter;
static volatile LONG responded, failed, completed;

static void __thiscall on_responded(void *self, void *request, int32_t index)
{
    (void)self; (void)request; (void)index;
    InterlockedIncrement(&responded);
}

static void __thiscall on_failed(void *self, void *request, int32_t index)
{
    (void)self; (void)request; (void)index;
    InterlockedIncrement(&failed);
}

static void __thiscall on_complete(void *self, void *request, int32_t response)
{
    (void)self; (void)request; (void)response;
    InterlockedExchange(&completed, 1);
}

int main(void)
{
    freopen("G:\\steam-bridge-mm-result.txt", "w", stdout);
    setvbuf(stdout, NULL, _IONBF, 0);
    HMODULE module = LoadLibraryA("C:\\windows\\syswow64\\lsteamclient.dll");
    if (!module) return 2;
    create_interface_fn create_interface =
        (create_interface_fn)GetProcAddress(module, "CreateInterface");
    get_callback_fn get_callback =
        (get_callback_fn)GetProcAddress(module, "Steam_BGetCallback");
    free_callback_fn free_callback =
        (free_callback_fn)GetProcAddress(module, "Steam_FreeLastCallback");
    if (!create_interface || !get_callback || !free_callback) return 3;

    int code = -1;
    void *client = create_interface("SteamClient020", &code);
    void **client_vtable = client ? *(void ***)client : NULL;
    if (!client_vtable) return 4;
    int pipe = ((create_pipe_fn)client_vtable[0])(client);
    int user = ((connect_user_fn)client_vtable[2])(client, pipe);
    void *servers = ((get_generic_fn)client_vtable[11])(
        client, user, pipe, "SteamMatchMakingServers002");
    printf("servers=%p pipe=%d user=%d\n", servers, pipe, user);
    if (!servers) return 5;

    void *callbacks[] = {
        (void *)on_responded, (void *)on_failed, (void *)on_complete
    };
    list_response response = { callbacks };
    void **vtable = *(void ***)servers;
    matchmaking_filter filters[4] = {0};
    matchmaking_filter *filters_base = filters;
    strcpy(filters[0].key, "gamedir");
    strcpy(filters[0].value, "nmrih");
    strcpy(filters[1].key, "notfull");
    strcpy(filters[2].key, "hasplayers");
    strcpy(filters[3].key, "secure");
    printf("before_request\n");
    void *request = ((request_internet_fn)vtable[0])(
        servers, 224260, &filters_base, 4, &response);
    printf("after_request=%p\n", request);
    if (!request) return 6;

    for (int attempt = 0; attempt < 500 && !completed; ++attempt) {
        callback_msg msg = {0};
        int32_t steam_call = 0;
        if (get_callback(pipe, &msg, &steam_call)) free_callback(pipe);
        Sleep(10);
    }
    int32_t count = ((server_count_fn)vtable[11])(servers, request);
    printf("count=%d responded=%ld failed=%ld completed=%ld\n",
           count, responded, failed, completed);
    ((release_request_fn)vtable[6])(servers, request);
    return 0;
}
