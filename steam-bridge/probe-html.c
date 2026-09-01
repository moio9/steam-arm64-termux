#include <windows.h>
#include <stdint.h>
#include <stdio.h>

typedef void *(__cdecl *create_interface_fn)(const char *, int *);
typedef int (__thiscall *create_pipe_fn)(void *);
typedef int (__thiscall *connect_user_fn)(void *, int);
typedef void *(__thiscall *get_generic_fn)(void *, int, int, const char *);
typedef int (__thiscall *html_init_fn)(void *);
typedef uint64_t (__thiscall *html_create_fn)(void *, const char *, const char *);
typedef void (__thiscall *html_remove_fn)(void *, uint32_t);
typedef void (__thiscall *html_set_size_fn)(void *, uint32_t, uint32_t, uint32_t);

typedef struct callback_msg {
    int32_t user;
    int32_t id;
    uint8_t *data;
    int32_t size;
} callback_msg;

typedef int (__cdecl *get_callback_fn)(int32_t, callback_msg *, int32_t *);
typedef int (__cdecl *get_api_result_fn)(
    int32_t, uint64_t, void *, int32_t, int32_t, int *);
typedef void (__cdecl *free_callback_fn)(int32_t);

typedef struct api_completed {
    uint64_t call;
    int32_t callback_id;
    uint32_t callback_size;
} api_completed;

#pragma pack(push, 4)
typedef struct needs_paint32 {
    uint32_t browser;
    const char *bgra;
    uint32_t width, height;
    uint32_t update_x, update_y, update_width, update_height;
    uint32_t scroll_x, scroll_y;
    float page_scale;
    uint32_t page_serial;
} needs_paint32;
#pragma pack(pop)

int main(void)
{
    freopen("G:\\steam-bridge-html-result.txt", "w", stdout);
    setvbuf(stdout, NULL, _IONBF, 0);
    HMODULE module = LoadLibraryA("C:\\windows\\syswow64\\lsteamclient.dll");
    if (!module) return 2;
    create_interface_fn create_interface =
        (create_interface_fn)GetProcAddress(module, "CreateInterface");
    get_callback_fn get_callback =
        (get_callback_fn)GetProcAddress(module, "Steam_BGetCallback");
    get_api_result_fn get_api_result =
        (get_api_result_fn)GetProcAddress(module, "Steam_GetAPICallResult");
    free_callback_fn free_callback =
        (free_callback_fn)GetProcAddress(module, "Steam_FreeLastCallback");
    if (!create_interface || !get_callback || !get_api_result || !free_callback)
        return 3;

    int result = -1;
    void *client = create_interface("SteamClient020", &result);
    if (!client) return 4;
    void **client_vtable = *(void ***)client;
    int pipe = ((create_pipe_fn)client_vtable[0])(client);
    int user = ((connect_user_fn)client_vtable[2])(client, pipe);
    void *html = ((get_generic_fn)client_vtable[31])(
        client, user, pipe, "STEAMHTMLSURFACE_INTERFACE_VERSION_005");
    if (!html) return 5;
    void **html_vtable = *(void ***)html;
    int initialized = ((html_init_fn)html_vtable[1])(html);
    uint64_t call = ((html_create_fn)html_vtable[3])(
        html, "lsteambridge-probe", NULL);
    printf("html=%p init=%d call=%llu\n", html, initialized,
           (unsigned long long)call);
    if (!initialized || !call) return 6;

    for (int attempt = 0; attempt < 100; ++attempt) {
        callback_msg msg = {0};
        int32_t steam_call = 0;
        if (!get_callback(pipe, &msg, &steam_call)) {
            Sleep(10);
            continue;
        }
        if (msg.id == 703 && msg.size == (int32_t)sizeof(api_completed)) {
            api_completed completed = *(api_completed *)msg.data;
            free_callback(pipe);
            uint32_t browser = 0;
            int failed = 1;
            int ok = get_api_result(pipe, completed.call, &browser,
                                    sizeof(browser), 4501, &failed);
            printf("ready_call=%llu callback=%d size=%u ok=%d failed=%d browser=%u\n",
                   (unsigned long long)completed.call, completed.callback_id,
                   completed.callback_size, ok, failed, browser);
            if (ok && !failed && browser) {
                ((html_set_size_fn)html_vtable[6])(html, browser, 1280, 720);
                for (int paint_attempt = 0; paint_attempt < 100; ++paint_attempt) {
                    callback_msg paint_msg = {0};
                    if (!get_callback(pipe, &paint_msg, &steam_call)) {
                        Sleep(10);
                        continue;
                    }
                    if (paint_msg.id == 4502 &&
                        paint_msg.size == (int32_t)sizeof(needs_paint32)) {
                        needs_paint32 paint = *(needs_paint32 *)paint_msg.data;
                        printf("paint_browser=%u size=%ux%u pixel=%02x%02x%02x%02x\n",
                               paint.browser, paint.width, paint.height,
                               (unsigned char)paint.bgra[0],
                               (unsigned char)paint.bgra[1],
                               (unsigned char)paint.bgra[2],
                               (unsigned char)paint.bgra[3]);
                        free_callback(pipe);
                        ((html_remove_fn)html_vtable[4])(html, browser);
                        return paint.browser == browser && paint.width == 1 &&
                               paint.height == 1 ? 0 : 9;
                    }
                    free_callback(pipe);
                }
                return 10;
            }
            return 7;
        }
        free_callback(pipe);
    }
    return 8;
}
