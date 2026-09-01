#include <vector>

static void apps_copy_string(lsteam_bridge_game_server_response *response,
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

static void handle_apps_call(
    ISteamApps *apps, lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_APPS_CALL;
    response->status = 0;
    request->text[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    if (!apps) {
        response->status = -ENODEV;
        return;
    }

    switch (request->method) {
    case 0: response->result = apps->BIsSubscribed(); break;
    case 1: response->result = apps->BIsLowViolence(); break;
    case 2: response->result = apps->BIsCybercafe(); break;
    case 3: response->result = apps->BIsVACBanned(); break;
    case 4: apps_copy_string(response, apps->GetCurrentGameLanguage()); break;
    case 5: apps_copy_string(response, apps->GetAvailableGameLanguages()); break;
    case 6: response->result = apps->BIsSubscribedApp(request->arg0); break;
    case 7: response->result = apps->BIsDlcInstalled(request->arg0); break;
    case 8:
        response->result = apps->GetEarliestPurchaseUnixTime(request->arg0);
        break;
    case 9: response->result = apps->BIsSubscribedFromFreeWeekend(); break;
    case 10:
        response->result = static_cast<uint64_t>(
            static_cast<int64_t>(apps->GetDLCCount()));
        break;
    case 11: {
        AppId_t app_id = 0;
        bool available = false;
        char name[LSTEAM_BRIDGE_MAX_TICKET]{};
        response->result = apps->BGetDLCDataByIndex(
            static_cast<int>(request->arg0), &app_id, &available, name,
            static_cast<int>(sizeof(name)));
        response->arg0 = app_id;
        response->arg1 = available;
        apps_copy_string(response, name);
        break;
    }
    case 12: {
        char name[LSTEAM_BRIDGE_MAX_TICKET]{};
        response->result = apps->GetCurrentBetaName(
            name, static_cast<int>(sizeof(name)));
        apps_copy_string(response, name);
        break;
    }
    case 13: {
        uint32_t maximum = request->arg1;
        if (maximum > sizeof(response->data) / sizeof(DepotId_t)) {
            response->status = -E2BIG;
            break;
        }
        std::vector<DepotId_t> depots(maximum);
        uint32_t count = apps->GetInstalledDepots(
            request->arg0, maximum ? depots.data() : nullptr, maximum);
        if (count > maximum) count = maximum;
        if (count) memcpy(response->data, depots.data(),
                          count * sizeof(DepotId_t));
        response->data_size = count * sizeof(DepotId_t);
        response->result = count;
        break;
    }
    case 14: {
        char folder[LSTEAM_BRIDGE_MAX_TICKET]{};
        response->result = apps->GetAppInstallDir(
            request->arg0, folder, sizeof(folder));
        apps_copy_string(response, folder);
        break;
    }
    case 15: response->result = apps->BIsAppInstalled(request->arg0); break;
    case 16: response->steam_id = apps->GetAppOwner().ConvertToUint64(); break;
    case 17: apps_copy_string(response, apps->GetLaunchQueryParam(request->text)); break;
    case 18: {
        uint64 downloaded = 0, total = 0;
        response->result = apps->GetDlcDownloadProgress(
            request->arg0, &downloaded, &total);
        memcpy(response->data, &downloaded, 8);
        memcpy(response->data + 8, &total, 8);
        response->data_size = 16;
        break;
    }
    case 19:
        response->result = static_cast<uint64_t>(
            static_cast<int64_t>(apps->GetAppBuildId()));
        break;
    case 20: {
        char command[LSTEAM_BRIDGE_MAX_TICKET]{};
        response->result = static_cast<uint64_t>(static_cast<int64_t>(
            apps->GetLaunchCommandLine(command, sizeof(command))));
        apps_copy_string(response, command);
        break;
    }
    case 21: response->result = apps->BIsSubscribedFromFamilySharing(); break;
    case 22: {
        uint32_t allowed = 0, played = 0;
        response->result = apps->BIsTimedTrial(&allowed, &played);
        response->arg0 = allowed;
        response->arg1 = played;
        break;
    }
    case 23: response->result = apps->GetFileDetails(request->text); break;
    default: response->status = -ENOSYS; break;
    }
}
