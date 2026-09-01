/* Native SteamMatchMaking009 lobby/favorites dispatch. */
static bool lobby_second_id(const lsteam_bridge_game_server_request *request,
                            CSteamID *id)
{
    uint64_t value = 0;
    if (request->data_size != sizeof(value)) return false;
    memcpy(&value, request->data, sizeof(value));
    *id = CSteamID(static_cast<uint64>(value));
    return true;
}

static void lobby_copy_string(lsteam_bridge_game_server_response *response,
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

static void handle_lobby_call(ISteamMatchmaking *matchmaking,
                              lsteam_bridge_game_server_request *request,
                              lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_LOBBY_CALL;
    response->status = 0;
    request->text[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    request->value[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    if (!matchmaking) {
        response->status = -ENODEV;
        return;
    }
    if (request->data_size > LSTEAM_BRIDGE_MAX_TICKET) {
        response->status = -EINVAL;
        return;
    }

    CSteamID lobby(static_cast<uint64>(request->steam_id));
    CSteamID second;
    switch (request->method) {
    case 0:
        response->result = matchmaking->GetFavoriteGameCount();
        break;
    case 1: {
        AppId_t appid = 0;
        uint32_t ip = 0, flags = 0, last_played = 0;
        uint16_t connection_port = 0, query_port = 0;
        response->result = matchmaking->GetFavoriteGame(
            static_cast<int>(request->arg0), &appid, &ip, &connection_port,
            &query_port, &flags, &last_played);
        response->arg0 = appid;
        response->arg1 = ip;
        memcpy(response->data, &connection_port, sizeof(connection_port));
        memcpy(response->data + 2, &query_port, sizeof(query_port));
        memcpy(response->data + 4, &flags, sizeof(flags));
        memcpy(response->data + 8, &last_played, sizeof(last_played));
        response->data_size = 12;
        break;
    }
    case 2:
        response->result = matchmaking->AddFavoriteGame(
            request->arg0, request->arg1,
            static_cast<uint16_t>(request->arg2),
            static_cast<uint16_t>(request->arg2 >> 16), request->arg3,
            static_cast<uint32_t>(request->steam_id));
        break;
    case 3:
        response->result = matchmaking->RemoveFavoriteGame(
            request->arg0, request->arg1,
            static_cast<uint16_t>(request->arg2),
            static_cast<uint16_t>(request->arg2 >> 16), request->arg3);
        break;
    case 4:
        response->result = matchmaking->RequestLobbyList();
        break;
    case 5:
        matchmaking->AddRequestLobbyListStringFilter(
            request->text, request->value,
            static_cast<ELobbyComparison>(request->arg0));
        break;
    case 6:
        matchmaking->AddRequestLobbyListNumericalFilter(
            request->text, static_cast<int32_t>(request->arg0),
            static_cast<ELobbyComparison>(request->arg1));
        break;
    case 7:
        matchmaking->AddRequestLobbyListNearValueFilter(
            request->text, static_cast<int32_t>(request->arg0));
        break;
    case 8:
        matchmaking->AddRequestLobbyListFilterSlotsAvailable(
            static_cast<int32_t>(request->arg0));
        break;
    case 9:
        matchmaking->AddRequestLobbyListDistanceFilter(
            static_cast<ELobbyDistanceFilter>(request->arg0));
        break;
    case 10:
        matchmaking->AddRequestLobbyListResultCountFilter(
            static_cast<int32_t>(request->arg0));
        break;
    case 11:
        matchmaking->AddRequestLobbyListCompatibleMembersFilter(lobby);
        break;
    case 12: {
        CSteamID id = matchmaking->GetLobbyByIndex(
            static_cast<int32_t>(request->arg0));
        response->steam_id = id.ConvertToUint64();
        response->result = response->steam_id;
        break;
    }
    case 13:
        response->result = matchmaking->CreateLobby(
            static_cast<ELobbyType>(request->arg0),
            static_cast<int32_t>(request->arg1));
        break;
    case 14:
        response->result = matchmaking->JoinLobby(lobby);
        break;
    case 15:
        matchmaking->LeaveLobby(lobby);
        break;
    case 16:
        if (!lobby_second_id(request, &second)) response->status = -EINVAL;
        else response->result = matchmaking->InviteUserToLobby(lobby, second);
        break;
    case 17:
        response->result = matchmaking->GetNumLobbyMembers(lobby);
        break;
    case 18: {
        CSteamID id = matchmaking->GetLobbyMemberByIndex(
            lobby, static_cast<int32_t>(request->arg0));
        response->steam_id = id.ConvertToUint64();
        response->result = response->steam_id;
        break;
    }
    case 19:
        lobby_copy_string(response,
                          matchmaking->GetLobbyData(lobby, request->text));
        break;
    case 20:
        response->result = matchmaking->SetLobbyData(
            lobby, request->text, request->value);
        break;
    case 21:
        response->result = matchmaking->GetLobbyDataCount(lobby);
        break;
    case 22: {
        constexpr int half = LSTEAM_BRIDGE_MAX_TICKET / 2;
        char key[half]{}, value[half]{};
        response->result = matchmaking->GetLobbyDataByIndex(
            lobby, static_cast<int32_t>(request->arg0), key, sizeof(key),
            value, sizeof(value));
        if (response->result) {
            memcpy(response->data, key, sizeof(key));
            memcpy(response->data + half, value, sizeof(value));
            response->data_size = sizeof(response->data);
        }
        break;
    }
    case 23:
        response->result = matchmaking->DeleteLobbyData(lobby, request->text);
        break;
    case 24:
        if (!lobby_second_id(request, &second)) response->status = -EINVAL;
        else lobby_copy_string(response, matchmaking->GetLobbyMemberData(
                 lobby, second, request->text));
        break;
    case 25:
        matchmaking->SetLobbyMemberData(lobby, request->text, request->value);
        break;
    case 26:
        response->result = matchmaking->SendLobbyChatMsg(
            lobby, request->data, static_cast<int>(request->data_size));
        break;
    case 27: {
        CSteamID sender;
        EChatEntryType type = k_EChatEntryTypeInvalid;
        int size = matchmaking->GetLobbyChatEntry(
            lobby, static_cast<int32_t>(request->arg0), &sender,
            response->data, sizeof(response->data), &type);
        if (size < 0 || size > static_cast<int>(sizeof(response->data))) {
            response->status = -EOVERFLOW;
        } else {
            response->result = size;
            response->data_size = size;
            response->steam_id = sender.ConvertToUint64();
            response->arg0 = type;
        }
        break;
    }
    case 28:
        response->result = matchmaking->RequestLobbyData(lobby);
        break;
    case 29:
        if (!lobby_second_id(request, &second)) response->status = -EINVAL;
        else matchmaking->SetLobbyGameServer(
                 lobby, request->arg0, static_cast<uint16_t>(request->arg1),
                 second);
        break;
    case 30: {
        uint32_t ip = 0;
        uint16_t port = 0;
        CSteamID server;
        response->result = matchmaking->GetLobbyGameServer(
            lobby, &ip, &port, &server);
        response->arg0 = ip;
        response->arg1 = port;
        response->steam_id = server.ConvertToUint64();
        break;
    }
    case 31:
        response->result = matchmaking->SetLobbyMemberLimit(
            lobby, static_cast<int32_t>(request->arg0));
        break;
    case 32:
        response->result = matchmaking->GetLobbyMemberLimit(lobby);
        break;
    case 33:
        response->result = matchmaking->SetLobbyType(
            lobby, static_cast<ELobbyType>(request->arg0));
        break;
    case 34:
        response->result = matchmaking->SetLobbyJoinable(
            lobby, request->arg0 != 0);
        break;
    case 35: {
        CSteamID owner = matchmaking->GetLobbyOwner(lobby);
        response->steam_id = owner.ConvertToUint64();
        response->result = response->steam_id;
        break;
    }
    case 36:
        if (!lobby_second_id(request, &second)) response->status = -EINVAL;
        else response->result = matchmaking->SetLobbyOwner(lobby, second);
        break;
    case 37:
        if (!lobby_second_id(request, &second)) response->status = -EINVAL;
        else response->result = matchmaking->SetLinkedLobby(lobby, second);
        break;
    default:
        response->status = -ENOSYS;
        break;
    }
}
