#include <algorithm>
#include <vector>

struct bridge_app_list_state {
    std::vector<AppId_t> installed;
};

static void app_list_copy_string(
    lsteam_bridge_game_server_response *response, const char *value,
    uint32_t capacity)
{
    if (!capacity || capacity > sizeof(response->data)) {
        response->status = -E2BIG;
        return;
    }
    size_t length = strnlen(value, capacity);
    response->data_size = length < capacity ? length + 1 : capacity;
    if (response->data_size)
        memcpy(response->data, value, response->data_size);
}

static void handle_app_list_call(
    ISteamAppList *apps, bridge_app_list_state *state,
    lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_APP_LIST_CALL;
    response->status = 0;
    if (!apps) {
        response->status = -ENODEV;
        return;
    }

    switch (request->method) {
    case 0:
        response->result = apps->GetNumInstalledApps();
        break;
    case 1: {
        uint32_t offset = request->arg0;
        uint32_t maximum = request->arg1;
        uint32_t requested_total = request->arg2;
        constexpr uint32_t max_apps = 65536;
        constexpr uint32_t max_chunk =
            sizeof(response->data) / sizeof(AppId_t);
        if (!maximum || maximum > max_chunk || requested_total > max_apps) {
            response->status = -E2BIG;
            break;
        }
        if (!offset || state->installed.empty()) {
            uint32_t available = apps->GetNumInstalledApps();
            uint32_t capacity = std::min(available, requested_total);
            state->installed.resize(capacity);
            uint32_t count = apps->GetInstalledApps(
                capacity ? state->installed.data() : nullptr, capacity);
            if (count > capacity) count = capacity;
            state->installed.resize(count);
        }
        if (offset >= state->installed.size()) {
            response->result = 0;
            break;
        }
        uint32_t count = std::min<uint32_t>(
            maximum, state->installed.size() - offset);
        memcpy(response->data, state->installed.data() + offset,
               count * sizeof(AppId_t));
        response->data_size = count * sizeof(AppId_t);
        response->result = count;
        break;
    }
    case 2:
    case 3: {
        uint32_t capacity = std::min<uint32_t>(
            request->arg1, sizeof(response->data));
        if (!capacity) {
            response->status = -EINVAL;
            break;
        }
        char value[LSTEAM_BRIDGE_MAX_TICKET]{};
        int result = request->method == 2
            ? apps->GetAppName(request->arg0, value, capacity)
            : apps->GetAppInstallDir(request->arg0, value, capacity);
        response->result = static_cast<uint64_t>(static_cast<int64_t>(result));
        app_list_copy_string(response, value, capacity);
        break;
    }
    case 4:
        response->result = static_cast<uint64_t>(static_cast<int64_t>(
            apps->GetAppBuildId(request->arg0)));
        break;
    default:
        response->status = -ENOSYS;
        break;
    }
}
