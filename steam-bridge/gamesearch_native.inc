/* Native SteamMatchGameSearch001 dispatch. */
static void handle_game_search_call(
    ISteamGameSearch *search,
    lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_GAME_SEARCH_CALL;
    response->status = 0;
    request->text[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    request->value[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    if (!search) {
        response->status = -ENODEV;
        return;
    }
    if (request->data_size > sizeof(request->data)) {
        response->status = -EINVAL;
        return;
    }

    switch (request->method) {
    case 0:
        response->result = search->AddGameSearchParams(
            request->text, request->value);
        break;
    case 1:
        response->result = search->SearchForGameWithLobby(
            CSteamID(static_cast<uint64>(request->steam_id)),
            static_cast<int32_t>(request->arg0),
            static_cast<int32_t>(request->arg1));
        break;
    case 2:
        response->result = search->SearchForGameSolo(
            static_cast<int32_t>(request->arg0),
            static_cast<int32_t>(request->arg1));
        break;
    case 3:
        response->result = search->AcceptGame();
        break;
    case 4:
        response->result = search->DeclineGame();
        break;
    case 5: {
        uint32_t capacity = request->arg0;
        if (!capacity || capacity > sizeof(response->data)) {
            response->status = -E2BIG;
            break;
        }
        memset(response->data, 0, capacity);
        response->result = search->RetrieveConnectionDetails(
            CSteamID(static_cast<uint64>(request->steam_id)),
            reinterpret_cast<char *>(response->data),
            static_cast<int>(capacity));
        size_t length = strnlen(reinterpret_cast<char *>(response->data),
                                capacity);
        response->data_size = length < capacity ? length + 1 : capacity;
        break;
    }
    case 6:
        response->result = search->EndGameSearch();
        break;
    case 7:
        response->result = search->SetGameHostParams(
            request->text, request->value);
        break;
    case 8:
        response->result = search->SetConnectionDetails(
            reinterpret_cast<const char *>(request->data),
            static_cast<int>(request->data_size));
        break;
    case 9:
        response->result = search->RequestPlayersForGame(
            static_cast<int32_t>(request->arg0),
            static_cast<int32_t>(request->arg1),
            static_cast<int32_t>(request->arg2));
        break;
    case 10:
        response->result = search->HostConfirmGameStart(request->steam_id);
        break;
    case 11:
        response->result = search->CancelRequestPlayersForGame();
        break;
    case 12: {
        uint64_t player = 0;
        if (request->data_size != sizeof(player)) {
            response->status = -EINVAL;
            break;
        }
        memcpy(&player, request->data, sizeof(player));
        response->result = search->SubmitPlayerResult(
            request->steam_id, CSteamID(static_cast<uint64>(player)),
            static_cast<EPlayerResult_t>(request->arg0));
        break;
    }
    case 13:
        response->result = search->EndGame(request->steam_id);
        break;
    default:
        response->status = -ENOSYS;
        break;
    }
}
