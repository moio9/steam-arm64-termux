#include <algorithm>
#include <vector>

static void handle_network_utils_call(
    ISteamNetworkingUtils *utils,
    lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_NETWORK_UTILS_CALL;
    response->status = 0;
    if (!utils) {
        response->status = -ENODEV;
        return;
    }
    if (request->data_size > sizeof(request->data)) {
        response->status = -EINVAL;
        return;
    }

    switch (request->method) {
    case 0: { /* GetRelayNetworkStatus */
        SteamRelayNetworkStatus_t details{};
        response->result = utils->GetRelayNetworkStatus(
            request->arg0 ? &details : nullptr);
        if (request->arg0) {
            memcpy(response->data, &details, sizeof(details));
            response->data_size = sizeof(details);
        }
        break;
    }
    case 1: { /* GetLocalPingLocation */
        SteamNetworkPingLocation_t location{};
        float age = utils->GetLocalPingLocation(location);
        memcpy(&response->arg0, &age, sizeof(age));
        memcpy(response->data, &location, sizeof(location));
        response->data_size = sizeof(location);
        response->result = 1;
        break;
    }
    case 2: { /* EstimatePingTimeBetweenTwoLocations */
        if (request->data_size != 2 * sizeof(SteamNetworkPingLocation_t)) {
            response->status = -EINVAL;
            break;
        }
        SteamNetworkPingLocation_t locations[2]{};
        memcpy(locations, request->data, sizeof(locations));
        response->result = static_cast<uint64_t>(static_cast<int64_t>(
            utils->EstimatePingTimeBetweenTwoLocations(
                locations[0], locations[1])));
        break;
    }
    case 3: { /* EstimatePingTimeFromLocalHost */
        if (request->data_size != sizeof(SteamNetworkPingLocation_t)) {
            response->status = -EINVAL;
            break;
        }
        SteamNetworkPingLocation_t location{};
        memcpy(&location, request->data, sizeof(location));
        response->result = static_cast<uint64_t>(static_cast<int64_t>(
            utils->EstimatePingTimeFromLocalHost(location)));
        break;
    }
    case 4: { /* ConvertPingLocationToString */
        if (request->data_size != sizeof(SteamNetworkPingLocation_t)) {
            response->status = -EINVAL;
            break;
        }
        SteamNetworkPingLocation_t location{};
        memcpy(&location, request->data, sizeof(location));
        uint32_t capacity = std::min<uint32_t>(
            request->arg0, sizeof(response->data));
        if (capacity)
            utils->ConvertPingLocationToString(
                location, reinterpret_cast<char *>(response->data), capacity);
        response->data_size = capacity
            ? std::min<size_t>(
                strnlen(reinterpret_cast<char *>(response->data), capacity) + 1,
                capacity)
            : 0;
        response->result = 1;
        break;
    }
    case 5: { /* ParsePingLocationString */
        if (!request->data_size || request->data[request->data_size - 1]) {
            response->status = -EINVAL;
            break;
        }
        SteamNetworkPingLocation_t location{};
        response->result = utils->ParsePingLocationString(
            reinterpret_cast<const char *>(request->data), location);
        if (response->result) {
            memcpy(response->data, &location, sizeof(location));
            response->data_size = sizeof(location);
        }
        break;
    }
    case 6: { /* CheckPingDataUpToDate */
        float age = 0;
        memcpy(&age, &request->arg0, sizeof(age));
        response->result = utils->CheckPingDataUpToDate(age);
        break;
    }
    case 7: { /* GetPingToDataCenter */
        SteamNetworkingPOPID relay = 0;
        int result = utils->GetPingToDataCenter(
            request->arg0, request->arg1 ? &relay : nullptr);
        response->result = static_cast<uint64_t>(static_cast<int64_t>(result));
        response->arg0 = relay;
        break;
    }
    case 8:
        response->result = static_cast<uint64_t>(static_cast<int64_t>(
            utils->GetDirectPingToPOP(request->arg0)));
        break;
    case 9:
        response->result = static_cast<uint64_t>(static_cast<int64_t>(
            utils->GetPOPCount()));
        break;
    case 10: { /* GetPOPList */
        uint32_t capacity = std::min<uint32_t>(
            request->arg0, sizeof(response->data) / sizeof(uint32_t));
        std::vector<SteamNetworkingPOPID> list(capacity);
        int count = utils->GetPOPList(capacity ? list.data() : nullptr,
                                      capacity);
        response->result = static_cast<uint64_t>(static_cast<int64_t>(count));
        if (count > 0) {
            uint32_t copied = std::min<uint32_t>(count, capacity);
            memcpy(response->data, list.data(), copied * sizeof(list[0]));
            response->data_size = copied * sizeof(list[0]);
        }
        break;
    }
    case 11:
        response->result = static_cast<uint64_t>(utils->GetLocalTimestamp());
        break;
    case 12:
        response->result = utils->GetIPv4FakeIPType(request->arg0);
        break;
    case 13: { /* GetRealIdentityForFakeIP */
        if (request->data_size != sizeof(SteamNetworkingIPAddr)) {
            response->status = -EINVAL;
            break;
        }
        SteamNetworkingIPAddr address{};
        SteamNetworkingIdentity identity{};
        memcpy(&address, request->data, sizeof(address));
        response->result = utils->GetRealIdentityForFakeIP(address, &identity);
        if (response->result == k_EResultOK) {
            memcpy(response->data, &identity, sizeof(identity));
            response->data_size = sizeof(identity);
        }
        break;
    }
    case 14: { /* SetConfigValue */
        const void *value = request->data_size ? request->data : nullptr;
        uint32_t expected = 0;
        switch (request->arg3) {
        case k_ESteamNetworkingConfig_Int32:
        case k_ESteamNetworkingConfig_Float:
            expected = sizeof(uint32_t);
            break;
        case k_ESteamNetworkingConfig_Int64:
            expected = sizeof(uint64_t);
            break;
        case k_ESteamNetworkingConfig_String:
            if (request->data_size && !request->data[request->data_size - 1])
                expected = request->data_size;
            break;
        case k_ESteamNetworkingConfig_Ptr:
            response->status = -ENOTSUP;
            return;
        default:
            break;
        }
        if (request->data_size && expected != request->data_size) {
            response->status = -EINVAL;
            break;
        }
        response->result = utils->SetConfigValue(
            static_cast<ESteamNetworkingConfigValue>(request->arg0),
            static_cast<ESteamNetworkingConfigScope>(request->arg1),
            static_cast<intptr_t>(static_cast<int32_t>(request->arg2)),
            static_cast<ESteamNetworkingConfigDataType>(request->arg3), value);
        break;
    }
    case 15: { /* GetConfigValue */
        ESteamNetworkingConfigDataType type =
            k_ESteamNetworkingConfig_Int32;
        size_t capacity = std::min<size_t>(
            request->arg3, sizeof(response->data));
        size_t size = capacity;
        void *output = request->steam_id && capacity ? response->data : nullptr;
        response->result = static_cast<uint64_t>(static_cast<int64_t>(
            utils->GetConfigValue(
                static_cast<ESteamNetworkingConfigValue>(request->arg0),
                static_cast<ESteamNetworkingConfigScope>(request->arg1),
                static_cast<intptr_t>(static_cast<int32_t>(request->arg2)),
                &type, output, &size)));
        response->arg0 = type;
        response->arg1 = size > UINT32_MAX ? UINT32_MAX : size;
        if (output) response->data_size = std::min<size_t>(size, capacity);
        break;
    }
    case 16: { /* GetConfigValueInfo */
        ESteamNetworkingConfigDataType type{};
        ESteamNetworkingConfigScope scope{};
        const char *name = utils->GetConfigValueInfo(
            static_cast<ESteamNetworkingConfigValue>(request->arg0),
            request->arg1 & 1 ? &type : nullptr,
            request->arg1 & 2 ? &scope : nullptr);
        response->arg0 = type;
        response->arg1 = scope;
        response->result = name != nullptr;
        if (name) {
            response->data_size = strnlen(name, sizeof(response->data) - 1) + 1;
            memcpy(response->data, name, response->data_size);
        }
        break;
    }
    case 17:
        response->result = utils->IterateGenericEditableConfigValues(
            static_cast<ESteamNetworkingConfigValue>(request->arg0),
            request->arg1 != 0);
        break;
    case 18: { /* IP address to string */
        if (request->data_size != sizeof(SteamNetworkingIPAddr)) {
            response->status = -EINVAL;
            break;
        }
        SteamNetworkingIPAddr address{};
        memcpy(&address, request->data, sizeof(address));
        uint32_t capacity = std::min<uint32_t>(
            request->arg0, sizeof(response->data));
        if (capacity)
            utils->SteamNetworkingIPAddr_ToString(
                address, reinterpret_cast<char *>(response->data), capacity,
                request->arg1 != 0);
        response->data_size = capacity
            ? std::min<size_t>(
                strnlen(reinterpret_cast<char *>(response->data), capacity) + 1,
                capacity)
            : 0;
        response->result = 1;
        break;
    }
    case 19: { /* IP address parse */
        if (!request->data_size || request->data[request->data_size - 1]) {
            response->status = -EINVAL;
            break;
        }
        SteamNetworkingIPAddr address{};
        response->result = utils->SteamNetworkingIPAddr_ParseString(
            &address, reinterpret_cast<const char *>(request->data));
        if (response->result) {
            memcpy(response->data, &address, sizeof(address));
            response->data_size = sizeof(address);
        }
        break;
    }
    case 20: { /* IP address fake type */
        if (request->data_size != sizeof(SteamNetworkingIPAddr)) {
            response->status = -EINVAL;
            break;
        }
        SteamNetworkingIPAddr address{};
        memcpy(&address, request->data, sizeof(address));
        response->result = utils->SteamNetworkingIPAddr_GetFakeIPType(address);
        break;
    }
    case 21: { /* identity to string */
        if (request->data_size != sizeof(SteamNetworkingIdentity)) {
            response->status = -EINVAL;
            break;
        }
        SteamNetworkingIdentity identity{};
        memcpy(&identity, request->data, sizeof(identity));
        uint32_t capacity = std::min<uint32_t>(
            request->arg0, sizeof(response->data));
        if (capacity)
            utils->SteamNetworkingIdentity_ToString(
                identity, reinterpret_cast<char *>(response->data), capacity);
        response->data_size = capacity
            ? std::min<size_t>(
                strnlen(reinterpret_cast<char *>(response->data), capacity) + 1,
                capacity)
            : 0;
        response->result = 1;
        break;
    }
    case 22: { /* identity parse */
        if (!request->data_size || request->data[request->data_size - 1]) {
            response->status = -EINVAL;
            break;
        }
        SteamNetworkingIdentity identity{};
        response->result = utils->SteamNetworkingIdentity_ParseString(
            &identity, reinterpret_cast<const char *>(request->data));
        if (response->result) {
            memcpy(response->data, &identity, sizeof(identity));
            response->data_size = sizeof(identity);
        }
        break;
    }
    default:
        response->status = -ENOSYS;
        break;
    }
}
