#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_TICKET_CAPACITY 2048u
#define APP_TICKET_INTERFACE "STEAMAPPTICKET_INTERFACE_VERSION001"

typedef void *(__cdecl *create_interface_fn)(const char *, int *);
typedef int32_t (__thiscall *create_pipe_fn)(void *);
typedef int32_t (__thiscall *connect_user_fn)(void *, int32_t);
typedef void *(__thiscall *get_user_fn)(
    void *, int32_t, int32_t, const char *);
typedef void *(__thiscall *get_generic_fn)(
    void *, int32_t, int32_t, const char *);
typedef uint64_t *(__thiscall *get_steam_id_fn)(void *, uint64_t *);
typedef uint32_t (__thiscall *get_app_ticket_fn)(
    void *, uint32_t, void *, uint32_t, uint32_t *, uint32_t *, uint32_t *,
    uint32_t *);
typedef void (__thiscall *run_frame_fn)(void *);
typedef uint32_t (__thiscall *get_ipc_call_count_fn)(void *);

_Static_assert(sizeof(void *) == 4, "this probe must be compiled for Win32");
_Static_assert(sizeof(uint64_t) == 8, "unexpected uint64_t size");

static int range_valid(uint32_t offset, uint32_t size, uint32_t total)
{
    return offset <= total && size <= total - offset;
}

static int read_app_id(uint32_t *app_id)
{
    const char *text = getenv("SteamAppId");
    char *end = NULL;
    unsigned long long parsed;

    if (!text || !*text) return 0;
    parsed = strtoull(text, &end, 10);
    if (!end || *end || !parsed || parsed > UINT32_MAX) return 0;
    *app_id = (uint32_t)parsed;
    return 1;
}

int main(void)
{
    HMODULE module = NULL;
    void *client = NULL;
    void **client_vtable = NULL;
    uint8_t ticket[APP_TICKET_CAPACITY] = {0};
    const char *failure = NULL;
    int exit_code = 1;
    uint32_t app_id = 0;

    if (!freopen("G:\\steamstub-appticket-probe-result.txt", "w", stdout))
        return 2;
    setvbuf(stdout, NULL, _IONBF, 0);

#define FAIL_AT(stage, code) \
    do { failure = (stage); exit_code = (code); goto done; } while (0)

    if (!read_app_id(&app_id)) FAIL_AT("steam_app_id", 3);

    module = LoadLibraryA("lsteamclient.dll");
    if (!module) FAIL_AT("load_lsteamclient", 4);

    create_interface_fn create_interface =
        (create_interface_fn)GetProcAddress(module, "CreateInterface");
    if (!create_interface) FAIL_AT("create_interface_export", 5);

    int interface_result = -1;
    client = create_interface("SteamClient008", &interface_result);
    if (!client) FAIL_AT("steam_client_008", 6);
    client_vtable = *(void ***)client;
    if (!client_vtable) FAIL_AT("steam_client_008_vtable", 7);

    int32_t pipe = ((create_pipe_fn)client_vtable[0])(client);
    if (!pipe) FAIL_AT("create_steam_pipe", 8);

    int32_t user = ((connect_user_fn)client_vtable[2])(client, pipe);
    if (!user) FAIL_AT("connect_global_user", 9);

    void *steam_user = ((get_user_fn)client_vtable[5])(
        client, user, pipe, "SteamUser012");
    if (!steam_user) FAIL_AT("steam_user_012", 10);
    void **user_vtable = *(void ***)steam_user;
    if (!user_vtable) FAIL_AT("steam_user_012_vtable", 11);

    uint64_t expected_steam_id = 0;
    if (((get_steam_id_fn)user_vtable[2])(
            steam_user, &expected_steam_id) != &expected_steam_id ||
        !expected_steam_id)
        FAIL_AT("current_steam_id", 12);

    void *app_ticket = ((get_generic_fn)client_vtable[13])(
        client, user, pipe, APP_TICKET_INTERFACE);
    if (!app_ticket) FAIL_AT("app_ticket_interface", 13);
    void **app_ticket_vtable = *(void ***)app_ticket;
    if (!app_ticket_vtable) FAIL_AT("app_ticket_vtable", 14);

    uint32_t app_id_offset = UINT32_MAX;
    uint32_t steam_id_offset = UINT32_MAX;
    uint32_t signature_offset = UINT32_MAX;
    uint32_t signature_size = UINT32_MAX;
    uint32_t ticket_size = ((get_app_ticket_fn)app_ticket_vtable[0])(
        app_ticket, app_id, ticket, sizeof(ticket), &app_id_offset,
        &steam_id_offset, &signature_offset, &signature_size);

    if (!ticket_size || ticket_size > sizeof(ticket))
        FAIL_AT("ticket_size", 15);
    if (!range_valid(app_id_offset, sizeof(uint32_t), ticket_size))
        FAIL_AT("app_id_range", 16);
    if (!range_valid(steam_id_offset, sizeof(uint64_t), ticket_size))
        FAIL_AT("steam_id_range", 17);
    if (!signature_size ||
        !range_valid(signature_offset, signature_size, ticket_size))
        FAIL_AT("signature_range", 18);

    uint32_t ticket_app_id = 0;
    uint64_t ticket_steam_id = 0;
    memcpy(&ticket_app_id, ticket + app_id_offset, sizeof(ticket_app_id));
    memcpy(&ticket_steam_id, ticket + steam_id_offset,
           sizeof(ticket_steam_id));
    if (ticket_app_id != app_id) FAIL_AT("app_id_value", 19);
    if (ticket_steam_id != expected_steam_id)
        FAIL_AT("steam_id_value", 20);

    uint8_t signature_nonzero = 0;
    for (uint32_t i = 0; i < signature_size; ++i)
        signature_nonzero |= ticket[signature_offset + i];
    if (!signature_nonzero) FAIL_AT("signature_value", 21);

    ((run_frame_fn)client_vtable[18])(client);
    uint32_t ipc_call_count =
        ((get_ipc_call_count_fn)client_vtable[19])(client);
    (void)ipc_call_count;

    exit_code = 0;

done:
    SecureZeroMemory(ticket, sizeof(ticket));
    if (module) FreeLibrary(module);
    if (failure)
        printf("SteamStub AppTicket probe: FAIL stage=%s\n", failure);
    else
        printf("SteamStub AppTicket probe: PASS\n");
    return exit_code;

#undef FAIL_AT
}
