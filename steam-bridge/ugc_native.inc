template <typename R, typename... Args>
static R ugc_call(void *iface, size_t slot, Args... args)
{
    void **vtable = *reinterpret_cast<void ***>(iface);
    using method_t = R (*)(void *, Args...);
    return reinterpret_cast<method_t>(vtable[slot])(iface, args...);
}

#include <vector>

struct bridge_ugc_state {
    uint32_t details_version = 0;
    uint64_t details_query = 0;
    uint32_t details_index = 0;
    bool details_result = false;
    lsteam_bridge_ugc_details details{};
    uint32_t subscribed_version = 0;
    uint32_t subscribed_maximum = 0;
    std::vector<uint64_t> subscribed;
};

static void ugc_pack_details(const SteamUGCDetails_t &source,
                             lsteam_bridge_ugc_details *target)
{
    memset(target, 0, sizeof(*target));
    target->published_file_id = source.m_nPublishedFileId;
    target->result = source.m_eResult;
    target->file_type = source.m_eFileType;
    target->creator_app_id = source.m_nCreatorAppID;
    target->consumer_app_id = source.m_nConsumerAppID;
    memcpy(target->title, source.m_rgchTitle, sizeof(target->title));
    memcpy(target->description, source.m_rgchDescription,
           sizeof(target->description));
    target->owner_steam_id = source.m_ulSteamIDOwner;
    target->time_created = source.m_rtimeCreated;
    target->time_updated = source.m_rtimeUpdated;
    target->time_added_to_user_list = source.m_rtimeAddedToUserList;
    target->visibility = source.m_eVisibility;
    target->banned = source.m_bBanned;
    target->accepted_for_use = source.m_bAcceptedForUse;
    target->tags_truncated = source.m_bTagsTruncated;
    memcpy(target->tags, source.m_rgchTags, sizeof(target->tags));
    target->file_handle = source.m_hFile;
    target->preview_file_handle = source.m_hPreviewFile;
    memcpy(target->file_name, source.m_pchFileName,
           sizeof(target->file_name));
    target->file_size = source.m_nFileSize;
    target->preview_file_size = source.m_nPreviewFileSize;
    memcpy(target->url, source.m_rgchURL, sizeof(target->url));
    target->votes_up = source.m_unVotesUp;
    target->votes_down = source.m_unVotesDown;
    target->score = source.m_flScore;
    target->child_count = source.m_unNumChildren;
}

static void handle_ugc_call(
    void *ugc14, void *ugc15, bridge_ugc_state *state,
    lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_UGC_CALL;
    response->status = 0;
    uint32_t version = request->arg3;
    void *ugc = version == 14 ? ugc14 : version == 15 ? ugc15 : nullptr;
    if (!ugc) {
        response->status = -ENODEV;
        return;
    }

    switch (request->method) {
    case 0: {
        uint32_t creator = 0, consumer = 0, page = 0;
        if (request->data_size != 12) {
            response->status = -EINVAL;
            break;
        }
        memcpy(&creator, request->data, 4);
        memcpy(&consumer, request->data + 4, 4);
        memcpy(&page, request->data + 8, 4);
        response->result = ugc_call<uint64_t>(
            ugc, 0, request->arg0, request->arg1, request->arg2,
            static_cast<uint32_t>(request->steam_id), creator, consumer, page);
        break;
    }
    case 1:
        response->result = ugc_call<uint64_t>(ugc, 4, request->steam_id);
        break;
    case 2:
        response->result = ugc_call<bool>(
            ugc, version == 14 ? 21 : 24, request->steam_id,
            request->arg0 != 0);
        break;
    case 3:
        response->result = ugc_call<bool>(
            ugc, version == 14 ? 22 : 25, request->steam_id,
            request->arg0 != 0);
        break;
    case 4:
        response->result = ugc_call<uint32_t>(
            ugc, version == 14 ? 62 : 65);
        break;
    case 5: {
        uint32_t offset = request->arg1;
        if (offset >= sizeof(state->details)) {
            response->status = -EINVAL;
            break;
        }
        if (!offset || state->details_version != version ||
            state->details_query != request->steam_id ||
            state->details_index != request->arg0) {
            SteamUGCDetails_t details{};
            state->details_version = version;
            state->details_query = request->steam_id;
            state->details_index = request->arg0;
            state->details_result = ugc_call<bool>(
                ugc, 5, request->steam_id, request->arg0, &details);
            ugc_pack_details(details, &state->details);
        }
        uint32_t chunk = sizeof(state->details) - offset;
        if (chunk > sizeof(response->data)) chunk = sizeof(response->data);
        memcpy(response->data,
               reinterpret_cast<const uint8_t *>(&state->details) + offset,
               chunk);
        response->data_size = chunk;
        response->arg0 = sizeof(state->details);
        response->result = state->details_result;
        break;
    }
    case 6:
        response->result = ugc_call<bool>(
            ugc, version == 14 ? 15 : 18, request->steam_id);
        break;
    case 7: {
        uint32_t maximum = request->arg0;
        uint32_t offset = request->arg1;
        if (maximum > 4096 || offset > maximum) {
            response->status = -EINVAL;
            break;
        }
        if (!offset || state->subscribed_version != version ||
            state->subscribed_maximum != maximum) {
            state->subscribed_version = version;
            state->subscribed_maximum = maximum;
            state->subscribed.assign(maximum, 0);
            uint32_t count = ugc_call<uint32_t>(
                ugc, version == 14 ? 63 : 66,
                state->subscribed.data(), maximum);
            if (count > maximum) count = maximum;
            state->subscribed.resize(count);
        }
        if (offset > state->subscribed.size()) {
            response->status = -EINVAL;
            break;
        }
        uint32_t count = state->subscribed.size() - offset;
        uint32_t chunk_count = sizeof(response->data) / sizeof(uint64_t);
        if (count > chunk_count) count = chunk_count;
        if (count)
            memcpy(response->data, state->subscribed.data() + offset,
                   count * sizeof(uint64_t));
        response->data_size = count * sizeof(uint64_t);
        response->arg0 = state->subscribed.size();
        response->result = state->subscribed.size();
        break;
    }
    case 8:
        response->result = ugc_call<uint32_t>(
            ugc, version == 14 ? 64 : 67, request->steam_id);
        break;
    case 9: {
        uint64_t size = 0;
        uint32_t timestamp = 0;
        char folder[LSTEAM_BRIDGE_MAX_TICKET]{};
        response->result = ugc_call<bool>(
            ugc, version == 14 ? 65 : 68, request->steam_id, &size,
            folder, static_cast<uint32_t>(sizeof(folder)), &timestamp);
        memcpy(response->data, &size, sizeof(size));
        size_t folder_size = strnlen(folder, sizeof(folder) - 1) + 1;
        memcpy(response->data + sizeof(size), folder, folder_size);
        response->data_size = sizeof(size) + folder_size;
        response->arg0 = timestamp;
        break;
    }
    case 10: {
        uint64_t downloaded = 0, total = 0;
        response->result = ugc_call<bool>(
            ugc, version == 14 ? 66 : 69, request->steam_id,
            &downloaded, &total);
        memcpy(response->data, &downloaded, sizeof(downloaded));
        memcpy(response->data + 8, &total, sizeof(total));
        response->data_size = 16;
        break;
    }
    case 11:
        response->result = ugc_call<bool>(
            ugc, version == 14 ? 67 : 70, request->steam_id,
            request->arg0 != 0);
        break;
    default:
        response->status = -ENOSYS;
        break;
    }
}
