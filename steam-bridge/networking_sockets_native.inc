#include <algorithm>
#include <vector>

struct bridge_network_sockets_state {
    uint32_t send_connection = 0;
    uint32_t send_total = 0;
    int32_t send_flags = 0;
    std::vector<uint8_t> send_data;
    lsteam_bridge_network_message_header received{};
    std::vector<uint8_t> received_data;
};

static bool network_sockets_receive_one(
    ISteamNetworkingSockets *sockets, bridge_network_sockets_state *state,
    uint32_t handle, bool poll_group)
{
    SteamNetworkingMessage_t *message = nullptr;
    int count = poll_group
        ? sockets->ReceiveMessagesOnPollGroup(handle, &message, 1)
        : sockets->ReceiveMessagesOnConnection(handle, &message, 1);
    if (count <= 0 || !message) return false;
    constexpr uint32_t maximum_message = 16u * 1024u * 1024u;
    if (message->m_cbSize < 0 ||
        static_cast<uint32_t>(message->m_cbSize) > maximum_message ||
        (message->m_cbSize && !message->m_pData)) {
        message->m_pfnRelease(message);
        return false;
    }
    memset(&state->received, 0, sizeof(state->received));
    memcpy(state->received.identity, &message->m_identityPeer,
           sizeof(message->m_identityPeer));
    state->received.connection = message->m_conn;
    state->received.connection_user_data = message->m_nConnUserData;
    state->received.time_received = message->m_usecTimeReceived;
    state->received.message_number = message->m_nMessageNumber;
    state->received.channel = message->m_nChannel;
    state->received.flags = message->m_nFlags;
    state->received.user_data = message->m_nUserData;
    state->received.lane = message->m_idxLane;
    state->received_data.resize(message->m_cbSize);
    if (message->m_cbSize)
        memcpy(state->received_data.data(), message->m_pData,
               message->m_cbSize);
    message->m_pfnRelease(message);
    return true;
}

static void handle_network_sockets_call(
    ISteamNetworkingSockets *sockets, bridge_network_sockets_state *state,
    lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    constexpr uint32_t maximum_message = 16u * 1024u * 1024u;
    constexpr uint32_t message_header_size =
        sizeof(lsteam_bridge_network_message_header);
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_NETWORK_SOCKETS_CALL;
    response->status = 0;
    if (!sockets) {
        response->status = -ENODEV;
        return;
    }
    if (request->data_size > sizeof(request->data)) {
        response->status = -EINVAL;
        return;
    }

    switch (request->method) {
    case 0: { /* CreateSocketPair */
        const SteamNetworkingIdentity *first = nullptr, *second = nullptr;
        SteamNetworkingIdentity identities[2]{};
        uint32_t mask = request->arg1;
        uint32_t expected = ((mask & 1) ? 1u : 0u) * sizeof(identities[0]) +
                            ((mask & 2) ? 1u : 0u) * sizeof(identities[0]);
        if (request->data_size != expected) {
            response->status = -EINVAL;
            break;
        }
        uint32_t offset = 0;
        if (mask & 1) {
            memcpy(&identities[0], request->data + offset,
                   sizeof(identities[0]));
            offset += sizeof(identities[0]);
            first = &identities[0];
        }
        if (mask & 2) {
            memcpy(&identities[1], request->data + offset,
                   sizeof(identities[1]));
            second = &identities[1];
        }
        HSteamNetConnection connection1 = 0, connection2 = 0;
        response->result = sockets->CreateSocketPair(
            &connection1, &connection2, request->arg0 != 0, first, second);
        response->arg0 = connection1;
        response->arg1 = connection2;
        break;
    }
    case 1: { /* SendMessageToConnection, chunked */
        uint32_t total = request->arg2;
        uint32_t offset = request->arg3;
        if (total > maximum_message || offset > total ||
            request->data_size > total - offset) {
            response->status = -EINVAL;
            break;
        }
        if (!offset) {
            state->send_connection = request->arg0;
            state->send_total = total;
            state->send_flags = static_cast<int32_t>(request->arg1);
            state->send_data.assign(total, 0);
        }
        if (state->send_connection != request->arg0 ||
            state->send_total != total ||
            state->send_flags != static_cast<int32_t>(request->arg1) ||
            state->send_data.size() != total) {
            response->status = -EINVAL;
            break;
        }
        if (request->data_size)
            memcpy(state->send_data.data() + offset, request->data,
                   request->data_size);
        response->result = k_EResultOK;
        if (offset + request->data_size == total) {
            int64 message_number = 0;
            response->result = sockets->SendMessageToConnection(
                state->send_connection,
                total ? state->send_data.data() : nullptr, total,
                state->send_flags, &message_number);
            response->steam_id = static_cast<uint64_t>(message_number);
            state->send_data.clear();
            state->send_total = 0;
        }
        break;
    }
    case 2:
    case 20: { /* ReceiveMessagesOnConnection / PollGroup, chunked */
        uint32_t offset = request->arg1;
        if (!offset && !network_sockets_receive_one(
                sockets, state, request->arg0, request->method == 20)) {
            response->result = 0;
            break;
        }
        if (offset > state->received_data.size()) {
            response->status = -EINVAL;
            break;
        }
        response->result = 1;
        response->arg0 = state->received_data.size();
        memcpy(response->data, &state->received, message_header_size);
        size_t chunk = state->received_data.size() - offset;
        chunk = std::min(chunk, sizeof(response->data) - message_header_size);
        if (chunk)
            memcpy(response->data + message_header_size,
                   state->received_data.data() + offset, chunk);
        response->data_size = message_header_size + chunk;
        break;
    }
    case 3:
        response->result = sockets->AcceptConnection(request->arg0);
        break;
    case 4:
        response->result = sockets->CloseConnection(
            request->arg0, static_cast<int32_t>(request->arg1),
            request->text[0] ? request->text : nullptr, request->arg2 != 0);
        break;
    case 5:
        response->result = sockets->CloseListenSocket(request->arg0);
        break;
    case 6:
        response->result = sockets->SetConnectionUserData(
            request->arg0, static_cast<int64_t>(request->steam_id));
        break;
    case 7:
        response->steam_id = static_cast<uint64_t>(
            sockets->GetConnectionUserData(request->arg0));
        response->result = 1;
        break;
    case 8:
        sockets->SetConnectionName(request->arg0, request->text);
        response->result = 1;
        break;
    case 9: {
        uint32_t capacity = std::min<uint32_t>(
            request->arg1, sizeof(response->data));
        response->result = capacity && sockets->GetConnectionName(
            request->arg0, reinterpret_cast<char *>(response->data), capacity);
        response->data_size = response->result
            ? strnlen(reinterpret_cast<char *>(response->data), capacity) + 1
            : 0;
        break;
    }
    case 10:
        response->result = sockets->FlushMessagesOnConnection(request->arg0);
        break;
    case 11: {
        SteamNetConnectionInfo_t info{};
        response->result = sockets->GetConnectionInfo(request->arg0, &info);
        if (response->result) {
            static_assert(sizeof(info) <= sizeof(response->data),
                          "connection info exceeds bridge response");
            memcpy(response->data, &info, sizeof(info));
            response->data_size = sizeof(info);
        }
        break;
    }
    case 12: {
        uint32_t lane_count = std::min<uint32_t>(request->arg1, 16);
        SteamNetConnectionRealTimeStatus_t quick{};
        std::vector<SteamNetConnectionRealTimeLaneStatus_t> lanes(lane_count);
        response->result = sockets->GetConnectionRealTimeStatus(
            request->arg0, request->arg2 & 1 ? &quick : nullptr,
            lane_count, lane_count ? lanes.data() : nullptr);
        size_t size = 0;
        if (request->arg2 & 1) {
            memcpy(response->data, &quick, sizeof(quick));
            size += sizeof(quick);
        }
        if (lane_count) {
            size_t lane_bytes = lane_count * sizeof(lanes[0]);
            if (size + lane_bytes > sizeof(response->data)) {
                response->status = -EOVERFLOW;
                break;
            }
            memcpy(response->data + size, lanes.data(), lane_bytes);
            size += lane_bytes;
        }
        response->data_size = size;
        response->arg0 = lane_count;
        break;
    }
    case 13: {
        uint32_t capacity = std::min<uint32_t>(
            request->arg1, sizeof(response->data));
        response->result = static_cast<uint64_t>(static_cast<int64_t>(
            sockets->GetDetailedConnectionStatus(
                request->arg0,
                capacity ? reinterpret_cast<char *>(response->data) : nullptr,
                capacity)));
        response->data_size = capacity
            ? strnlen(reinterpret_cast<char *>(response->data), capacity) + 1
            : 0;
        if (response->data_size > capacity) response->data_size = capacity;
        break;
    }
    case 14: {
        SteamNetworkingIdentity identity{};
        response->result = sockets->GetIdentity(&identity);
        if (response->result) {
            memcpy(response->data, &identity, sizeof(identity));
            response->data_size = sizeof(identity);
        }
        break;
    }
    case 15:
        response->result = sockets->InitAuthentication();
        break;
    case 16: {
        SteamNetAuthenticationStatus_t details{};
        response->result = sockets->GetAuthenticationStatus(
            request->arg0 ? &details : nullptr);
        if (request->arg0) {
            memcpy(response->data, &details, sizeof(details));
            response->data_size = sizeof(details);
        }
        break;
    }
    case 17:
        response->result = sockets->CreatePollGroup();
        break;
    case 18:
        response->result = sockets->DestroyPollGroup(request->arg0);
        break;
    case 19:
        response->result = sockets->SetConnectionPollGroup(
            request->arg0, request->arg1);
        break;
    case 21:
        if (request->arg1) response->status = -ENOTSUP;
        else response->result = sockets->CreateListenSocketP2P(
            static_cast<int32_t>(request->arg0), 0, nullptr);
        break;
    case 22: {
        if (request->arg2 || request->data_size != sizeof(SteamNetworkingIdentity)) {
            response->status = request->arg2 ? -ENOTSUP : -EINVAL;
            break;
        }
        SteamNetworkingIdentity identity{};
        memcpy(&identity, request->data, sizeof(identity));
        response->result = sockets->ConnectP2P(
            identity, static_cast<int32_t>(request->arg0), 0, nullptr);
        break;
    }
    case 23:
    case 24: {
        if (request->arg0 || request->data_size != sizeof(SteamNetworkingIPAddr)) {
            response->status = request->arg0 ? -ENOTSUP : -EINVAL;
            break;
        }
        SteamNetworkingIPAddr address{};
        memcpy(&address, request->data, sizeof(address));
        response->result = request->method == 23
            ? sockets->CreateListenSocketIP(address, 0, nullptr)
            : sockets->ConnectByIPAddress(address, 0, nullptr);
        break;
    }
    case 25: {
        SteamNetworkingIPAddr address{};
        response->result = sockets->GetListenSocketAddress(
            request->arg0, &address);
        if (response->result) {
            memcpy(response->data, &address, sizeof(address));
            response->data_size = sizeof(address);
        }
        break;
    }
    case 26: {
        uint32_t count = request->arg1;
        size_t priorities_size = count * sizeof(int32_t);
        size_t weights_size = count * sizeof(uint16_t);
        if (count > 128 || request->data_size != priorities_size + weights_size) {
            response->status = -EINVAL;
            break;
        }
        response->result = sockets->ConfigureConnectionLanes(
            request->arg0, count,
            count ? reinterpret_cast<const int32_t *>(request->data) : nullptr,
            count ? reinterpret_cast<const uint16_t *>(
                request->data + priorities_size) : nullptr);
        break;
    }
    default:
        response->status = -ENOSYS;
        break;
    }
}
