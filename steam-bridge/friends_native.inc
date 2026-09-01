/* Native SteamFriends017 dispatch for identity, friends and rich presence. */
static void friends_copy_string(lsteam_bridge_game_server_response *response,
                                const char *value)
{
    response->arg0 = value != nullptr;
    if (!value) return;
    size_t size = strlen(value) + 1;
    if (size > sizeof(response->data)) {
        response->status = -EOVERFLOW;
        return;
    }
    memcpy(response->data, value, size);
    response->data_size = size;
}

static void handle_friends_call(ISteamFriends *friends,
                                lsteam_bridge_game_server_request *request,
                                lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_FRIENDS_CALL;
    response->status = 0;
    request->text[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    request->value[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    if (!friends) {
        response->status = -ENODEV;
        return;
    }

    CSteamID steam_id(static_cast<uint64>(request->steam_id));
    switch (request->method) {
    case 0:
        friends_copy_string(response, friends->GetPersonaName());
        break;
    case 1:
        response->result = friends->GetPersonaState();
        break;
    case 2:
        response->result = friends->GetFriendCount(static_cast<int>(request->arg0));
        break;
    case 3: {
        CSteamID result = friends->GetFriendByIndex(
            static_cast<int>(request->arg0), static_cast<int>(request->arg1));
        response->steam_id = result.ConvertToUint64();
        response->result = response->steam_id;
        break;
    }
    case 4:
        response->result = friends->GetFriendRelationship(steam_id);
        break;
    case 5:
        response->result = friends->GetFriendPersonaState(steam_id);
        break;
    case 6:
        friends_copy_string(response, friends->GetFriendPersonaName(steam_id));
        break;
    case 7:
        response->result = friends->HasFriend(
            steam_id, static_cast<int>(request->arg0));
        break;
    case 8:
        response->result = friends->RequestUserInformation(
            steam_id, request->arg0 != 0);
        break;
    case 9:
        response->result = friends->SetRichPresence(
            request->text, request->arg0 ? request->value : nullptr);
        break;
    case 10:
        friends->ClearRichPresence();
        break;
    case 11:
        friends_copy_string(response,
                            friends->GetFriendRichPresence(steam_id, request->text));
        break;
    case 12:
        friends->RequestFriendRichPresence(steam_id);
        break;
    case 13:
        response->result = friends->InviteUserToGame(steam_id, request->text);
        break;
    case 14:
        response->result = friends->GetSmallFriendAvatar(steam_id);
        break;
    case 15:
        response->result = friends->GetMediumFriendAvatar(steam_id);
        break;
    case 16:
        response->result = friends->GetLargeFriendAvatar(steam_id);
        break;
    case 17:
        friends->SetPlayedWith(steam_id);
        break;
    case 18:
        friends->SetInGameVoiceSpeaking(steam_id, request->arg0 != 0);
        break;
    case 19:
        response->result = friends->GetFriendSteamLevel(steam_id);
        break;
    case 20:
        friends_copy_string(response, friends->GetPlayerNickname(steam_id));
        break;
    case 21:
        response->result = friends->GetFriendRichPresenceKeyCount(steam_id);
        break;
    case 22:
        friends_copy_string(response, friends->GetFriendRichPresenceKeyByIndex(
            steam_id, static_cast<int>(request->arg0)));
        break;
    case 23: {
        FriendGameInfo_t info{};
        response->result = friends->GetFriendGamePlayed(steam_id, &info);
        static_assert(sizeof(info) <= LSTEAM_BRIDGE_MAX_TICKET,
                      "friend game info does not fit bridge response");
        if (response->result) {
            memcpy(response->data, &info, sizeof(info));
            response->data_size = sizeof(info);
        }
        break;
    }
    default:
        response->status = -ENOSYS;
        break;
    }
}
