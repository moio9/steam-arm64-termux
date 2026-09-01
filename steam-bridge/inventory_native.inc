#include <algorithm>
#include <string>
#include <vector>

static constexpr uint32_t LSTEAM_BRIDGE_INVENTORY_MAX_ENTRIES = 65536u;
static constexpr uint32_t LSTEAM_BRIDGE_INVENTORY_MAX_BLOB = 64u * 1024u * 1024u;

struct bridge_inventory_state {
    uint32_t items_handle = 0;
    uint32_t items_capacity = 0;
    bool items_wanted = false;
    bool items_valid = false;
    bool items_result = false;
    uint32_t items_total = 0;
    std::vector<lsteam_bridge_inventory_item> items;

    uint32_t string_method = 0;
    int32_t string_key = 0;
    uint32_t string_index = 0;
    bool string_name_present = false;
    std::string string_name;
    bool string_valid = false;
    bool string_result = false;
    std::vector<uint8_t> string_value;

    int32_t serialized_handle = 0;
    bool serialized_valid = false;
    bool serialized_result = false;
    std::vector<uint8_t> serialized;

    uint32_t ids_method = 0;
    uint64_t ids_steam_id = 0;
    bool ids_valid = false;
    bool ids_result = false;
    std::vector<int32_t> ids;

    bool prices_valid = false;
    bool prices_result = false;
    std::vector<lsteam_bridge_inventory_price> prices;

    uint32_t upload_method = 0;
    uint32_t upload_total = 0;
    uint32_t upload_received = 0;
    std::vector<uint8_t> upload;
};

static void inventory_copy_bytes(const void *data, size_t size, uint32_t offset,
                                 lsteam_bridge_game_server_response *response)
{
    response->arg1 = size;
    if (offset > size) {
        response->status = -EINVAL;
        return;
    }
    size_t count = size - offset;
    if (count > sizeof(response->data)) count = sizeof(response->data);
    if (count) memcpy(response->data, static_cast<const uint8_t *>(data) + offset,
                      count);
    response->data_size = count;
}

static void inventory_copy_records(
    const void *data, size_t size, size_t record_size, uint32_t offset,
    lsteam_bridge_game_server_response *response)
{
    response->arg1 = size;
    if (!record_size || offset > size || offset % record_size ||
        size % record_size) {
        response->status = -EINVAL;
        return;
    }
    size_t count = size - offset;
    size_t chunk = sizeof(response->data) / record_size * record_size;
    if (count > chunk) count = chunk;
    if (count) memcpy(response->data, static_cast<const uint8_t *>(data) + offset,
                      count);
    response->data_size = count;
}

static bool inventory_receive_upload(
    bridge_inventory_state *state, uint32_t method, uint32_t total,
    uint32_t offset, const uint8_t *data, uint32_t size,
    lsteam_bridge_game_server_response *response)
{
    if (total > LSTEAM_BRIDGE_INVENTORY_MAX_BLOB || offset > total ||
        size > total - offset) {
        response->status = total > LSTEAM_BRIDGE_INVENTORY_MAX_BLOB
            ? -EFBIG : -EINVAL;
        return false;
    }
    if (!offset) {
        state->upload_method = method;
        state->upload_total = total;
        state->upload_received = 0;
        state->upload.assign(total, 0);
    }
    if (state->upload_method != method || state->upload_total != total ||
        state->upload_received != offset) {
        response->status = -EINVAL;
        return false;
    }
    if (size) memcpy(state->upload.data() + offset, data, size);
    state->upload_received += size;
    return state->upload_received == total;
}

static void inventory_load_string(
    ISteamInventory *inventory, bridge_inventory_state *state,
    uint32_t method, int32_t key, uint32_t index, bool name_present,
    const char *name, lsteam_bridge_game_server_response *response)
{
    bool same = state->string_valid && state->string_method == method &&
        state->string_key == key && state->string_index == index &&
        state->string_name_present == name_present &&
        state->string_name == (name_present ? name : "");
    if (!same) {
        state->string_method = method;
        state->string_key = key;
        state->string_index = index;
        state->string_name_present = name_present;
        state->string_name = name_present ? name : "";
        state->string_valid = true;
        uint32_t size = 0;
        if (method == 2)
            state->string_result = inventory->GetResultItemProperty(
                key, index, name_present ? name : nullptr, nullptr, &size);
        else
            state->string_result = inventory->GetItemDefinitionProperty(
                key, name_present ? name : nullptr, nullptr, &size);
        if (size > LSTEAM_BRIDGE_INVENTORY_MAX_BLOB) {
            state->string_result = false;
            response->status = -EFBIG;
            return;
        }
        state->string_value.assign(size, 0);
        if (state->string_result && size) {
            uint32_t actual = size;
            if (method == 2)
                state->string_result = inventory->GetResultItemProperty(
                    key, index, name_present ? name : nullptr,
                    reinterpret_cast<char *>(state->string_value.data()), &actual);
            else
                state->string_result = inventory->GetItemDefinitionProperty(
                    key, name_present ? name : nullptr,
                    reinterpret_cast<char *>(state->string_value.data()), &actual);
            if (actual < state->string_value.size())
                state->string_value.resize(actual);
        }
    }
    response->result = state->string_result;
    response->arg0 = state->string_value.size();
}

static void handle_inventory_call(
    ISteamInventory *inventory, bridge_inventory_state *state,
    lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_INVENTORY_CALL;
    response->status = 0;
    request->text[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    if (!inventory) {
        response->status = -ENODEV;
        return;
    }

    switch (request->method) {
    case 0:
        response->result = inventory->GetResultStatus(
            static_cast<int32_t>(request->arg0));
        break;
    case 1: {
        uint32_t capacity = request->arg1;
        uint32_t offset = request->arg2;
        bool wanted = request->arg3 != 0;
        if (capacity > LSTEAM_BRIDGE_INVENTORY_MAX_ENTRIES) {
            response->status = -EFBIG;
            break;
        }
        if (!offset || !state->items_valid ||
            state->items_handle != request->arg0 ||
            state->items_capacity != capacity || state->items_wanted != wanted) {
            state->items_handle = request->arg0;
            state->items_capacity = capacity;
            state->items_wanted = wanted;
            state->items_valid = true;
            std::vector<SteamItemDetails_t> native(wanted ? capacity : 0);
            uint32_t count = capacity;
            state->items_result = inventory->GetResultItems(
                static_cast<int32_t>(request->arg0),
                wanted && capacity ? native.data() : nullptr, &count);
            state->items_total = count;
            uint32_t copied = wanted ? std::min(capacity, count) : 0;
            state->items.resize(copied);
            for (uint32_t i = 0; i < copied; ++i) {
                state->items[i].item_id = native[i].m_itemId;
                state->items[i].definition = native[i].m_iDefinition;
                state->items[i].quantity = native[i].m_unQuantity;
                state->items[i].flags = native[i].m_unFlags;
            }
        }
        response->result = state->items_result;
        response->arg0 = state->items_total;
        if (wanted)
            inventory_copy_bytes(state->items.data(),
                state->items.size() * sizeof(state->items[0]), offset, response);
        break;
    }
    case 2:
    case 22:
        inventory_load_string(inventory, state, request->method,
            static_cast<int32_t>(request->arg0), request->arg1,
            request->arg3 != 0, request->text, response);
        if (!response->status && response->result)
            inventory_copy_bytes(state->string_value.data(),
                state->string_value.size(), request->arg2, response);
        break;
    case 3:
        response->result = inventory->GetResultTimestamp(
            static_cast<int32_t>(request->arg0));
        break;
    case 4:
        response->result = inventory->CheckResultSteamID(
            static_cast<int32_t>(request->arg0),
            CSteamID(static_cast<uint64>(request->steam_id)));
        break;
    case 5:
        inventory->DestroyResult(static_cast<int32_t>(request->arg0));
        response->result = 1;
        break;
    case 6: {
        SteamInventoryResult_t result = k_SteamInventoryResultInvalid;
        response->result = inventory->GetAllItems(&result);
        response->arg0 = result;
        break;
    }
    case 7: {
        uint32_t count = request->arg2;
        if (count > LSTEAM_BRIDGE_INVENTORY_MAX_ENTRIES) {
            response->status = -EFBIG;
            break;
        }
        uint32_t total = count * sizeof(uint64_t);
        bool complete = inventory_receive_upload(state, request->method, total,
            request->arg1, request->data, request->data_size, response);
        if (!complete || response->status) break;
        SteamInventoryResult_t result = k_SteamInventoryResultInvalid;
        response->result = inventory->GetItemsByID(&result,
            count ? reinterpret_cast<const SteamItemInstanceID_t *>(state->upload.data()) : nullptr,
            count);
        response->arg0 = result;
        break;
    }
    case 8: {
        uint32_t offset = request->arg2;
        if (!offset || !state->serialized_valid ||
            state->serialized_handle != static_cast<int32_t>(request->arg0)) {
            state->serialized_handle = request->arg0;
            state->serialized_valid = true;
            uint32_t size = 0;
            state->serialized_result = inventory->SerializeResult(
                state->serialized_handle, nullptr, &size);
            if (size > LSTEAM_BRIDGE_INVENTORY_MAX_BLOB) {
                state->serialized_result = false;
                response->status = -EFBIG;
                break;
            }
            state->serialized.assign(size, 0);
            if (state->serialized_result && size) {
                uint32_t actual = size;
                state->serialized_result = inventory->SerializeResult(
                    state->serialized_handle, state->serialized.data(), &actual);
                if (actual < state->serialized.size())
                    state->serialized.resize(actual);
            }
        }
        response->result = state->serialized_result;
        response->arg0 = state->serialized.size();
        if (state->serialized_result && request->arg3)
            inventory_copy_bytes(state->serialized.data(),
                state->serialized.size(), offset, response);
        break;
    }
    case 9: {
        uint32_t total = request->arg2;
        bool complete = inventory_receive_upload(state, request->method, total,
            request->arg1, request->data, request->data_size, response);
        if (!complete || response->status) break;
        SteamInventoryResult_t result = k_SteamInventoryResultInvalid;
        response->result = inventory->DeserializeResult(&result,
            total ? state->upload.data() : nullptr, total, request->arg3 != 0);
        response->arg0 = result;
        break;
    }
    case 20:
        response->result = inventory->LoadItemDefinitions();
        break;
    case 21:
    case 24: {
        uint32_t method = request->method;
        if (!request->arg2 || !state->ids_valid || state->ids_method != method ||
            state->ids_steam_id != request->steam_id) {
            state->ids_method = method;
            state->ids_steam_id = request->steam_id;
            state->ids_valid = true;
            uint32_t count = 0;
            state->ids_result = method == 21
                ? inventory->GetItemDefinitionIDs(nullptr, &count)
                : inventory->GetEligiblePromoItemDefinitionIDs(
                    CSteamID(static_cast<uint64>(request->steam_id)), nullptr,
                    &count);
            if (count > LSTEAM_BRIDGE_INVENTORY_MAX_ENTRIES) {
                state->ids_result = false;
                response->status = -EFBIG;
                break;
            }
            state->ids.assign(count, 0);
            if (state->ids_result && count) {
                uint32_t actual = count;
                state->ids_result = method == 21
                    ? inventory->GetItemDefinitionIDs(state->ids.data(), &actual)
                    : inventory->GetEligiblePromoItemDefinitionIDs(
                        CSteamID(static_cast<uint64>(request->steam_id)),
                        state->ids.data(), &actual);
                if (actual < state->ids.size()) state->ids.resize(actual);
            }
        }
        response->result = state->ids_result;
        response->arg0 = state->ids.size();
        if (state->ids_result && request->arg3)
            inventory_copy_bytes(state->ids.data(),
                state->ids.size() * sizeof(state->ids[0]), request->arg2,
                response);
        break;
    }
    case 23:
        response->result = inventory->RequestEligiblePromoItemDefinitionsIDs(
            CSteamID(static_cast<uint64>(request->steam_id)));
        break;
    case 26:
        response->result = inventory->RequestPrices();
        break;
    case 27:
        response->result = inventory->GetNumItemsWithPrices();
        break;
    case 28: {
        uint32_t offset = request->arg2;
        if (!offset || !state->prices_valid) {
            state->prices_valid = true;
            uint32_t count = inventory->GetNumItemsWithPrices();
            if (count > LSTEAM_BRIDGE_INVENTORY_MAX_ENTRIES) {
                response->status = -EFBIG;
                state->prices_result = false;
                break;
            }
            std::vector<SteamItemDef_t> definitions(count);
            std::vector<uint64> current(count), base(count);
            state->prices_result = inventory->GetItemsWithPrices(
                definitions.data(), current.data(), base.data(), count);
            state->prices.resize(count);
            for (uint32_t i = 0; i < count; ++i) {
                state->prices[i].definition = definitions[i];
                state->prices[i].current_price = current[i];
                state->prices[i].base_price = base[i];
            }
        }
        response->result = state->prices_result;
        response->arg0 = state->prices.size();
        if (state->prices_result && request->arg3)
            inventory_copy_records(state->prices.data(),
                state->prices.size() * sizeof(state->prices[0]),
                sizeof(state->prices[0]), offset, response);
        break;
    }
    case 29: {
        uint64 current = 0, base = 0;
        response->result = inventory->GetItemPrice(
            static_cast<int32_t>(request->arg0), &current, &base);
        memcpy(response->data, &current, sizeof(current));
        memcpy(response->data + sizeof(current), &base, sizeof(base));
        response->data_size = 16;
        break;
    }
    case 37: {
        SteamInventoryResult_t result = k_SteamInventoryResultInvalid;
        response->result = inventory->InspectItem(&result, request->text);
        response->arg0 = result;
        break;
    }
    default:
        response->status = -ENOSYS;
        break;
    }
}
