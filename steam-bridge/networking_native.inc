#include <vector>

struct bridge_networking_state {
    uint64_t send_remote = 0;
    uint32_t send_total = 0;
    int32_t send_channel = 0;
    std::vector<uint8_t> send_data;
    uint32_t read_capacity = 0;
    uint32_t read_size = 0;
    int32_t read_channel = 0;
    uint64_t read_remote = 0;
    bool read_ok = false;
    std::vector<uint8_t> read_data;
};

static void handle_networking_call(
    ISteamNetworking *networking, bridge_networking_state *state,
    lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    constexpr uint32_t maximum_packet = 16u * 1024u * 1024u;
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_NETWORKING_CALL;
    response->status = 0;
    if (!networking) {
        response->status = -ENODEV;
        return;
    }
    if (request->data_size > LSTEAM_BRIDGE_MAX_TICKET) {
        response->status = -EINVAL;
        return;
    }

    CSteamID remote(static_cast<uint64>(request->steam_id));
    switch (request->method) {
    case 0: {
        uint32_t total = request->arg2, offset = request->arg3;
        if (total > maximum_packet || offset > total ||
            request->data_size > total - offset) {
            response->status = -EINVAL;
            break;
        }
        if (!offset) {
            state->send_remote = request->steam_id;
            state->send_total = total;
            state->send_channel = static_cast<int32_t>(request->arg1);
            state->send_data.assign(total, 0);
        }
        if (state->send_remote != request->steam_id ||
            state->send_total != total ||
            state->send_channel != static_cast<int32_t>(request->arg1) ||
            state->send_data.size() != total) {
            response->status = -EINVAL;
            break;
        }
        if (request->data_size)
            memcpy(state->send_data.data() + offset, request->data,
                   request->data_size);
        response->result = 1;
        if (offset + request->data_size == total) {
            response->result = networking->SendP2PPacket(
                remote, total ? state->send_data.data() : nullptr, total,
                static_cast<EP2PSend>(request->arg0),
                static_cast<int32_t>(request->arg1));
            state->send_data.clear();
            state->send_total = 0;
        }
        break;
    }
    case 1: {
        uint32_t size = 0;
        response->result = networking->IsP2PPacketAvailable(
            &size, static_cast<int32_t>(request->arg0));
        response->arg0 = size;
        break;
    }
    case 2: {
        uint32_t capacity = request->arg0, offset = request->arg2;
        int32_t channel = static_cast<int32_t>(request->arg1);
        if (capacity > maximum_packet || offset > capacity) {
            response->status = -EINVAL;
            break;
        }
        if (!offset || state->read_capacity != capacity ||
            state->read_channel != channel) {
            state->read_capacity = capacity;
            state->read_channel = channel;
            state->read_size = 0;
            state->read_remote = 0;
            state->read_data.assign(capacity, 0);
            CSteamID sender;
            state->read_ok = networking->ReadP2PPacket(
                capacity ? state->read_data.data() : nullptr, capacity,
                &state->read_size, &sender, channel);
            state->read_remote = sender.ConvertToUint64();
        }
        response->result = state->read_ok;
        response->arg0 = state->read_size;
        response->steam_id = state->read_remote;
        if (!state->read_ok) break;
        if (state->read_size > capacity || offset > state->read_size) {
            response->status = -EIO;
            break;
        }
        size_t chunk = state->read_size - offset;
        if (chunk > sizeof(response->data)) chunk = sizeof(response->data);
        if (chunk) memcpy(response->data, state->read_data.data() + offset, chunk);
        response->data_size = chunk;
        break;
    }
    case 3:
        response->result = networking->AcceptP2PSessionWithUser(remote);
        break;
    case 4:
        response->result = networking->CloseP2PSessionWithUser(remote);
        break;
    case 5:
        response->result = networking->CloseP2PChannelWithUser(
            remote, static_cast<int32_t>(request->arg0));
        break;
    case 6: {
        P2PSessionState_t session{};
        response->result = networking->GetP2PSessionState(remote, &session);
        static_assert(sizeof(session) <= LSTEAM_BRIDGE_MAX_TICKET,
                      "P2P session state does not fit bridge response");
        if (response->result) {
            memcpy(response->data, &session, sizeof(session));
            response->data_size = sizeof(session);
        }
        break;
    }
    case 7:
        response->result = networking->AllowP2PPacketRelay(request->arg0 != 0);
        break;
    default:
        response->status = -ENOSYS;
        break;
    }
}
