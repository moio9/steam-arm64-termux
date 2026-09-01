#include <stdint.h>
#include <string.h>

extern "C" {

struct callback_msg {
    int32_t steam_user;
    int32_t callback_id;
    uint8_t *data;
    int32_t data_size;
};

int32_t Steam_CreateSteamPipe(void) { return 1; }
int32_t Steam_ConnectToGlobalUser(int32_t) { return 1; }
int32_t Steam_CreateLocalUser(int32_t *pipe, int32_t)
{
    if (pipe) *pipe = 2;
    return 2;
}
bool Steam_BLoggedOn(int32_t, int32_t) { return true; }
void Steam_ReleaseUser(int32_t, int32_t) {}
bool Steam_BReleaseSteamPipe(int32_t) { return true; }
void *CreateInterface(const char *, int *result)
{
    if (result) *result = 1;
    return nullptr;
}

bool Steam_BGetCallback(int32_t, callback_msg *msg, int32_t *call)
{
    static uint8_t payload[] = {0x11, 0x22, 0x33, 0x44};
    static bool delivered;
    if (delivered) return false;
    delivered = true;
    msg->steam_user = 1;
    msg->callback_id = 101;
    msg->data = payload;
    msg->data_size = sizeof(payload);
    if (call) *call = 7;
    return true;
}

bool Steam_FreeLastCallback(int32_t) { return true; }

bool Steam_GetAPICallResult(int32_t, uint64_t, void *data, int32_t size,
                            int32_t, bool *failed)
{
    if (failed) *failed = false;
    if (data && size > 0) memset(data, 0x5a, size);
    return true;
}

}
