/* Native ISteamUser021 calls which need to share Steam's real auth state. */
static void handle_user_call(ISteamUser *steam_user,
                             lsteam_bridge_game_server_request *request,
                             lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_USER_CALL;
    response->status = 0;
    request->text[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    if (!steam_user) {
        response->status = -ENODEV;
        return;
    }
    if (request->data_size > LSTEAM_BRIDGE_MAX_TICKET) {
        response->status = -EINVAL;
        return;
    }

    CSteamID steam_id(static_cast<uint64>(request->steam_id));
    switch (request->method) {
    case 0:
        response->result = steam_user->BeginAuthSession(
            request->data, static_cast<int>(request->data_size), steam_id);
        break;
    case 1:
        steam_user->EndAuthSession(steam_id);
        break;
    case 2:
        steam_user->CancelAuthTicket(request->arg0);
        break;
    case 3:
        response->result = steam_user->UserHasLicenseForApp(steam_id,
                                                            request->arg0);
        break;
    case 4:
        steam_user->TerminateGameConnection_DEPRECATED(
            request->arg0, static_cast<uint16_t>(request->arg1));
        break;
    case 5:
        steam_user->AdvertiseGame(steam_id, request->arg0,
                                  static_cast<uint16_t>(request->arg1));
        break;
    case 6:
        response->result = steam_user->BIsBehindNAT();
        break;
    case 7:
        response->result = steam_user->GetGameBadgeLevel(
            static_cast<int>(request->arg0), request->arg1 != 0);
        break;
    case 8:
        response->result = steam_user->GetPlayerSteamLevel();
        break;
    case 9:
        response->result = steam_user->RequestEncryptedAppTicket(
            request->data_size ? request->data : nullptr,
            static_cast<int>(request->data_size));
        break;
    case 10: {
        uint32_t size = 0;
        response->result = steam_user->GetEncryptedAppTicket(
            response->data, sizeof(response->data), &size);
        if (size > sizeof(response->data)) response->status = -EOVERFLOW;
        else response->data_size = size;
        break;
    }
    case 11:
        steam_user->TrackAppUsageEvent(
            CGameID(static_cast<uint64>(request->steam_id)),
            static_cast<int>(request->arg0), request->text);
        break;
    case 12: {
        char *folder = reinterpret_cast<char *>(response->data);
        response->result = steam_user->GetUserDataFolder(
            folder, static_cast<int>(sizeof(response->data)));
        if (response->result) {
            folder[sizeof(response->data) - 1] = 0;
            response->data_size = strnlen(folder, sizeof(response->data) - 1) + 1;
        }
        break;
    }
    case 13:
        response->result = steam_user->RequestStoreAuthURL(request->text);
        break;
    case 14:
        response->result = steam_user->BIsPhoneVerified();
        break;
    case 15:
        response->result = steam_user->BIsTwoFactorEnabled();
        break;
    case 16:
        response->result = steam_user->BIsPhoneIdentifying();
        break;
    case 17:
        response->result = steam_user->BIsPhoneRequiringVerification();
        break;
    case 18:
        response->result = steam_user->GetMarketEligibility();
        break;
    case 19:
        response->result = steam_user->GetDurationControl();
        break;
    case 20:
        response->result = steam_user->BSetDurationControlOnlineState(
            static_cast<EDurationControlOnlineState>(request->arg0));
        break;
    default:
        response->status = -ENOSYS;
        break;
    }
}
