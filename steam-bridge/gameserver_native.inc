/* Native ISteamGameServer013 dispatch for the bridge helper. */
struct bridge_game_server_state {
    HSteamUser (*create_local_user)(HSteamPipe *, EAccountType) = nullptr;
    HSteamPipe pipe = 0;
    HSteamUser user = 0;
    void *iface = nullptr;
    bool owns_handles = false;
};

template <typename R, typename... Args>
static R game_server_call(void *iface, size_t slot, Args... args)
{
    void **vtable = *reinterpret_cast<void ***>(iface);
    using method_t = R (*)(void *, Args...);
    return reinterpret_cast<method_t>(vtable[slot])(iface, args...);
}

template <typename... Args>
static void game_server_call_void(void *iface, size_t slot, Args... args)
{
    void **vtable = *reinterpret_cast<void ***>(iface);
    using method_t = void (*)(void *, Args...);
    reinterpret_cast<method_t>(vtable[slot])(iface, args...);
}

static bool ensure_game_server(ISteamClient *client, bridge_game_server_state *state)
{
    if (state->iface) return true;
    if (!state->user || !state->pipe) {
        state->user = state->create_local_user ?
            state->create_local_user(&state->pipe, k_EAccountTypeGameServer) :
            client->CreateLocalUser(&state->pipe, k_EAccountTypeGameServer);
        state->owns_handles = state->user && state->pipe;
    }
    if (!state->user || !state->pipe) return false;
    state->iface = client->GetISteamGameServer(state->user, state->pipe,
                                               "SteamGameServer013");
    return state->iface != nullptr;
}

static void handle_game_server_call(ISteamClient *client,
                                    bridge_game_server_state *state,
                                    lsteam_bridge_game_server_request *request,
                                    lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_GAME_SERVER_CALL;
    request->text[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    request->value[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    if (request->data_size > LSTEAM_BRIDGE_MAX_TICKET ||
        !ensure_game_server(client, state)) {
        response->status = request->data_size > LSTEAM_BRIDGE_MAX_TICKET ? -EINVAL : -ENODEV;
        return;
    }

    void *iface = state->iface;
    CSteamID steam_id(static_cast<uint64>(request->steam_id));
    response->status = 0;
    switch (request->method) {
    case 0:
        response->result = game_server_call<bool>(iface, 0, request->arg0,
                static_cast<uint16_t>(request->arg1), static_cast<uint16_t>(request->arg2),
                request->arg3, static_cast<uint32_t>(request->steam_id), request->text);
        break;
    case 1: case 2: case 3: case 14: case 15: case 18:
    case 21: case 22: case 23:
        game_server_call_void(iface, request->method, request->text);
        break;
    case 4: case 16: case 39:
        game_server_call_void(iface, request->method, request->arg0 != 0);
        break;
    case 5:
        game_server_call_void(iface, 5, request->text);
        break;
    case 6: case 7: case 19: case 34: case 41:
        game_server_call_void(iface, request->method);
        break;
    case 8: case 9: case 11:
        response->result = game_server_call<bool>(iface, request->method);
        break;
    case 10: case 25: {
        CSteamID result = game_server_call<CSteamID>(iface, request->method);
        response->steam_id = result.ConvertToUint64();
        response->result = response->steam_id;
        break;
    }
    case 12: case 13: case 40:
        game_server_call_void(iface, request->method, static_cast<int32_t>(request->arg0));
        break;
    case 17:
        game_server_call_void(iface, 17, static_cast<uint16_t>(request->arg0));
        break;
    case 20:
        game_server_call_void(iface, 20, request->text, request->value);
        break;
    case 24: {
        CSteamID authenticated;
        response->result = game_server_call<bool>(iface, 24, request->arg0,
                request->data, request->data_size, &authenticated);
        response->steam_id = authenticated.ConvertToUint64();
        break;
    }
    case 26: case 30:
        game_server_call_void(iface, request->method, steam_id);
        break;
    case 27:
        response->result = game_server_call<bool>(iface, 27, steam_id,
                                                   request->text, request->arg0);
        break;
    case 28: {
        uint32_t ticket_size = 0;
        response->result = game_server_call<uint32_t>(iface, 28, response->data,
                static_cast<int32_t>(sizeof(response->data)), &ticket_size);
        if (ticket_size > sizeof(response->data)) {
            response->status = -EOVERFLOW;
        } else {
            response->data_size = ticket_size;
        }
        break;
    }
    case 29:
        response->result = game_server_call<uint32_t>(iface, 29, request->data,
                                                       static_cast<int32_t>(request->data_size),
                                                       steam_id);
        break;
    case 31:
        game_server_call_void(iface, 31, request->arg0);
        break;
    case 32:
        response->result = game_server_call<uint32_t>(iface, 32, steam_id, request->arg0);
        break;
    case 33: {
        uint64_t group_value = 0;
        if (request->data_size != sizeof(group_value)) {
            response->status = -EINVAL;
            break;
        }
        memcpy(&group_value, request->data, sizeof(group_value));
        response->result = game_server_call<bool>(iface, 33, steam_id,
                                                   CSteamID(static_cast<uint64>(group_value)));
        break;
    }
    case 35:
        response->result = game_server_call<uint64_t>(iface, 35);
        break;
    case 36: {
        SteamIPAddress_t address = game_server_call<SteamIPAddress_t>(iface, 36);
        static_assert(sizeof(address) <= LSTEAM_BRIDGE_MAX_TICKET,
                      "Steam IP does not fit in bridge response");
        memcpy(response->data, &address, sizeof(address));
        response->data_size = sizeof(address);
        response->arg0 = address.x.m_unIPv4;
        break;
    }
    case 37:
        response->result = game_server_call<bool>(iface, 37, request->data,
                static_cast<int32_t>(request->data_size), request->arg0,
                static_cast<uint16_t>(request->arg1));
        break;
    case 38: {
        uint32_t ip = 0;
        uint16_t port = 0;
        int32_t size = game_server_call<int32_t>(iface, 38, response->data,
                static_cast<int32_t>(sizeof(response->data)), &ip, &port);
        if (size < 0 || size > static_cast<int32_t>(sizeof(response->data))) {
            response->status = -EIO;
        } else {
            response->data_size = size;
            response->arg0 = ip;
            response->arg1 = port;
            response->result = size;
        }
        break;
    }
    case 42: case 43:
        response->result = game_server_call<uint64_t>(iface, request->method, steam_id);
        break;
    default:
        response->status = -ENOSYS;
        break;
    }
}
