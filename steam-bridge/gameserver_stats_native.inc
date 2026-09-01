struct bridge_game_server_stats_state {
    ISteamGameServerStats *iface = nullptr;
};

static bool ensure_game_server_stats(ISteamClient *client,
                                     bridge_game_server_state *game_server,
                                     bridge_game_server_stats_state *stats)
{
    if (stats->iface) return true;
    if (!ensure_game_server(client, game_server)) return false;
    stats->iface = client->GetISteamGameServerStats(
        game_server->user, game_server->pipe,
        STEAMGAMESERVERSTATS_INTERFACE_VERSION);
    return stats->iface != nullptr;
}

static void handle_game_server_stats_call(
    ISteamClient *client, bridge_game_server_state *game_server,
    bridge_game_server_stats_state *state,
    lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_GAME_SERVER_STATS_CALL;
    response->status = 0;
    request->text[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    if (!ensure_game_server_stats(client, game_server, state)) {
        response->status = -ENODEV;
        return;
    }

    ISteamGameServerStats *stats = state->iface;
    CSteamID user(static_cast<uint64>(request->steam_id));
    switch (request->method) {
    case 0:
        response->result = stats->RequestUserStats(user);
        break;
    case 1: {
        int32 value = 0;
        response->result = stats->GetUserStat(user, request->text, &value);
        memcpy(&response->arg0, &value, sizeof(value));
        break;
    }
    case 2: {
        float value = 0.0f;
        response->result = stats->GetUserStat(user, request->text, &value);
        memcpy(&response->arg0, &value, sizeof(value));
        break;
    }
    case 3: {
        bool achieved = false;
        response->result = stats->GetUserAchievement(
            user, request->text, &achieved);
        response->arg0 = achieved;
        break;
    }
    case 4:
        response->result = stats->SetUserStat(
            user, request->text, static_cast<int32_t>(request->arg0));
        break;
    case 5: {
        float value = 0.0f;
        memcpy(&value, &request->arg0, sizeof(value));
        response->result = stats->SetUserStat(user, request->text, value);
        break;
    }
    case 6: {
        float count = 0.0f;
        double length = 0.0;
        memcpy(&count, &request->arg0, sizeof(count));
        if (request->data_size != sizeof(length)) {
            response->status = -EINVAL;
            break;
        }
        memcpy(&length, request->data, sizeof(length));
        response->result = stats->UpdateUserAvgRateStat(
            user, request->text, count, length);
        break;
    }
    case 7:
        response->result = stats->SetUserAchievement(user, request->text);
        break;
    case 8:
        response->result = stats->ClearUserAchievement(user, request->text);
        break;
    case 9:
        response->result = stats->StoreUserStats(user);
        break;
    default:
        response->status = -ENOSYS;
        break;
    }
}
