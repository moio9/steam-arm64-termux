#include <vector>

struct bridge_network_messages_state {
    SteamNetworkingIdentity send_identity{};
    uint32_t send_total = 0;
    int32_t send_flags = 0;
    int32_t send_channel = 0;
    std::vector<uint8_t> send_data;
    lsteam_bridge_network_message_header received{};
    std::vector<uint8_t> received_data;
};

static_assert(sizeof(SteamNetworkingIdentity) == 136,
              "unexpected native SteamNetworkingIdentity ABI");

static bool network_messages_read_identity(
    const lsteam_bridge_game_server_request *request,
    SteamNetworkingIdentity *identity)
{
    if (request->data_size < sizeof(*identity)) return false;
    memcpy(identity, request->data, sizeof(*identity));
    return true;
}

static uint64_t network_messages_steam_id(
    const SteamNetworkingIdentity &identity)
{
    if (identity.m_eType != k_ESteamNetworkingIdentityType_SteamID ||
        identity.m_cbSize != static_cast<int>(sizeof(uint64_t)))
        return 0;
    return identity.data.m_steamID64;
}

static void network_messages_set_steam_id(
    lsteam_bridge_network_message_header *header, uint64_t steam_id)
{
    SteamNetworkingIdentity identity{};
    identity.m_eType = k_ESteamNetworkingIdentityType_SteamID;
    identity.m_cbSize = sizeof(steam_id);
    identity.data.m_steamID64 = steam_id;
    memcpy(header->identity, &identity, sizeof(identity));
}

static void handle_network_messages_call(
    ISteamNetworkingMessages *messages, ISteamNetworking *legacy,
    bridge_network_messages_state *state,
    lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    constexpr uint32_t maximum_message = 16u * 1024u * 1024u;
    constexpr uint32_t identity_size = sizeof(SteamNetworkingIdentity);
    constexpr uint32_t header_size =
        sizeof(lsteam_bridge_network_message_header);
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_NETWORK_MESSAGES_CALL;
    response->status = 0;
    if (!messages && !legacy) {
        response->status = -ENODEV;
        return;
    }
    if (request->data_size > sizeof(request->data)) {
        response->status = -EINVAL;
        return;
    }

    switch (request->method) {
    case 0: {
        uint32_t total = request->arg2;
        uint32_t offset = request->arg3;
        uint32_t prefix = offset ? 0 : identity_size;
        if (total > maximum_message || offset > total ||
            request->data_size < prefix ||
            request->data_size - prefix > total - offset) {
            response->status = -EINVAL;
            break;
        }
        if (!offset) {
            memcpy(&state->send_identity, request->data, identity_size);
            state->send_total = total;
            state->send_flags = static_cast<int32_t>(request->arg0);
            state->send_channel = static_cast<int32_t>(request->arg1);
            state->send_data.assign(total, 0);
        }
        if (state->send_total != total ||
            state->send_flags != static_cast<int32_t>(request->arg0) ||
            state->send_channel != static_cast<int32_t>(request->arg1) ||
            state->send_data.size() != total) {
            response->status = -EINVAL;
            break;
        }
        uint32_t chunk = request->data_size - prefix;
        if (chunk)
            memcpy(state->send_data.data() + offset,
                   request->data + prefix, chunk);
        response->result = 1; /* k_EResultOK while assembling. */
        if (offset + chunk == total) {
            if (messages) {
                response->result = messages->SendMessageToUser(
                    state->send_identity,
                    total ? state->send_data.data() : nullptr, total,
                    state->send_flags, state->send_channel);
            } else {
                uint64_t remote = network_messages_steam_id(
                    state->send_identity);
                EP2PSend send_type = state->send_flags & 8
                    ? k_EP2PSendReliable : k_EP2PSendUnreliable;
                response->result = remote && legacy->SendP2PPacket(
                    CSteamID(static_cast<uint64>(remote)),
                    total ? state->send_data.data() : nullptr, total,
                    send_type, state->send_channel) ? 1 : 2;
            }
            state->send_data.clear();
            state->send_total = 0;
        }
        break;
    }
    case 1: {
        uint32_t offset = request->arg1;
        if (!offset) {
            memset(&state->received, 0, sizeof(state->received));
            if (messages) {
                SteamNetworkingMessage_t *message = nullptr;
                int count = messages->ReceiveMessagesOnChannel(
                    static_cast<int32_t>(request->arg0), &message, 1);
                if (count <= 0 || !message) {
                    response->result = 0;
                    break;
                }
                if (message->m_cbSize < 0 ||
                    static_cast<uint32_t>(message->m_cbSize) > maximum_message ||
                    (message->m_cbSize && !message->m_pData)) {
                    message->m_pfnRelease(message);
                    response->status = -EOVERFLOW;
                    break;
                }
                memcpy(state->received.identity, &message->m_identityPeer,
                       identity_size);
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
            } else {
                uint32_t size = 0;
                int32_t channel = static_cast<int32_t>(request->arg0);
                if (!legacy->IsP2PPacketAvailable(&size, channel)) {
                    response->result = 0;
                    break;
                }
                if (size > maximum_message) {
                    response->status = -EOVERFLOW;
                    break;
                }
                state->received_data.assign(size, 0);
                uint32_t read = 0;
                CSteamID sender;
                if (!legacy->ReadP2PPacket(
                        size ? state->received_data.data() : nullptr, size,
                        &read, &sender, channel) || read > size) {
                    state->received_data.clear();
                    response->result = 0;
                    break;
                }
                state->received_data.resize(read);
                network_messages_set_steam_id(
                    &state->received, sender.ConvertToUint64());
                state->received.channel = channel;
            }
        }
        if (offset > state->received_data.size()) {
            response->status = -EINVAL;
            break;
        }
        response->result = 1;
        response->arg0 = state->received_data.size();
        memcpy(response->data, &state->received, header_size);
        size_t chunk = state->received_data.size() - offset;
        if (chunk > sizeof(response->data) - header_size)
            chunk = sizeof(response->data) - header_size;
        if (chunk)
            memcpy(response->data + header_size,
                   state->received_data.data() + offset, chunk);
        response->data_size = header_size + chunk;
        break;
    }
    case 2:
    case 3:
    case 4: {
        SteamNetworkingIdentity identity{};
        if (!network_messages_read_identity(request, &identity)) {
            response->status = -EINVAL;
            break;
        }
        if (messages) {
            if (request->method == 2)
                response->result = messages->AcceptSessionWithUser(identity);
            else if (request->method == 3)
                response->result = messages->CloseSessionWithUser(identity);
            else
                response->result = messages->CloseChannelWithUser(
                    identity, static_cast<int32_t>(request->arg0));
        } else {
            uint64_t remote = network_messages_steam_id(identity);
            if (!remote) break;
            if (request->method == 2)
                response->result = legacy->AcceptP2PSessionWithUser(
                    CSteamID(static_cast<uint64>(remote)));
            else if (request->method == 3)
                response->result = legacy->CloseP2PSessionWithUser(
                    CSteamID(static_cast<uint64>(remote)));
            else
                response->result = legacy->CloseP2PChannelWithUser(
                    CSteamID(static_cast<uint64>(remote)),
                    static_cast<int32_t>(request->arg0));
        }
        break;
    }
    case 5: {
        SteamNetworkingIdentity identity{};
        if (!network_messages_read_identity(request, &identity)) {
            response->status = -EINVAL;
            break;
        }
        SteamNetConnectionInfo_t info{};
        SteamNetConnectionRealTimeStatus_t quick{};
        if (messages) {
            response->result = messages->GetSessionConnectionInfo(
                identity, request->arg0 & 1 ? &info : nullptr,
                request->arg0 & 2 ? &quick : nullptr);
        } else {
            uint64_t remote = network_messages_steam_id(identity);
            P2PSessionState_t session{};
            bool found = remote && legacy->GetP2PSessionState(
                CSteamID(static_cast<uint64>(remote)), &session);
            uint32_t connection_state = !found ? 0 :
                session.m_bConnectionActive ? 3 :
                session.m_bConnecting ? 1 : 0;
            info.m_identityRemote = identity;
            info.m_eState = static_cast<ESteamNetworkingConnectionState>(
                connection_state);
            quick.m_eState = static_cast<ESteamNetworkingConnectionState>(
                connection_state);
            quick.m_cbPendingReliable = session.m_nBytesQueuedForSend;
            response->result = connection_state;
        }
        size_t size = 0;
        if (request->arg0 & 1) {
            memcpy(response->data + size, &info, sizeof(info));
            size += sizeof(info);
        }
        if (request->arg0 & 2) {
            memcpy(response->data + size, &quick, sizeof(quick));
            size += sizeof(quick);
        }
        response->data_size = size;
        break;
    }
    default:
        response->status = -ENOSYS;
        break;
    }
}
