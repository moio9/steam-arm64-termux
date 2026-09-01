#include <string>
#include <vector>

struct bridge_remote_storage_state {
    std::string write_name;
    uint32_t write_total = 0;
    std::vector<uint8_t> write_data;
    std::string read_name;
    uint32_t read_capacity = 0;
    int32_t read_size = 0;
    std::vector<uint8_t> read_data;
};

static void remote_copy_string(lsteam_bridge_game_server_response *response,
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

static void handle_remote_storage_call(
    ISteamRemoteStorage *storage, bridge_remote_storage_state *state,
    lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    constexpr uint32_t maximum_file = 64u * 1024u * 1024u;
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_REMOTE_STORAGE_CALL;
    response->status = 0;
    request->text[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    if (!storage) {
        response->status = -ENODEV;
        return;
    }
    if (request->data_size > LSTEAM_BRIDGE_MAX_TICKET) {
        response->status = -EINVAL;
        return;
    }

    switch (request->method) {
    case 0: {
        uint32_t total = request->arg0, offset = request->arg1;
        if (total > maximum_file || offset > total ||
            request->data_size > total - offset) {
            response->status = -EINVAL;
            break;
        }
        if (!offset) {
            state->write_name = request->text;
            state->write_total = total;
            state->write_data.assign(total, 0);
        }
        if (state->write_name != request->text ||
            state->write_total != total || state->write_data.size() != total) {
            response->status = -EINVAL;
            break;
        }
        if (request->data_size)
            memcpy(state->write_data.data() + offset, request->data,
                   request->data_size);
        response->result = 1;
        if (offset + request->data_size == total) {
            response->result = storage->FileWrite(
                request->text, total ? state->write_data.data() : nullptr,
                static_cast<int32_t>(total));
            state->write_data.clear();
            state->write_name.clear();
            state->write_total = 0;
        }
        break;
    }
    case 1: {
        uint32_t capacity = request->arg0, offset = request->arg1;
        if (capacity > maximum_file || offset > capacity) {
            response->status = -EINVAL;
            break;
        }
        if (!offset || state->read_name != request->text ||
            state->read_capacity != capacity) {
            state->read_name = request->text;
            state->read_capacity = capacity;
            state->read_data.assign(capacity, 0);
            state->read_size = storage->FileRead(
                request->text, capacity ? state->read_data.data() : nullptr,
                static_cast<int32_t>(capacity));
        }
        if (state->read_size < 0 ||
            static_cast<uint32_t>(state->read_size) > capacity ||
            offset > static_cast<uint32_t>(state->read_size)) {
            response->status = -EIO;
            break;
        }
        size_t chunk = static_cast<uint32_t>(state->read_size) - offset;
        if (chunk > sizeof(response->data)) chunk = sizeof(response->data);
        if (chunk) memcpy(response->data, state->read_data.data() + offset, chunk);
        response->data_size = chunk;
        response->arg0 = state->read_size;
        response->result = state->read_size;
        break;
    }
    case 5:
        response->result = storage->FileForget(request->text);
        break;
    case 6:
        response->result = storage->FileDelete(request->text);
        break;
    case 7:
        response->result = storage->FileShare(request->text);
        break;
    case 8:
        response->result = storage->SetSyncPlatforms(
            request->text, static_cast<ERemoteStoragePlatform>(request->arg0));
        break;
    case 9:
        response->result = storage->FileWriteStreamOpen(request->text);
        break;
    case 10:
        response->result = storage->FileWriteStreamWriteChunk(
            request->steam_id, request->data,
            static_cast<int32_t>(request->data_size));
        break;
    case 11:
        response->result = storage->FileWriteStreamClose(request->steam_id);
        break;
    case 12:
        response->result = storage->FileWriteStreamCancel(request->steam_id);
        break;
    case 13:
        response->result = storage->FileExists(request->text);
        break;
    case 14:
        response->result = storage->FilePersisted(request->text);
        break;
    case 15:
        response->result = static_cast<uint64_t>(
            static_cast<int64_t>(storage->GetFileSize(request->text)));
        break;
    case 16:
        response->result = static_cast<uint64_t>(
            storage->GetFileTimestamp(request->text));
        break;
    case 17:
        response->result = storage->GetSyncPlatforms(request->text);
        break;
    case 18:
        response->result = storage->GetFileCount();
        break;
    case 19: {
        int32_t size = 0;
        const char *name = storage->GetFileNameAndSize(
            static_cast<int>(request->arg0), &size);
        response->arg0 = size;
        remote_copy_string(response, name);
        break;
    }
    case 20: {
        uint64 total = 0, available = 0;
        response->result = storage->GetQuota(&total, &available);
        memcpy(response->data, &total, sizeof(total));
        memcpy(response->data + sizeof(total), &available, sizeof(available));
        response->data_size = sizeof(total) + sizeof(available);
        break;
    }
    case 21:
        response->result = storage->IsCloudEnabledForAccount();
        break;
    case 22:
        response->result = storage->IsCloudEnabledForApp();
        break;
    case 23:
        storage->SetCloudEnabledForApp(request->arg0 != 0);
        break;
    default:
        response->status = -ENOSYS;
        break;
    }
}
