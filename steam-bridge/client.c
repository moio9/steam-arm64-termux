#include "protocol.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    const char *socket_path = getenv("LSTEAM_BRIDGE_SOCKET");
    if (!socket_path || !*socket_path) socket_path = LSTEAM_BRIDGE_DEFAULT_SOCKET;
    uint16_t opcode = LSTEAM_BRIDGE_STATUS;
    uint32_t game_server_method = 8;
    uint32_t mm_method = 0;
    uint32_t mm_query = 0;
    int32_t mm_argument = 0;
    uint32_t interface_method = 0;
    uint64_t interface_id = 0;
    uint32_t interface_arg0 = 0;
    uint32_t interface_arg1 = 0, interface_arg2 = 0, interface_arg3 = 0;
    uint8_t interface_data[12] = {0};
    uint32_t interface_data_size = 0;
    const char *interface_text = NULL;
    if (argc > 1 && !strcmp(argv[1], "ping")) opcode = LSTEAM_BRIDGE_PING;
    else if (argc > 1 && !strcmp(argv[1], "stop")) opcode = LSTEAM_BRIDGE_STOP;
    else if (argc > 1 && !strcmp(argv[1], "ticket")) opcode = LSTEAM_BRIDGE_GET_AUTH_TICKET;
    else if (argc > 1 && !strcmp(argv[1], "gameserver-status")) opcode = LSTEAM_BRIDGE_GAME_SERVER_CALL;
    else if (argc > 1 && !strcmp(argv[1], "gameserver-ticket")) {
        opcode = LSTEAM_BRIDGE_GAME_SERVER_CALL;
        game_server_method = 28;
    }
    else if (argc > 1 && !strcmp(argv[1], "gameserver-reputation")) {
        opcode = LSTEAM_BRIDGE_GAME_SERVER_CALL;
        game_server_method = 35;
    }
    else if (argc > 1 && !strcmp(argv[1], "gameserver-ip")) {
        opcode = LSTEAM_BRIDGE_GAME_SERVER_CALL;
        game_server_method = 36;
    }
    else if (argc > 1 && !strcmp(argv[1], "callback")) opcode = LSTEAM_BRIDGE_GET_CALLBACK;
    else if (argc > 1 && !strcmp(argv[1], "callback-free")) opcode = LSTEAM_BRIDGE_FREE_CALLBACK;
    else if (argc > 1 && !strcmp(argv[1], "api-result")) opcode = LSTEAM_BRIDGE_GET_API_CALL_RESULT;
    else if (argc > 1 && !strcmp(argv[1], "mm-request")) {
        opcode = LSTEAM_BRIDGE_MATCHMAKING_REQUEST;
    }
    else if (argc > 3 && !strcmp(argv[1], "mm-ping")) {
        opcode = LSTEAM_BRIDGE_MATCHMAKING_REQUEST;
        mm_method = 13;
        mm_query = strtoul(argv[3], NULL, 10);
    }
    else if (argc > 3 && !strcmp(argv[1], "mm-players")) {
        opcode = LSTEAM_BRIDGE_MATCHMAKING_REQUEST;
        mm_method = 14;
        mm_query = strtoul(argv[3], NULL, 10);
    }
    else if (argc > 3 && !strcmp(argv[1], "mm-rules")) {
        opcode = LSTEAM_BRIDGE_MATCHMAKING_REQUEST;
        mm_method = 15;
        mm_query = strtoul(argv[3], NULL, 10);
    }
    else if (argc > 2 && !strcmp(argv[1], "mm-pump")) {
        opcode = LSTEAM_BRIDGE_MATCHMAKING_CALL;
        mm_method = 17;
        mm_query = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 2 && !strcmp(argv[1], "mm-cancel-server")) {
        opcode = LSTEAM_BRIDGE_MATCHMAKING_CALL;
        mm_method = 16;
        mm_query = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 2 && !strcmp(argv[1], "mm-count")) {
        opcode = LSTEAM_BRIDGE_MATCHMAKING_CALL;
        mm_method = 11;
        mm_query = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 3 && !strcmp(argv[1], "mm-details")) {
        opcode = LSTEAM_BRIDGE_MATCHMAKING_CALL;
        mm_method = 7;
        mm_query = strtoul(argv[2], NULL, 10);
        mm_argument = strtol(argv[3], NULL, 10);
    }
    else if (argc > 2 && !strcmp(argv[1], "mm-release")) {
        opcode = LSTEAM_BRIDGE_MATCHMAKING_CALL;
        mm_method = 6;
        mm_query = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 1 && !strcmp(argv[1], "friends-persona")) {
        opcode = LSTEAM_BRIDGE_FRIENDS_CALL;
    }
    else if (argc > 1 && !strcmp(argv[1], "friends-count")) {
        opcode = LSTEAM_BRIDGE_FRIENDS_CALL;
        interface_method = 2;
        interface_arg0 = 0xffff;
    }
    else if (argc > 2 && !strcmp(argv[1], "friends-name")) {
        opcode = LSTEAM_BRIDGE_FRIENDS_CALL;
        interface_method = 6;
        interface_id = strtoull(argv[2], NULL, 10);
    }
    else if (argc > 2 && !strcmp(argv[1], "friends-has")) {
        opcode = LSTEAM_BRIDGE_FRIENDS_CALL;
        interface_method = 7;
        interface_id = strtoull(argv[2], NULL, 10);
        interface_arg0 = 0xffff;
    }
    else if (argc > 2 && !strcmp(argv[1], "friends-avatar")) {
        opcode = LSTEAM_BRIDGE_FRIENDS_CALL;
        interface_method = 14;
        interface_id = strtoull(argv[2], NULL, 10);
    }
    else if (argc > 3 && !strcmp(argv[1], "user-license")) {
        opcode = LSTEAM_BRIDGE_USER_CALL;
        interface_method = 3;
        interface_id = strtoull(argv[2], NULL, 10);
        interface_arg0 = strtoul(argv[3], NULL, 10);
    }
    else if (argc > 1 && !strcmp(argv[1], "lobby-favorites")) {
        opcode = LSTEAM_BRIDGE_LOBBY_CALL;
    }
    else if (argc > 1 && !strcmp(argv[1], "lobby-request")) {
        opcode = LSTEAM_BRIDGE_LOBBY_CALL;
        interface_method = 4;
    }
    else if (argc > 1 && !strcmp(argv[1], "utils-country")) {
        opcode = LSTEAM_BRIDGE_UTILS_CALL;
        interface_method = 4;
    }
    else if (argc > 1 && !strcmp(argv[1], "utils-time")) {
        opcode = LSTEAM_BRIDGE_UTILS_CALL;
        interface_method = 3;
    }
    else if (argc > 2 && !strcmp(argv[1], "utils-image")) {
        opcode = LSTEAM_BRIDGE_UTILS_CALL;
        interface_method = 0;
        interface_arg0 = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 1 && !strcmp(argv[1], "remote-quota")) {
        opcode = LSTEAM_BRIDGE_REMOTE_STORAGE_CALL;
        interface_method = 20;
    }
    else if (argc > 1 && !strcmp(argv[1], "remote-cloud")) {
        opcode = LSTEAM_BRIDGE_REMOTE_STORAGE_CALL;
        interface_method = 22;
    }
    else if (argc > 2 && !strcmp(argv[1], "remote-size")) {
        opcode = LSTEAM_BRIDGE_REMOTE_STORAGE_CALL;
        interface_method = 15;
        interface_text = argv[2];
    }
    else if (argc > 1 && !strcmp(argv[1], "network-available")) {
        opcode = LSTEAM_BRIDGE_NETWORKING_CALL;
        interface_method = 1;
        interface_arg0 = argc > 2 ? strtoul(argv[2], NULL, 10) : 0;
    }
    else if (argc > 2 && !strcmp(argv[1], "gameserver-stats-request")) {
        opcode = LSTEAM_BRIDGE_GAME_SERVER_STATS_CALL;
        interface_method = 0;
        interface_id = strtoull(argv[2], NULL, 10);
    }
    else if (argc > 1 && !strcmp(argv[1], "userstats-request")) {
        opcode = LSTEAM_BRIDGE_USER_STATS_CALL;
        interface_method = 0;
    }
    else if (argc > 1 && !strcmp(argv[1], "userstats-count")) {
        opcode = LSTEAM_BRIDGE_USER_STATS_CALL;
        interface_method = 14;
    }
    else if (argc > 2 && !strcmp(argv[1], "userstats-name")) {
        opcode = LSTEAM_BRIDGE_USER_STATS_CALL;
        interface_method = 15;
        interface_arg0 = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 2 && !strcmp(argv[1], "ugc-count")) {
        opcode = LSTEAM_BRIDGE_UGC_CALL;
        interface_method = 4;
        interface_arg3 = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 2 && !strcmp(argv[1], "ugc-query")) {
        uint32_t creator = 224260, consumer = 224260, page = 1;
        opcode = LSTEAM_BRIDGE_UGC_CALL;
        interface_method = 0;
        interface_arg0 = 209060904;
        interface_arg1 = 6;
        interface_arg2 = UINT32_MAX;
        interface_arg3 = strtoul(argv[2], NULL, 10);
        interface_data_size = sizeof(interface_data);
        memcpy(interface_data, &creator, 4);
        memcpy(interface_data + 4, &consumer, 4);
        memcpy(interface_data + 8, &page, 4);
    }
    else if (argc > 3 && !strcmp(argv[1], "ugc-send")) {
        opcode = LSTEAM_BRIDGE_UGC_CALL;
        interface_method = 1;
        interface_arg3 = strtoul(argv[2], NULL, 10);
        interface_id = strtoull(argv[3], NULL, 10);
    }
    else if (argc > 3 && !strcmp(argv[1], "ugc-release")) {
        opcode = LSTEAM_BRIDGE_UGC_CALL;
        interface_method = 6;
        interface_arg3 = strtoul(argv[2], NULL, 10);
        interface_id = strtoull(argv[3], NULL, 10);
    }
    else if (argc > 3 && !strcmp(argv[1], "ugc-state")) {
        opcode = LSTEAM_BRIDGE_UGC_CALL;
        interface_method = 8;
        interface_arg3 = strtoul(argv[2], NULL, 10);
        interface_id = strtoull(argv[3], NULL, 10);
    }
    else if (argc > 1 && !strcmp(argv[1], "apps-language")) {
        opcode = LSTEAM_BRIDGE_APPS_CALL;
        interface_method = 4;
    }
    else if (argc > 1 && !strcmp(argv[1], "apps-build")) {
        opcode = LSTEAM_BRIDGE_APPS_CALL;
        interface_method = 19;
    }
    else if (argc > 2 && !strcmp(argv[1], "apps-subscribed")) {
        opcode = LSTEAM_BRIDGE_APPS_CALL;
        interface_method = 6;
        interface_arg0 = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 2 && !strcmp(argv[1], "apps-install-dir")) {
        opcode = LSTEAM_BRIDGE_APPS_CALL;
        interface_method = 14;
        interface_arg0 = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 3 && !strcmp(argv[1], "http-create")) {
        opcode = LSTEAM_BRIDGE_HTTP_CALL;
        interface_method = 0;
        interface_arg0 = strtoul(argv[2], NULL, 10);
        interface_text = argv[3];
    }
    else if (argc > 2 && !strcmp(argv[1], "http-send")) {
        opcode = LSTEAM_BRIDGE_HTTP_CALL;
        interface_method = 5;
        interface_arg0 = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 2 && !strcmp(argv[1], "http-body-size")) {
        opcode = LSTEAM_BRIDGE_HTTP_CALL;
        interface_method = 11;
        interface_arg0 = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 3 && !strcmp(argv[1], "http-body-chunk")) {
        opcode = LSTEAM_BRIDGE_HTTP_CALL;
        interface_method = 12;
        interface_arg0 = strtoul(argv[2], NULL, 10);
        interface_arg1 = strtoul(argv[3], NULL, 10);
    }
    else if (argc > 1 && !strcmp(argv[1], "inventory-all")) {
        opcode = LSTEAM_BRIDGE_INVENTORY_CALL;
        interface_method = 6;
    }
    else if (argc > 2 && !strcmp(argv[1], "inventory-status")) {
        opcode = LSTEAM_BRIDGE_INVENTORY_CALL;
        interface_method = 0;
        interface_arg0 = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 2 && !strcmp(argv[1], "inventory-item-count")) {
        opcode = LSTEAM_BRIDGE_INVENTORY_CALL;
        interface_method = 1;
        interface_arg0 = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 1 && !strcmp(argv[1], "inventory-load-definitions")) {
        opcode = LSTEAM_BRIDGE_INVENTORY_CALL;
        interface_method = 20;
    }
    else if (argc > 1 && !strcmp(argv[1], "inventory-definition-count")) {
        opcode = LSTEAM_BRIDGE_INVENTORY_CALL;
        interface_method = 21;
    }
    else if (argc > 1 && !strcmp(argv[1], "inventory-price-count")) {
        opcode = LSTEAM_BRIDGE_INVENTORY_CALL;
        interface_method = 27;
    }
    else if (argc > 1 && !strcmp(argv[1], "game-search-cancel")) {
        opcode = LSTEAM_BRIDGE_GAME_SEARCH_CALL;
        interface_method = 11;
    }
    else if (argc > 1 && !strcmp(argv[1], "app-list-count")) {
        opcode = LSTEAM_BRIDGE_APP_LIST_CALL;
        interface_method = 0;
    }
    else if (argc > 2 && !strcmp(argv[1], "app-list-build")) {
        opcode = LSTEAM_BRIDGE_APP_LIST_CALL;
        interface_method = 4;
        interface_arg0 = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 1 && !strcmp(argv[1], "video-broadcasting")) {
        opcode = LSTEAM_BRIDGE_VIDEO_CALL;
        interface_method = 1;
    }
    else if (argc > 1 && !strcmp(argv[1], "parental-status")) {
        opcode = LSTEAM_BRIDGE_PARENTAL_CALL;
        interface_method = 0;
    }
    else if (argc > 2 && !strcmp(argv[1], "parental-app-blocked")) {
        opcode = LSTEAM_BRIDGE_PARENTAL_CALL;
        interface_method = 2;
        interface_arg0 = strtoul(argv[2], NULL, 10);
    }
    else if (argc > 1 && !strcmp(argv[1], "network-messages-receive")) {
        opcode = LSTEAM_BRIDGE_NETWORK_MESSAGES_CALL;
        interface_method = 1;
    }
    else if (argc > 1 && !strcmp(argv[1], "client-bind-any")) {
        opcode = LSTEAM_BRIDGE_CLIENT_CALL;
        interface_method = 0;
        interface_data_size = sizeof(uint32_t);
    }
    else if (argc > 1 && !strcmp(argv[1], "html-init")) {
        opcode = LSTEAM_BRIDGE_HTML_CALL;
        interface_method = 0;
    }
    else if (argc > 1 && !strcmp(argv[1], "html-create")) {
        opcode = LSTEAM_BRIDGE_HTML_CALL;
        interface_method = 2;
        interface_text = "lsteambridge-smoke";
    }
    else if (argc > 1 && strcmp(argv[1], "status")) {
        fprintf(stderr, "usage: %s [status|ping|stop|ticket|client-bind-any|gameserver-status|gameserver-ticket|gameserver-reputation|gameserver-ip|callback|callback-free|api-result]\n", argv[0]);
        return 2;
    }
    if (strlen(socket_path) >= sizeof(((struct sockaddr_un *)0)->sun_path)) return 2;

    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        perror("lsteambridge-client: socket");
        return 2;
    }
    struct sockaddr_un address = {0};
    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, socket_path, strlen(socket_path) + 1);
    if (connect(fd, (struct sockaddr *)&address,
                offsetof(struct sockaddr_un, sun_path) + strlen(socket_path) + 1) < 0) {
        perror("lsteambridge-client: connect");
        return 1;
    }

    struct lsteam_bridge_request request = {
        LSTEAM_BRIDGE_MAGIC, LSTEAM_BRIDGE_VERSION, opcode, 1
    };
    struct lsteam_bridge_response response = {0};
    if (send(fd, &request, sizeof(request), MSG_NOSIGNAL) != sizeof(request)) {
        perror("lsteambridge-client: exchange");
        return 1;
    }
    if (opcode == LSTEAM_BRIDGE_GET_AUTH_TICKET) {
        struct lsteam_bridge_ticket_response ticket = {0};
        if (recv(fd, &ticket, sizeof(ticket), 0) != sizeof(ticket)) {
            perror("lsteambridge-client: ticket");
            return 1;
        }
        close(fd);
        if (ticket.magic != LSTEAM_BRIDGE_MAGIC ||
            ticket.version != LSTEAM_BRIDGE_VERSION ||
            ticket.sequence != request.sequence ||
            ticket.ticket_size > LSTEAM_BRIDGE_MAX_TICKET) {
            fprintf(stderr, "lsteambridge-client: invalid ticket response\n");
            return 1;
        }
        printf("status=%d ticket_handle=%u ticket_size=%u\n",
               ticket.status, ticket.ticket_handle, ticket.ticket_size);
        return ticket.status || !ticket.ticket_handle || !ticket.ticket_size;
    }
    if (opcode == LSTEAM_BRIDGE_MATCHMAKING_REQUEST ||
        opcode == LSTEAM_BRIDGE_MATCHMAKING_CALL) {
        struct lsteam_bridge_mm_request mm = {0};
        struct lsteam_bridge_mm_response mm_response = {0};
        mm.method = mm_method;
        mm.app_id = argc > 2 && opcode == LSTEAM_BRIDGE_MATCHMAKING_REQUEST ?
                    strtoul(argv[2], NULL, 10) : 224260;
        mm.query_id = mm_query;
        mm.argument = mm_argument;
        if (send(fd, &mm, sizeof(mm), MSG_NOSIGNAL) != sizeof(mm) ||
            recv(fd, &mm_response, sizeof(mm_response), 0) != sizeof(mm_response)) {
            perror("lsteambridge-client: matchmaking");
            return 1;
        }
        close(fd);
        if (mm_response.magic != LSTEAM_BRIDGE_MAGIC ||
            mm_response.version != LSTEAM_BRIDGE_VERSION ||
            mm_response.sequence != request.sequence) {
            fprintf(stderr, "lsteambridge-client: invalid matchmaking response\n");
            return 1;
        }
        printf("status=%d method=%u query=%u count=%d refreshing=%u event=%u index=%d data=%u",
               mm_response.status, mm_method, mm_response.query_id,
               mm_response.server_count, mm_response.refreshing,
               mm_response.event_kind, mm_response.event_index,
               mm_response.data_size);
        if (mm_response.data_size == LSTEAM_BRIDGE_MM_ITEM_SIZE) {
            uint16_t connection_port = 0, query_port = 0;
            uint32_t ip = 0;
            memcpy(&connection_port, mm_response.data, sizeof(connection_port));
            memcpy(&query_port, mm_response.data + 2, sizeof(query_port));
            memcpy(&ip, mm_response.data + 4, sizeof(ip));
            printf(" ip=%u conn=%u queryport=%u gamedir=%.32s map=%.32s server=%.64s",
                   ip, connection_port, query_port, mm_response.data + 14,
                   mm_response.data + 46, mm_response.data + 172);
        } else if (mm_response.event_kind == 6) {
            float time_played = 0.0f;
            memcpy(&time_played, &mm_response.event_response, sizeof(time_played));
            printf(" player=%.128s score=%d time=%.1f", mm_response.data,
                   mm_response.event_index, time_played);
        } else if (mm_response.event_kind == 9) {
            printf(" rule=%.256s value=%.256s", mm_response.data,
                   mm_response.data + LSTEAM_BRIDGE_MM_DATA_SIZE / 2);
        }
        putchar('\n');
        return mm_response.status ? 1 : 0;
    }
    if (opcode == LSTEAM_BRIDGE_GAME_SERVER_CALL) {
        struct lsteam_bridge_game_server_request game = {0};
        struct lsteam_bridge_game_server_response game_response = {0};
        game.method = game_server_method;
        if (send(fd, &game, sizeof(game), MSG_NOSIGNAL) != sizeof(game) ||
            recv(fd, &game_response, sizeof(game_response), 0) != sizeof(game_response)) {
            perror("lsteambridge-client: gameserver");
            return 1;
        }
        close(fd);
        if (game_response.magic != LSTEAM_BRIDGE_MAGIC ||
            game_response.version != LSTEAM_BRIDGE_VERSION ||
            game_response.sequence != request.sequence) {
            fprintf(stderr, "lsteambridge-client: invalid gameserver response\n");
            return 1;
        }
        printf("status=%d method=%u result=%llu steam_id=%llu arg0=%u size=%u\n",
               game_response.status, game_server_method,
               (unsigned long long)game_response.result,
               (unsigned long long)game_response.steam_id,
               game_response.arg0, game_response.data_size);
        return game_response.status ? 1 : 0;
    }
    if (opcode == LSTEAM_BRIDGE_CLIENT_CALL ||
        opcode == LSTEAM_BRIDGE_USER_CALL ||
        opcode == LSTEAM_BRIDGE_FRIENDS_CALL ||
        opcode == LSTEAM_BRIDGE_UTILS_CALL ||
        opcode == LSTEAM_BRIDGE_REMOTE_STORAGE_CALL ||
        opcode == LSTEAM_BRIDGE_NETWORKING_CALL ||
        opcode == LSTEAM_BRIDGE_GAME_SERVER_STATS_CALL ||
        opcode == LSTEAM_BRIDGE_USER_STATS_CALL ||
        opcode == LSTEAM_BRIDGE_UGC_CALL ||
        opcode == LSTEAM_BRIDGE_APPS_CALL ||
        opcode == LSTEAM_BRIDGE_HTTP_CALL ||
        opcode == LSTEAM_BRIDGE_INVENTORY_CALL ||
        opcode == LSTEAM_BRIDGE_GAME_SEARCH_CALL ||
        opcode == LSTEAM_BRIDGE_APP_LIST_CALL ||
        opcode == LSTEAM_BRIDGE_VIDEO_CALL ||
        opcode == LSTEAM_BRIDGE_PARENTAL_CALL ||
        opcode == LSTEAM_BRIDGE_NETWORK_MESSAGES_CALL ||
        opcode == LSTEAM_BRIDGE_NETWORK_UTILS_CALL ||
        opcode == LSTEAM_BRIDGE_HTML_CALL ||
        opcode == LSTEAM_BRIDGE_LOBBY_CALL) {
        struct lsteam_bridge_game_server_request interface_request = {0};
        struct lsteam_bridge_game_server_response interface_response = {0};
        interface_request.method = interface_method;
        interface_request.steam_id = interface_id;
        interface_request.arg0 = interface_arg0;
        interface_request.arg1 = interface_arg1;
        interface_request.arg2 = interface_arg2;
        interface_request.arg3 = interface_arg3;
        interface_request.data_size = interface_data_size;
        if (interface_data_size)
            memcpy(interface_request.data, interface_data,
                   interface_data_size);
        if (interface_text)
            snprintf(interface_request.text, sizeof(interface_request.text),
                     "%s", interface_text);
        if (send(fd, &interface_request, sizeof(interface_request),
                 MSG_NOSIGNAL) != sizeof(interface_request) ||
            recv(fd, &interface_response, sizeof(interface_response), 0) !=
                 sizeof(interface_response)) {
            perror("lsteambridge-client: interface");
            return 1;
        }
        close(fd);
        if (interface_response.magic != LSTEAM_BRIDGE_MAGIC ||
            interface_response.version != LSTEAM_BRIDGE_VERSION ||
            interface_response.opcode != opcode ||
            interface_response.sequence != request.sequence ||
            interface_response.data_size > LSTEAM_BRIDGE_MAX_TICKET) {
            fprintf(stderr, "lsteambridge-client: invalid interface response\n");
            return 1;
        }
        printf("status=%d method=%u result=%llu steam_id=%llu arg0=%u size=%u",
               interface_response.status, interface_method,
               (unsigned long long)interface_response.result,
               (unsigned long long)interface_response.steam_id,
               interface_response.arg0,
               interface_response.data_size);
        if (interface_response.data_size)
            printf(" text=%.*s", (int)interface_response.data_size,
                   interface_response.data);
        putchar('\n');
        return interface_response.status ? 1 : 0;
    }
    if (opcode == LSTEAM_BRIDGE_GET_CALLBACK) {
        struct lsteam_bridge_callback_response callback = {0};
        if (recv(fd, &callback, sizeof(callback), 0) != sizeof(callback)) {
            perror("lsteambridge-client: callback");
            return 1;
        }
        close(fd);
        if (callback.magic != LSTEAM_BRIDGE_MAGIC ||
            callback.version != LSTEAM_BRIDGE_VERSION ||
            callback.sequence != request.sequence) {
            fprintf(stderr, "lsteambridge-client: invalid callback response\n");
            return 1;
        }
        printf("status=%d callback_id=%d size=%u steam_call=%d\n",
               callback.status, callback.callback_id, callback.data_size,
               callback.steam_call);
        return callback.status == -EAGAIN ? 0 : callback.status != 0;
    }
    if (opcode == LSTEAM_BRIDGE_GET_API_CALL_RESULT) {
        struct lsteam_bridge_api_call_request api = {1, 16, 703};
        struct lsteam_bridge_api_call_response api_response = {0};
        if (send(fd, &api, sizeof(api), MSG_NOSIGNAL) != sizeof(api) ||
            recv(fd, &api_response, sizeof(api_response), 0) != sizeof(api_response)) {
            perror("lsteambridge-client: API result");
            return 1;
        }
        close(fd);
        if (api_response.magic != LSTEAM_BRIDGE_MAGIC ||
            api_response.version != LSTEAM_BRIDGE_VERSION ||
            api_response.sequence != request.sequence ||
            api_response.data_size > LSTEAM_BRIDGE_MAX_CALLBACK) {
            fprintf(stderr, "lsteambridge-client: invalid API result response\n");
            return 1;
        }
        printf("status=%d failed=%u size=%u first_byte=%02x\n",
               api_response.status, api_response.failed, api_response.data_size,
               api_response.data_size ? api_response.data[0] : 0);
        return api_response.status != 0;
    }
    if (recv(fd, &response, sizeof(response), 0) != sizeof(response)) {
        perror("lsteambridge-client: exchange");
        return 1;
    }
    close(fd);
    if (response.magic != LSTEAM_BRIDGE_MAGIC ||
        response.version != LSTEAM_BRIDGE_VERSION ||
        response.sequence != request.sequence) {
        fprintf(stderr, "lsteambridge-client: invalid response\n");
        return 1;
    }
    printf("status=%d server_pid=%u uid=%u pipe=%d user=%d logged_on=%s steam_id=%llu\n",
           response.status, response.server_pid, response.server_uid,
           response.steam_pipe, response.steam_user,
           response.logged_on ? "yes" : "no",
           (unsigned long long)response.steam_id);
    return response.status ? 1 : 0;
}
