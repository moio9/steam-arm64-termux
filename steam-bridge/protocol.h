#ifndef LSTEAM_BRIDGE_PROTOCOL_H
#define LSTEAM_BRIDGE_PROTOCOL_H

#include <stdint.h>

#define LSTEAM_BRIDGE_MAGIC 0x4c535042u /* LSPB */
#define LSTEAM_BRIDGE_VERSION 5u
#define LSTEAM_BRIDGE_DEFAULT_SOCKET "/data/data/com.termux/files/usr/tmp/lsteambridge.sock"

enum lsteam_bridge_opcode {
    LSTEAM_BRIDGE_PING = 1,
    LSTEAM_BRIDGE_STATUS = 2,
    LSTEAM_BRIDGE_STOP = 3,
    LSTEAM_BRIDGE_GET_AUTH_TICKET = 4,
    LSTEAM_BRIDGE_INIT_GAME_CONNECTION = 5,
    LSTEAM_BRIDGE_GAME_SERVER_CALL = 6,
    LSTEAM_BRIDGE_GET_CALLBACK = 7,
    LSTEAM_BRIDGE_FREE_CALLBACK = 8,
    LSTEAM_BRIDGE_GET_API_CALL_RESULT = 9,
    LSTEAM_BRIDGE_MATCHMAKING_REQUEST = 10,
    LSTEAM_BRIDGE_MATCHMAKING_CALL = 11,
    LSTEAM_BRIDGE_USER_CALL = 12,
    LSTEAM_BRIDGE_FRIENDS_CALL = 13,
    LSTEAM_BRIDGE_UTILS_CALL = 14,
    LSTEAM_BRIDGE_REMOTE_STORAGE_CALL = 15,
    LSTEAM_BRIDGE_LOBBY_CALL = 16,
    LSTEAM_BRIDGE_NETWORKING_CALL = 17,
    LSTEAM_BRIDGE_GAME_SERVER_STATS_CALL = 18,
    LSTEAM_BRIDGE_USER_STATS_CALL = 19,
    LSTEAM_BRIDGE_UGC_CALL = 20,
    LSTEAM_BRIDGE_APPS_CALL = 21,
    LSTEAM_BRIDGE_HTTP_CALL = 22,
    LSTEAM_BRIDGE_INVENTORY_CALL = 23,
    LSTEAM_BRIDGE_GAME_SEARCH_CALL = 24,
    LSTEAM_BRIDGE_APP_LIST_CALL = 25,
    LSTEAM_BRIDGE_VIDEO_CALL = 26,
    LSTEAM_BRIDGE_PARENTAL_CALL = 27,
    LSTEAM_BRIDGE_NETWORK_MESSAGES_CALL = 28,
    LSTEAM_BRIDGE_NETWORK_SOCKETS_CALL = 29,
    LSTEAM_BRIDGE_NETWORK_UTILS_CALL = 30,
    LSTEAM_BRIDGE_CLIENT_CALL = 31,
    LSTEAM_BRIDGE_VOICE_CALL = 32,
    LSTEAM_BRIDGE_HTML_CALL = 33,
    LSTEAM_BRIDGE_APP_TICKET_CALL = 34,
};

struct lsteam_bridge_request {
    uint32_t magic;
    uint16_t version;
    uint16_t opcode;
    uint32_t sequence;
};

struct lsteam_bridge_game_connection_request {
    int32_t max_blob;
    uint32_t server_ip;
    uint64_t server_steam_id;
    uint16_t server_port;
    uint8_t secure;
    uint8_t reserved[5];
};

struct lsteam_bridge_response {
    uint32_t magic;
    uint16_t version;
    uint16_t opcode;
    uint32_t sequence;
    int32_t status;
    uint32_t server_pid;
    uint32_t server_uid;
    int32_t steam_pipe;
    int32_t steam_user;
    uint64_t steam_id;
    uint8_t logged_on;
    uint8_t reserved[7];
};

#define LSTEAM_BRIDGE_MAX_TICKET 2048u
struct lsteam_bridge_app_ticket_request {
    uint32_t app_id;
    uint32_t capacity;
};

struct lsteam_bridge_app_ticket_response {
    uint32_t magic;
    uint16_t version;
    uint16_t opcode;
    uint32_t sequence;
    int32_t status;
    uint32_t returned_size;
    uint32_t app_id_offset;
    uint32_t steam_id_offset;
    uint32_t signature_offset;
    uint32_t signature_size;
    uint8_t ticket[LSTEAM_BRIDGE_MAX_TICKET];
};

struct lsteam_bridge_ticket_response {
    uint32_t magic;
    uint16_t version;
    uint16_t opcode;
    uint32_t sequence;
    int32_t status;
    uint32_t ticket_handle;
    uint32_t ticket_size;
    uint8_t ticket[LSTEAM_BRIDGE_MAX_TICKET];
};

struct lsteam_bridge_blob_response {
    uint32_t magic;
    uint16_t version;
    uint16_t opcode;
    uint32_t sequence;
    int32_t status;
    uint32_t blob_size;
    uint8_t blob[LSTEAM_BRIDGE_MAX_TICKET];
};

#define LSTEAM_BRIDGE_MAX_CALLBACK 8192u
struct lsteam_bridge_callback_response {
    uint32_t magic;
    uint16_t version;
    uint16_t opcode;
    uint32_t sequence;
    int32_t status;
    int32_t steam_user;
    int32_t callback_id;
    int32_t steam_call;
    uint32_t data_size;
    uint8_t data[LSTEAM_BRIDGE_MAX_CALLBACK];
};

struct lsteam_bridge_api_call_request {
    uint64_t call;
    int32_t callback_size;
    int32_t callback_id;
};

struct lsteam_bridge_api_call_response {
    uint32_t magic;
    uint16_t version;
    uint16_t opcode;
    uint32_t sequence;
    int32_t status;
    uint8_t failed;
    uint8_t reserved[3];
    uint32_t data_size;
    uint8_t data[LSTEAM_BRIDGE_MAX_CALLBACK];
};

#define LSTEAM_BRIDGE_MAX_MM_FILTERS 16u
#define LSTEAM_BRIDGE_MM_TEXT 256u
#define LSTEAM_BRIDGE_MM_ITEM_SIZE 372u
#define LSTEAM_BRIDGE_MM_DATA_SIZE 1024u

struct lsteam_bridge_mm_filter {
    char key[LSTEAM_BRIDGE_MM_TEXT];
    char value[LSTEAM_BRIDGE_MM_TEXT];
};

struct lsteam_bridge_mm_request {
    uint32_t method;
    uint32_t app_id;
    uint32_t query_id;
    int32_t argument;
    uint32_t filter_count;
    struct lsteam_bridge_mm_filter filters[LSTEAM_BRIDGE_MAX_MM_FILTERS];
};

struct lsteam_bridge_mm_response {
    uint32_t magic;
    uint16_t version;
    uint16_t opcode;
    uint32_t sequence;
    int32_t status;
    uint32_t query_id;
    int32_t server_count;
    uint32_t refreshing;
    uint32_t event_kind;
    int32_t event_index;
    uint32_t event_response;
    uint32_t data_size;
    uint8_t data[LSTEAM_BRIDGE_MM_DATA_SIZE];
};

#define LSTEAM_BRIDGE_MAX_TEXT 512u
struct lsteam_bridge_game_server_request {
    uint32_t method;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t arg2;
    uint32_t arg3;
    uint64_t steam_id;
    uint32_t data_size;
    char text[LSTEAM_BRIDGE_MAX_TEXT];
    char value[LSTEAM_BRIDGE_MAX_TEXT];
    uint8_t data[LSTEAM_BRIDGE_MAX_TICKET];
};

struct lsteam_bridge_game_server_response {
    uint32_t magic;
    uint16_t version;
    uint16_t opcode;
    uint32_t sequence;
    int32_t status;
    uint64_t result;
    uint64_t steam_id;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t data_size;
    uint8_t data[LSTEAM_BRIDGE_MAX_TICKET];
};

#define LSTEAM_BRIDGE_MAX_VOICE 65536u
struct lsteam_bridge_voice_request {
    uint32_t method;
    uint32_t flags;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t arg2;
    uint32_t data_size;
    uint8_t data[LSTEAM_BRIDGE_MAX_VOICE];
};

struct lsteam_bridge_voice_response {
    uint32_t magic;
    uint16_t version;
    uint16_t opcode;
    uint32_t sequence;
    int32_t status;
    uint32_t result;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t data_size;
    uint8_t data[LSTEAM_BRIDGE_MAX_VOICE];
};

/* Stable packed form shared by SteamUGC014/015. */
#pragma pack(push, 4)
struct lsteam_bridge_ugc_details {
    uint64_t published_file_id;
    uint32_t result;
    uint32_t file_type;
    uint32_t creator_app_id;
    uint32_t consumer_app_id;
    char title[129];
    char description[8000];
    uint64_t owner_steam_id;
    uint32_t time_created;
    uint32_t time_updated;
    uint32_t time_added_to_user_list;
    uint32_t visibility;
    uint8_t banned;
    uint8_t accepted_for_use;
    uint8_t tags_truncated;
    char tags[1025];
    uint64_t file_handle;
    uint64_t preview_file_handle;
    char file_name[260];
    int32_t file_size;
    int32_t preview_file_size;
    char url[256];
    uint32_t votes_up;
    uint32_t votes_down;
    float score;
    uint32_t child_count;
};

struct lsteam_bridge_inventory_item {
    uint64_t item_id;
    int32_t definition;
    uint16_t quantity;
    uint16_t flags;
};

struct lsteam_bridge_inventory_price {
    int32_t definition;
    uint64_t current_price;
    uint64_t base_price;
};

struct lsteam_bridge_network_message_header {
    uint8_t identity[136];
    uint32_t connection;
    int64_t connection_user_data;
    int64_t time_received;
    int64_t message_number;
    int32_t channel;
    int32_t flags;
    int64_t user_data;
    uint16_t lane;
    uint16_t reserved;
};
#pragma pack(pop)

#ifdef __cplusplus
static_assert(sizeof(lsteam_bridge_request) == 12, "bridge request ABI changed");
static_assert(sizeof(lsteam_bridge_game_connection_request) == 24, "bridge game request ABI changed");
static_assert(sizeof(lsteam_bridge_response) == 48, "bridge response ABI changed");
static_assert(sizeof(lsteam_bridge_app_ticket_request) == 8, "bridge app ticket request ABI changed");
static_assert(sizeof(lsteam_bridge_app_ticket_response) == 2084, "bridge app ticket response ABI changed");
static_assert(sizeof(lsteam_bridge_ticket_response) == 2072, "bridge ticket ABI changed");
static_assert(sizeof(lsteam_bridge_blob_response) == 2068, "bridge blob ABI changed");
static_assert(sizeof(lsteam_bridge_callback_response) == 8224, "bridge callback ABI changed");
static_assert(sizeof(lsteam_bridge_api_call_request) == 16, "bridge API request ABI changed");
static_assert(sizeof(lsteam_bridge_api_call_response) == 8216, "bridge API response ABI changed");
static_assert(sizeof(lsteam_bridge_ugc_details) == 9764, "bridge UGC details ABI changed");
static_assert(sizeof(lsteam_bridge_inventory_item) == 16, "bridge inventory item ABI changed");
static_assert(sizeof(lsteam_bridge_inventory_price) == 20, "bridge inventory price ABI changed");
static_assert(sizeof(lsteam_bridge_network_message_header) == 184, "bridge network message ABI changed");
static_assert(sizeof(lsteam_bridge_voice_request) == 65560, "bridge voice request ABI changed");
static_assert(sizeof(lsteam_bridge_voice_response) == 65568, "bridge voice response ABI changed");
#else
_Static_assert(sizeof(struct lsteam_bridge_request) == 12, "bridge request ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_game_connection_request) == 24, "bridge game request ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_response) == 48, "bridge response ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_app_ticket_request) == 8, "bridge app ticket request ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_app_ticket_response) == 2084, "bridge app ticket response ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_ticket_response) == 2072, "bridge ticket ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_blob_response) == 2068, "bridge blob ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_callback_response) == 8224, "bridge callback ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_api_call_request) == 16, "bridge API request ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_api_call_response) == 8216, "bridge API response ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_ugc_details) == 9764, "bridge UGC details ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_inventory_item) == 16, "bridge inventory item ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_inventory_price) == 20, "bridge inventory price ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_network_message_header) == 184, "bridge network message ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_voice_request) == 65560, "bridge voice request ABI changed");
_Static_assert(sizeof(struct lsteam_bridge_voice_response) == 65568, "bridge voice response ABI changed");
#endif

#endif
