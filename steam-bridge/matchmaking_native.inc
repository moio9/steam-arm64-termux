#include <atomic>
#include <deque>
#include <new>

static constexpr int32_t LSTEAM_BRIDGE_MM_VISIBLE_SERVER_LIMIT = 64;

struct bridge_mm_event {
    uint32_t kind;
    int32_t index;
    uint32_t response;
};

struct bridge_mm_query;

struct bridge_mm_callback : ISteamMatchmakingServerListResponse {
    bridge_mm_query *query;
    explicit bridge_mm_callback(bridge_mm_query *owner) : query(owner) {}
    void ServerResponded(HServerListRequest, int server) override;
    void ServerFailedToRespond(HServerListRequest, int server) override;
    void RefreshComplete(HServerListRequest, EMatchMakingServerResponse response) override;
};

struct bridge_mm_query {
    uint32_t id = 0;
    HServerListRequest handle = nullptr;
    bridge_mm_callback callback;
    std::atomic_flag event_lock = ATOMIC_FLAG_INIT;
    std::deque<bridge_mm_event> events;

    explicit bridge_mm_query(uint32_t query_id) : id(query_id), callback(this) {}

    void push(uint32_t kind, int32_t index, uint32_t response)
    {
        while (event_lock.test_and_set(std::memory_order_acquire)) {}
        if (events.size() < 8192) events.push_back({kind, index, response});
        event_lock.clear(std::memory_order_release);
    }

    bool pop(bridge_mm_event *event)
    {
        bool available = false;
        while (event_lock.test_and_set(std::memory_order_acquire)) {}
        if (!events.empty()) {
            *event = events.front();
            events.pop_front();
            available = true;
        }
        event_lock.clear(std::memory_order_release);
        return available;
    }
};

void bridge_mm_callback::ServerResponded(HServerListRequest, int server)
{
    if (server >= 0 && server < LSTEAM_BRIDGE_MM_VISIBLE_SERVER_LIMIT)
        query->push(1, server, 0);
}

void bridge_mm_callback::ServerFailedToRespond(HServerListRequest, int server)
{
    if (server >= 0 && server < LSTEAM_BRIDGE_MM_VISIBLE_SERVER_LIMIT)
        query->push(2, server, 0);
}

void bridge_mm_callback::RefreshComplete(HServerListRequest,
                                         EMatchMakingServerResponse response)
{
    query->push(3, -1, static_cast<uint32_t>(response));
}

struct bridge_mm_direct_event {
    uint32_t kind = 0;
    int32_t argument = 0;
    uint32_t value = 0;
    uint32_t data_size = 0;
    uint8_t data[LSTEAM_BRIDGE_MM_DATA_SIZE]{};
};

struct bridge_mm_direct_query;

struct bridge_mm_ping_callback : ISteamMatchmakingPingResponse {
    bridge_mm_direct_query *query;
    explicit bridge_mm_ping_callback(bridge_mm_direct_query *owner) : query(owner) {}
    void ServerResponded(gameserveritem_t &server) override;
    void ServerFailedToRespond() override;
};

struct bridge_mm_players_callback : ISteamMatchmakingPlayersResponse {
    bridge_mm_direct_query *query;
    explicit bridge_mm_players_callback(bridge_mm_direct_query *owner) : query(owner) {}
    void AddPlayerToList(const char *name, int score, float time_played) override;
    void PlayersFailedToRespond() override;
    void PlayersRefreshComplete() override;
};

struct bridge_mm_rules_callback : ISteamMatchmakingRulesResponse {
    bridge_mm_direct_query *query;
    explicit bridge_mm_rules_callback(bridge_mm_direct_query *owner) : query(owner) {}
    void RulesResponded(const char *rule, const char *value) override;
    void RulesFailedToRespond() override;
    void RulesRefreshComplete() override;
};

struct bridge_mm_direct_query {
    uint32_t id = 0;
    uint32_t type = 0;
    HServerQuery handle = HSERVERQUERY_INVALID;
    bridge_mm_ping_callback ping;
    bridge_mm_players_callback players;
    bridge_mm_rules_callback rules;
    std::atomic_flag event_lock = ATOMIC_FLAG_INIT;
    uint32_t event_read = 0;
    uint32_t event_write = 0;
    bridge_mm_direct_event events[128]{};

    bridge_mm_direct_query(uint32_t query_id, uint32_t query_type)
        : id(query_id), type(query_type), ping(this), players(this), rules(this) {}

    void push(uint32_t kind, int32_t argument = 0, uint32_t value = 0,
              const void *data = nullptr, uint32_t data_size = 0)
    {
        while (event_lock.test_and_set(std::memory_order_acquire)) {}
        if (event_write - event_read < 128) {
            auto &event = events[event_write % 128];
            event = {};
            event.kind = kind;
            event.argument = argument;
            event.value = value;
            event.data_size = data_size > sizeof(event.data) ? sizeof(event.data) : data_size;
            if (event.data_size && data) memcpy(event.data, data, event.data_size);
            ++event_write;
        }
        event_lock.clear(std::memory_order_release);
    }

    bool pop(bridge_mm_direct_event *event)
    {
        bool available = false;
        while (event_lock.test_and_set(std::memory_order_acquire)) {}
        if (event_read != event_write) {
            *event = events[event_read % 128];
            ++event_read;
            available = true;
        }
        event_lock.clear(std::memory_order_release);
        return available;
    }
};

void bridge_mm_ping_callback::ServerResponded(gameserveritem_t &server)
{
    query->push(4, 0, 0, &server, sizeof(server));
}

void bridge_mm_ping_callback::ServerFailedToRespond()
{
    query->push(5);
}

void bridge_mm_players_callback::AddPlayerToList(const char *name, int score,
                                                  float time_played)
{
    uint32_t bits = 0;
    memcpy(&bits, &time_played, sizeof(bits));
    char text[LSTEAM_BRIDGE_MM_DATA_SIZE]{};
    snprintf(text, sizeof(text), "%s", name ? name : "");
    query->push(6, score, bits, text, strlen(text) + 1);
}

void bridge_mm_players_callback::PlayersFailedToRespond() { query->push(7); }
void bridge_mm_players_callback::PlayersRefreshComplete() { query->push(8); }

void bridge_mm_rules_callback::RulesResponded(const char *rule, const char *value)
{
    uint8_t text[LSTEAM_BRIDGE_MM_DATA_SIZE]{};
    constexpr size_t half = LSTEAM_BRIDGE_MM_DATA_SIZE / 2;
    snprintf(reinterpret_cast<char *>(text), half, "%s", rule ? rule : "");
    snprintf(reinterpret_cast<char *>(text) + half, half, "%s", value ? value : "");
    query->push(9, 0, 0, text, sizeof(text));
}

void bridge_mm_rules_callback::RulesFailedToRespond() { query->push(10); }
void bridge_mm_rules_callback::RulesRefreshComplete() { query->push(11); }

struct bridge_mm_state {
    ISteamMatchmakingServers *iface = nullptr;
    uint32_t next_id = 1;
    bridge_mm_query *queries[16]{};
    bridge_mm_direct_query *direct_queries[16]{};
};

static bridge_mm_query *find_mm_query(bridge_mm_state *state, uint32_t id)
{
    for (auto *query : state->queries)
        if (query && query->id == id) return query;
    return nullptr;
}

static bridge_mm_direct_query *find_mm_direct_query(bridge_mm_state *state,
                                                     uint32_t id)
{
    for (auto *query : state->direct_queries)
        if (query && query->id == id) return query;
    return nullptr;
}

static void retire_mm_direct_query(bridge_mm_state *state,
                                   bridge_mm_direct_query *query, bool cancel)
{
    if (!query) return;
    if (cancel && state->iface && query->handle != HSERVERQUERY_INVALID)
        state->iface->CancelServerQuery(query->handle);
    for (auto &entry : state->direct_queries)
        if (entry == query) { entry = nullptr; break; }
    delete query;
}

static void fill_mm_direct_response(bridge_mm_state *state,
                                    bridge_mm_direct_query *query,
                                    lsteam_bridge_mm_response *response)
{
    if (!query) return;
    response->query_id = query->id;
    bridge_mm_direct_event event{};
    if (!query->pop(&event)) return;
    response->event_kind = event.kind;
    response->event_index = event.argument;
    response->event_response = event.value;
    response->data_size = event.data_size;
    if (event.data_size) memcpy(response->data, event.data, event.data_size);
    if (event.kind == 4 || event.kind == 5 || event.kind == 7 ||
        event.kind == 8 || event.kind == 10 || event.kind == 11)
        retire_mm_direct_query(state, query, false);
}

static void handle_mm_direct_request(bridge_mm_state *state,
                                     lsteam_bridge_mm_request *request,
                                     lsteam_bridge_mm_response *response)
{
    size_t slot = 16;
    for (size_t i = 0; i < 16; ++i)
        if (!state->direct_queries[i]) { slot = i; break; }
    if (slot == 16) {
        response->status = -ENOSPC;
        return;
    }
    uint32_t id = state->next_id++;
    if (!id) id = state->next_id++;
    auto *query = new (std::nothrow) bridge_mm_direct_query(id, request->method);
    if (!query) {
        response->status = -ENOMEM;
        return;
    }
    state->direct_queries[slot] = query;
    uint32_t ip = request->app_id;
    uint16_t port = static_cast<uint16_t>(request->query_id);
    switch (request->method) {
    case 13: query->handle = state->iface->PingServer(ip, port, &query->ping); break;
    case 14: query->handle = state->iface->PlayerDetails(ip, port, &query->players); break;
    case 15: query->handle = state->iface->ServerRules(ip, port, &query->rules); break;
    }
    if (query->handle == HSERVERQUERY_INVALID) {
        retire_mm_direct_query(state, query, false);
        response->status = -EIO;
        return;
    }
    response->query_id = id;
    fill_mm_direct_response(state, query, response);
}

static void fill_mm_response(bridge_mm_state *state, bridge_mm_query *query,
                             lsteam_bridge_mm_response *response)
{
    if (!query || !query->handle || !state->iface) return;
    response->query_id = query->id;
    response->server_count = state->iface->GetServerCount(query->handle);
    if (response->server_count > LSTEAM_BRIDGE_MM_VISIBLE_SERVER_LIMIT)
        response->server_count = LSTEAM_BRIDGE_MM_VISIBLE_SERVER_LIMIT;
    response->refreshing = state->iface->IsRefreshing(query->handle);
    bridge_mm_event event{};
    if (query->pop(&event)) {
        response->event_kind = event.kind;
        response->event_index = event.index;
        response->event_response = event.response;
    }
}

static void init_mm_response(const lsteam_bridge_request *request,
                             lsteam_bridge_mm_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = request->opcode;
    response->sequence = request->sequence;
}

static void handle_mm_request(ISteamClient *client, HSteamUser user, HSteamPipe pipe,
                              bridge_mm_state *state,
                              const lsteam_bridge_request *header,
                              lsteam_bridge_mm_request *request,
                              lsteam_bridge_mm_response *response)
{
    init_mm_response(header, response);
    if (!((request->method <= 5) ||
          (request->method >= 13 && request->method <= 15)) ||
        request->filter_count > LSTEAM_BRIDGE_MAX_MM_FILTERS) {
        response->status = -EINVAL;
        return;
    }
    /* The native ARM64 client currently asserts on LAN discovery. */
    if (request->method == 1) {
        response->status = -ENOTSUP;
        return;
    }
    if (!state->iface)
        state->iface = client->GetISteamMatchmakingServers(
            user, pipe, STEAMMATCHMAKINGSERVERS_INTERFACE_VERSION);
    if (!state->iface) {
        response->status = -ENODEV;
        return;
    }
    if (request->method >= 13) {
        handle_mm_direct_request(state, request, response);
        return;
    }

    size_t slot = 16;
    for (size_t i = 0; i < 16; ++i)
        if (!state->queries[i]) { slot = i; break; }
    if (slot == 16) {
        response->status = -ENOSPC;
        return;
    }

    uint32_t id = state->next_id++;
    if (!id) id = state->next_id++;
    auto *query = new (std::nothrow) bridge_mm_query(id);
    if (!query) {
        response->status = -ENOMEM;
        return;
    }
    state->queries[slot] = query;

    MatchMakingKeyValuePair_t native_filters[LSTEAM_BRIDGE_MAX_MM_FILTERS]{};
    MatchMakingKeyValuePair_t *filter_ptrs[LSTEAM_BRIDGE_MAX_MM_FILTERS]{};
    for (uint32_t i = 0; i < request->filter_count; ++i) {
        snprintf(native_filters[i].m_szKey, sizeof(native_filters[i].m_szKey),
                 "%.*s", static_cast<int>(sizeof(native_filters[i].m_szKey)) - 1,
                 request->filters[i].key);
        snprintf(native_filters[i].m_szValue, sizeof(native_filters[i].m_szValue),
                 "%.*s", static_cast<int>(sizeof(native_filters[i].m_szValue)) - 1,
                 request->filters[i].value);
        filter_ptrs[i] = &native_filters[i];
    }
    MatchMakingKeyValuePair_t **filters = request->filter_count ? filter_ptrs : nullptr;

    switch (request->method) {
    case 0:
        query->handle = state->iface->RequestInternetServerList(
            request->app_id, filters, request->filter_count, &query->callback);
        break;
    case 2:
        query->handle = state->iface->RequestFriendsServerList(
            request->app_id, filters, request->filter_count, &query->callback);
        break;
    case 3:
        query->handle = state->iface->RequestFavoritesServerList(
            request->app_id, filters, request->filter_count, &query->callback);
        break;
    case 4:
        query->handle = state->iface->RequestHistoryServerList(
            request->app_id, filters, request->filter_count, &query->callback);
        break;
    case 5:
        query->handle = state->iface->RequestSpectatorServerList(
            request->app_id, filters, request->filter_count, &query->callback);
        break;
    default:
        break;
    }
    if (!query->handle) {
        state->queries[slot] = nullptr;
        delete query;
        response->status = -EIO;
        return;
    }
    fill_mm_response(state, query, response);
}

static void handle_mm_call(bridge_mm_state *state,
                           const lsteam_bridge_request *header,
                           lsteam_bridge_mm_request *request,
                           lsteam_bridge_mm_response *response)
{
    init_mm_response(header, response);
    if (request->method == 16) {
        auto *direct = find_mm_direct_query(state, request->query_id);
        if (!state->iface || !direct) response->status = -ENOENT;
        else {
            response->query_id = direct->id;
            retire_mm_direct_query(state, direct, true);
        }
        return;
    }
    if (request->method == 17) {
        if (auto *query = find_mm_query(state, request->query_id))
            fill_mm_response(state, query, response);
        else if (auto *direct = find_mm_direct_query(state, request->query_id))
            fill_mm_direct_response(state, direct, response);
        else response->status = -ENOENT;
        return;
    }
    bridge_mm_query *query = find_mm_query(state, request->query_id);
    if (!state->iface || !query || !query->handle) {
        response->status = -ENOENT;
        return;
    }
    switch (request->method) {
    case 6:
        state->iface->ReleaseRequest(query->handle);
        for (auto &entry : state->queries)
            if (entry == query) { entry = nullptr; break; }
        response->query_id = request->query_id;
        delete query;
        return;
    case 7: {
        gameserveritem_t *item = state->iface->GetServerDetails(
            query->handle, request->argument);
        if (!item) {
            response->status = -ENOENT;
            return;
        }
        static_assert(sizeof(*item) >= LSTEAM_BRIDGE_MM_ITEM_SIZE,
                      "native gameserver item is smaller than SteamMatchMakingServers002");
        memcpy(response->data, item, LSTEAM_BRIDGE_MM_ITEM_SIZE);
        response->data_size = LSTEAM_BRIDGE_MM_ITEM_SIZE;
        break;
    }
    case 8:
        state->iface->CancelQuery(query->handle);
        break;
    case 9:
        state->iface->RefreshQuery(query->handle);
        break;
    case 10: case 11:
        break;
    case 12:
        state->iface->RefreshServer(query->handle, request->argument);
        break;
    default:
        response->status = -ENOSYS;
        return;
    }
    fill_mm_response(state, query, response);
}

static void cleanup_mm_state(bridge_mm_state *state)
{
    if (!state->iface) return;
    for (auto *&query : state->queries) {
        if (!query) continue;
        if (query->handle) state->iface->ReleaseRequest(query->handle);
        delete query;
        query = nullptr;
    }
    for (auto *&query : state->direct_queries) {
        if (!query) continue;
        if (query->handle != HSERVERQUERY_INVALID)
            state->iface->CancelServerQuery(query->handle);
        delete query;
        query = nullptr;
    }
}
