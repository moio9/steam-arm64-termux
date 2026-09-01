#include <vector>

struct bridge_utils_state {
    int32_t image = 0;
    uint32_t image_size = 0;
    std::vector<uint8_t> image_data;
};

static void utils_copy_string(lsteam_bridge_game_server_response *response,
                              const char *value)
{
    if (!value) value = "";
    size_t size = strlen(value) + 1;
    if (size > sizeof(response->data)) {
        response->status = -EOVERFLOW;
        return;
    }
    memcpy(response->data, value, size);
    response->data_size = size;
}

static void handle_utils_call(ISteamUtils *utils, bridge_utils_state *state,
                              lsteam_bridge_game_server_request *request,
                              lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_UTILS_CALL;
    response->status = 0;
    request->text[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    if (!utils) {
        response->status = -ENODEV;
        return;
    }

    switch (request->method) {
    case 0: {
        uint32_t width = 0, height = 0;
        response->result = utils->GetImageSize(
            static_cast<int>(request->arg0), &width, &height);
        response->arg0 = width;
        response->arg1 = height;
        break;
    }
    case 1: {
        uint32_t total = request->arg1;
        uint32_t offset = request->arg2;
        if (!total || total > 4u * 1024u * 1024u || offset > total) {
            response->status = -EINVAL;
            break;
        }
        if (state->image != static_cast<int32_t>(request->arg0) ||
            state->image_size != total || !offset || state->image_data.empty()) {
            state->image = static_cast<int32_t>(request->arg0);
            state->image_size = total;
            state->image_data.resize(total);
            if (!utils->GetImageRGBA(state->image, state->image_data.data(),
                                     static_cast<int>(total))) {
                state->image_data.clear();
                response->result = 0;
                break;
            }
        }
        if (state->image_data.size() != total) {
            response->result = 0;
            break;
        }
        size_t chunk = total - offset;
        if (chunk > sizeof(response->data)) chunk = sizeof(response->data);
        memcpy(response->data, state->image_data.data() + offset, chunk);
        response->data_size = chunk;
        response->arg0 = total;
        response->result = 1;
        break;
    }
    case 2:
        response->result = utils->GetConnectedUniverse();
        break;
    case 3:
        response->result = utils->GetServerRealTime();
        break;
    case 4:
        utils_copy_string(response, utils->GetIPCountry());
        break;
    case 5:
        response->result = utils->GetCurrentBatteryPower();
        break;
    case 6:
        response->result = utils->IsOverlayEnabled();
        break;
    case 7:
        response->result = utils->IsSteamInBigPictureMode();
        break;
    case 8:
        utils_copy_string(response, utils->GetSteamUILanguage());
        break;
    case 9:
        response->result = utils->IsSteamRunningOnSteamDeck();
        break;
    case 10:
        response->result = utils->InitFilterText(request->arg0);
        break;
    case 11: {
        CSteamID source(static_cast<uint64>(request->steam_id));
        int length = utils->FilterText(
            static_cast<ETextFilteringContext>(request->arg0), source,
            request->text, reinterpret_cast<char *>(response->data),
            sizeof(response->data));
        if (length < 0 || length >= static_cast<int>(sizeof(response->data))) {
            response->status = -EOVERFLOW;
        } else {
            response->result = length;
            response->data_size = length + 1;
        }
        break;
    }
    case 12:
        utils->SetOverlayNotificationPosition(
            static_cast<ENotificationPosition>(request->arg0));
        break;
    default:
        response->status = -ENOSYS;
        break;
    }
}
