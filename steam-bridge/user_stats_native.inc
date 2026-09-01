#include <vector>

static void user_stats_copy_string(
    lsteam_bridge_game_server_response *response, const char *value)
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

static void handle_user_stats_call(
    ISteamUserStats *stats, lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_USER_STATS_CALL;
    response->status = 0;
    request->text[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    request->value[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    if (!stats) {
        response->status = -ENODEV;
        return;
    }
    if (request->data_size > LSTEAM_BRIDGE_MAX_TICKET) {
        response->status = -EINVAL;
        return;
    }

    CSteamID user(static_cast<uint64>(request->steam_id));
    switch (request->method) {
    case 0:
        response->result = stats->RequestCurrentStats();
        break;
    case 1: {
        int32 value = 0;
        response->result = stats->GetStat(request->text, &value);
        memcpy(&response->arg0, &value, sizeof(value));
        break;
    }
    case 2: {
        float value = 0.0f;
        response->result = stats->GetStat(request->text, &value);
        memcpy(&response->arg0, &value, sizeof(value));
        break;
    }
    case 3:
        response->result = stats->SetStat(
            request->text, static_cast<int32>(request->arg0));
        break;
    case 4: {
        float value = 0.0f;
        memcpy(&value, &request->arg0, sizeof(value));
        response->result = stats->SetStat(request->text, value);
        break;
    }
    case 5: {
        float count = 0.0f;
        double length = 0.0;
        if (request->data_size != sizeof(length)) {
            response->status = -EINVAL;
            break;
        }
        memcpy(&count, &request->arg0, sizeof(count));
        memcpy(&length, request->data, sizeof(length));
        response->result = stats->UpdateAvgRateStat(
            request->text, count, length);
        break;
    }
    case 6: {
        bool achieved = false;
        response->result = stats->GetAchievement(request->text, &achieved);
        response->arg0 = achieved;
        break;
    }
    case 7:
        response->result = stats->SetAchievement(request->text);
        break;
    case 8:
        response->result = stats->ClearAchievement(request->text);
        break;
    case 9: {
        bool achieved = false;
        uint32 unlock_time = 0;
        response->result = stats->GetAchievementAndUnlockTime(
            request->text, &achieved, &unlock_time);
        response->arg0 = achieved;
        response->arg1 = unlock_time;
        break;
    }
    case 10:
        response->result = stats->StoreStats();
        break;
    case 11:
        response->result = stats->GetAchievementIcon(request->text);
        break;
    case 12:
        user_stats_copy_string(response,
            stats->GetAchievementDisplayAttribute(
                request->text, request->value));
        break;
    case 13:
        response->result = stats->IndicateAchievementProgress(
            request->text, request->arg0, request->arg1);
        break;
    case 14:
        response->result = stats->GetNumAchievements();
        break;
    case 15:
        user_stats_copy_string(response,
            stats->GetAchievementName(request->arg0));
        break;
    case 16:
        response->result = stats->RequestUserStats(user);
        break;
    case 17: {
        int32 value = 0;
        response->result = stats->GetUserStat(user, request->text, &value);
        memcpy(&response->arg0, &value, sizeof(value));
        break;
    }
    case 18: {
        float value = 0.0f;
        response->result = stats->GetUserStat(user, request->text, &value);
        memcpy(&response->arg0, &value, sizeof(value));
        break;
    }
    case 19: {
        bool achieved = false;
        response->result = stats->GetUserAchievement(
            user, request->text, &achieved);
        response->arg0 = achieved;
        break;
    }
    case 20: {
        bool achieved = false;
        uint32 unlock_time = 0;
        response->result = stats->GetUserAchievementAndUnlockTime(
            user, request->text, &achieved, &unlock_time);
        response->arg0 = achieved;
        response->arg1 = unlock_time;
        break;
    }
    case 21:
        response->result = stats->ResetAllStats(request->arg0 != 0);
        break;
    case 22:
        response->result = stats->FindOrCreateLeaderboard(
            request->text,
            static_cast<ELeaderboardSortMethod>(request->arg0),
            static_cast<ELeaderboardDisplayType>(request->arg1));
        break;
    case 23:
        response->result = stats->FindLeaderboard(request->text);
        break;
    case 24:
        user_stats_copy_string(response,
            stats->GetLeaderboardName(request->steam_id));
        break;
    case 25:
        response->result = stats->GetLeaderboardEntryCount(request->steam_id);
        break;
    case 26:
        response->result = stats->GetLeaderboardSortMethod(request->steam_id);
        break;
    case 27:
        response->result = stats->GetLeaderboardDisplayType(request->steam_id);
        break;
    case 28:
        response->result = stats->DownloadLeaderboardEntries(
            request->steam_id,
            static_cast<ELeaderboardDataRequest>(request->arg0),
            static_cast<int32>(request->arg1),
            static_cast<int32>(request->arg2));
        break;
    case 29: {
        uint32 count = request->arg0;
        if (count > 100 || request->data_size != count * sizeof(uint64_t)) {
            response->status = -EINVAL;
            break;
        }
        std::vector<CSteamID> users;
        users.reserve(count);
        for (uint32 i = 0; i < count; ++i) {
            uint64_t value = 0;
            memcpy(&value, request->data + i * sizeof(value), sizeof(value));
            users.emplace_back(static_cast<uint64>(value));
        }
        response->result = stats->DownloadLeaderboardEntriesForUsers(
            request->steam_id, users.data(), static_cast<int>(count));
        break;
    }
    case 30: {
        if (request->arg1 > k_cLeaderboardDetailsMax) {
            response->status = -EINVAL;
            break;
        }
        LeaderboardEntry_t entry{};
        std::vector<int32> details(request->arg1);
        response->result = stats->GetDownloadedLeaderboardEntry(
            request->steam_id, static_cast<int>(request->arg0), &entry,
            details.empty() ? nullptr : details.data(),
            static_cast<int>(details.size()));
        response->steam_id = entry.m_steamIDUser.ConvertToUint64();
        memcpy(&response->arg0, &entry.m_nGlobalRank,
               sizeof(entry.m_nGlobalRank));
        memcpy(&response->arg1, &entry.m_nScore, sizeof(entry.m_nScore));
        int32 copied = entry.m_cDetails;
        if (copied < 0) copied = 0;
        if (copied > static_cast<int32>(details.size()))
            copied = details.size();
        memcpy(response->data, &entry.m_cDetails, sizeof(entry.m_cDetails));
        memcpy(response->data + 4, &entry.m_hUGC, sizeof(entry.m_hUGC));
        if (copied)
            memcpy(response->data + 12, details.data(), copied * sizeof(int32));
        response->data_size = 12 + copied * sizeof(int32);
        break;
    }
    case 31: {
        uint32 count = request->arg2;
        if (count > k_cLeaderboardDetailsMax ||
            request->data_size != count * sizeof(int32)) {
            response->status = -EINVAL;
            break;
        }
        response->result = stats->UploadLeaderboardScore(
            request->steam_id,
            static_cast<ELeaderboardUploadScoreMethod>(request->arg0),
            static_cast<int32>(request->arg1),
            reinterpret_cast<const int32 *>(request->data), count);
        break;
    }
    case 32: {
        uint64_t ugc = 0;
        if (request->data_size != sizeof(ugc)) {
            response->status = -EINVAL;
            break;
        }
        memcpy(&ugc, request->data, sizeof(ugc));
        response->result = stats->AttachLeaderboardUGC(
            request->steam_id, ugc);
        break;
    }
    case 33:
        response->result = stats->GetNumberOfCurrentPlayers();
        break;
    case 34:
        response->result = stats->RequestGlobalAchievementPercentages();
        break;
    case 35:
    case 36: {
        char name[LSTEAM_BRIDGE_MAX_TEXT]{};
        float percent = 0.0f;
        bool achieved = false;
        int iterator = request->method == 35 ?
            stats->GetMostAchievedAchievementInfo(
                name, sizeof(name), &percent, &achieved) :
            stats->GetNextMostAchievedAchievementInfo(
                static_cast<int32>(request->arg0), name, sizeof(name),
                &percent, &achieved);
        response->result = static_cast<uint64_t>(static_cast<int64_t>(iterator));
        memcpy(&response->arg0, &percent, sizeof(percent));
        response->arg1 = achieved;
        user_stats_copy_string(response, name);
        break;
    }
    case 37: {
        float percent = 0.0f;
        response->result = stats->GetAchievementAchievedPercent(
            request->text, &percent);
        memcpy(&response->arg0, &percent, sizeof(percent));
        break;
    }
    case 38:
        response->result = stats->RequestGlobalStats(
            static_cast<int32>(request->arg0));
        break;
    case 39: {
        int64 value = 0;
        response->result = stats->GetGlobalStat(request->text, &value);
        memcpy(response->data, &value, sizeof(value));
        response->data_size = sizeof(value);
        break;
    }
    case 40: {
        double value = 0.0;
        response->result = stats->GetGlobalStat(request->text, &value);
        memcpy(response->data, &value, sizeof(value));
        response->data_size = sizeof(value);
        break;
    }
    case 41:
    case 42: {
        uint32 bytes = request->arg0;
        if (bytes > sizeof(response->data) || bytes % sizeof(uint64_t)) {
            response->status = -EINVAL;
            break;
        }
        response->result = request->method == 41 ?
            stats->GetGlobalStatHistory(
                request->text, reinterpret_cast<int64 *>(response->data), bytes) :
            stats->GetGlobalStatHistory(
                request->text, reinterpret_cast<double *>(response->data), bytes);
        uint64_t count = response->result;
        response->data_size = count * sizeof(uint64_t) <= bytes ?
            count * sizeof(uint64_t) : bytes;
        break;
    }
    case 43: {
        int32 minimum = 0, maximum = 0;
        response->result = stats->GetAchievementProgressLimits(
            request->text, &minimum, &maximum);
        memcpy(&response->arg0, &minimum, sizeof(minimum));
        memcpy(&response->arg1, &maximum, sizeof(maximum));
        break;
    }
    case 44: {
        float minimum = 0.0f, maximum = 0.0f;
        response->result = stats->GetAchievementProgressLimits(
            request->text, &minimum, &maximum);
        memcpy(&response->arg0, &minimum, sizeof(minimum));
        memcpy(&response->arg1, &maximum, sizeof(maximum));
        break;
    }
    default:
        response->status = -ENOSYS;
        break;
    }
}
