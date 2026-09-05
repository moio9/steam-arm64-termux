#include "protocol.h"

#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define STEAM_API_NODLL
#include "isteamclient.h"
#include "isteamuser.h"
#include "isteamfriends.h"
#include "isteamutils.h"
#include "isteammatchmaking.h"
#include "isteamremotestorage.h"
#include "isteamnetworking.h"
#include "isteamgameserverstats.h"
#include "isteamuserstats.h"
#include "isteamugc.h"
#include "isteamapps.h"
#include "isteamhttp.h"
#include "isteaminventory.h"
#include "isteamapplist.h"
#include "isteamvideo.h"
#include "isteamparentalsettings.h"
#include "isteamnetworkingmessages.h"
#include "isteamnetworkingsockets.h"
#include "steamnetworkingfakeip.h"
#include "isteamnetworkingutils.h"
#include "isteamhtmlsurface.h"
#include "isteamappticket.h"
#include "client_native.inc"
#include "gameserver_native.inc"
#include "gameserver_stats_native.inc"
#include "user_stats_native.inc"
#include "ugc_native.inc"
#include "apps_native.inc"
#include "http_native.inc"
#include "inventory_native.inc"
#include "gamesearch_native.inc"
#include "applist_native.inc"
#include "video_native.inc"
#include "parental_native.inc"
#include "networking_messages_native.inc"
#include "networking_sockets_native.inc"
#include "networking_utils_native.inc"
#include "user_native.inc"
#include "voice_native.inc"
#include "html_native.inc"
#include "friends_native.inc"
#include "utils_native.inc"
#include "remote_storage_native.inc"
#include "networking_native.inc"
#include "lobby_native.inc"
#include "matchmaking_native.inc"

using CreatePipeFn = HSteamPipe (*)(void);
using ConnectUserFn = HSteamUser (*)(HSteamPipe);
using CreateLocalUserFn = HSteamUser (*)(HSteamPipe *, EAccountType);
using LoggedOnFn = bool (*)(HSteamUser, HSteamPipe);
using ReleaseUserFn = void (*)(HSteamPipe, HSteamUser);
using ReleasePipeFn = bool (*)(HSteamPipe);
using CreateInterfaceFn = void *(*)(const char *, int *);

struct native_callback_msg {
    HSteamUser steam_user;
    int32_t callback_id;
    uint8_t *data;
    int32_t data_size;
};
using BGetCallbackFn = bool (*)(HSteamPipe, native_callback_msg *, int32_t *);
using FreeLastCallbackFn = void (*)(HSteamPipe);
using GetAPICallResultFn = bool (*)(HSteamPipe, uint64_t, void *, int32_t,
                                    int32_t, bool *);

static volatile sig_atomic_t running = 1;

static void stop_server(int) { running = 0; }
static uint32_t callback_registered(int32_t) { return 1; }

static bool app_ticket_range_valid(uint32_t offset, uint32_t size,
                                   uint32_t total)
{
    return offset <= total && size <= total - offset;
}

template <typename T>
static T require_symbol(void *module, const char *name)
{
    void *p = dlsym(module, name);
    if (!p) {
        fprintf(stderr, "lsteambridge: missing %s: %s\n", name, dlerror());
        exit(2);
    }
    return reinterpret_cast<T>(p);
}

static bool safe_remove_socket(const char *path)
{
    struct stat st;
    if (lstat(path, &st) < 0) return errno == ENOENT;
    if (!S_ISSOCK(st.st_mode) || st.st_uid != getuid()) return false;
    return unlink(path) == 0;
}

int main(int argc, char **argv)
{
    const char *workdir = getenv("LSTEAM_BRIDGE_WORKDIR");
    if (workdir && *workdir && chdir(workdir) < 0) {
        perror("lsteambridge: chdir");
        return 2;
    }
    const char *library = argc > 1 ? argv[1] : "./steamrtarm64/steamclient-patched.so";
    const char *socket_path = getenv("LSTEAM_BRIDGE_SOCKET");
    if (!socket_path || !*socket_path) socket_path = LSTEAM_BRIDGE_DEFAULT_SOCKET;
    if (strlen(socket_path) >= sizeof(sockaddr_un::sun_path)) {
        fprintf(stderr, "lsteambridge: socket path is too long\n");
        return 2;
    }

    void *module = dlopen(library, RTLD_NOW | RTLD_LOCAL);
    if (!module) {
        fprintf(stderr, "lsteambridge: dlopen failed: %s\n", dlerror());
        return 2;
    }
    auto create_pipe = require_symbol<CreatePipeFn>(module, "Steam_CreateSteamPipe");
    auto connect_user = require_symbol<ConnectUserFn>(module, "Steam_ConnectToGlobalUser");
    auto create_local_user = require_symbol<CreateLocalUserFn>(module, "Steam_CreateLocalUser");
    auto logged_on = require_symbol<LoggedOnFn>(module, "Steam_BLoggedOn");
    auto release_user = require_symbol<ReleaseUserFn>(module, "Steam_ReleaseUser");
    auto release_pipe = require_symbol<ReleasePipeFn>(module, "Steam_BReleaseSteamPipe");
    auto create_interface = require_symbol<CreateInterfaceFn>(module, "CreateInterface");
    auto get_callback = require_symbol<BGetCallbackFn>(module, "Steam_BGetCallback");
    auto free_callback = require_symbol<FreeLastCallbackFn>(module, "Steam_FreeLastCallback");
    auto get_api_call_result = require_symbol<GetAPICallResultFn>(
        module, "Steam_GetAPICallResult");

    HSteamPipe pipe = create_pipe();
    HSteamUser user = pipe ? connect_user(pipe) : 0;
    if (!pipe || !user) {
        fprintf(stderr, "lsteambridge: Steam session is unavailable\n");
        dlclose(module);
        return 1;
    }

    uint64_t steam_id = 0;
    ISteamUser *steam_user_iface = nullptr;
    ISteamFriends *steam_friends_iface = nullptr;
    ISteamUtils *steam_utils_iface = nullptr;
    ISteamRemoteStorage *steam_remote_storage_iface = nullptr;
    ISteamNetworking *steam_networking_iface = nullptr;
    ISteamUserStats *steam_user_stats_iface = nullptr;
    ISteamApps *steam_apps_iface = nullptr;
    ISteamHTTP *steam_http_iface = nullptr;
    ISteamInventory *steam_inventory_iface = nullptr;
    ISteamGameSearch *steam_game_search_iface = nullptr;
    ISteamAppList *steam_app_list_iface = nullptr;
    ISteamVideo *steam_video_iface = nullptr;
    ISteamParentalSettings *steam_parental_iface = nullptr;
    ISteamNetworkingMessages *steam_network_messages_iface = nullptr;
    ISteamNetworkingSockets *steam_network_sockets_iface = nullptr;
    ISteamNetworkingUtils *steam_network_utils_iface = nullptr;
    ISteamHTMLSurface *steam_html_iface = nullptr;
    ISteamAppTicket *steam_app_ticket_iface = nullptr;
    void *steam_ugc14_iface = nullptr;
    void *steam_ugc15_iface = nullptr;
    ISteamMatchmaking *steam_lobby_iface = nullptr;
    bridge_utils_state utils_state;
    bridge_remote_storage_state remote_storage_state;
    bridge_networking_state networking_state;
    bridge_game_server_state game_server_state;
    bridge_game_server_stats_state game_server_stats_state;
    bridge_ugc_state ugc_state;
    bridge_http_state http_state;
    bridge_inventory_state inventory_state;
    bridge_app_list_state app_list_state;
    bridge_network_messages_state network_messages_state;
    bridge_network_sockets_state network_sockets_state;
    bridge_mm_state matchmaking_state;
    bridge_html_state html_state;
    HSteamPipe callback_pipe = 0;
    game_server_state.create_local_user = create_local_user;
    int interface_result = 0;
    auto client = static_cast<ISteamClient *>(create_interface(STEAMCLIENT_INTERFACE_VERSION, &interface_result));
    if (client) {
        client->Set_SteamAPI_CCheckCallbackRegisteredInProcess(
            callback_registered);
        steam_user_iface = client->GetISteamUser(user, pipe, STEAMUSER_INTERFACE_VERSION);
        if (steam_user_iface) steam_id = steam_user_iface->GetSteamID().ConvertToUint64();
        steam_friends_iface = client->GetISteamFriends(
            user, pipe, STEAMFRIENDS_INTERFACE_VERSION);
        steam_utils_iface = client->GetISteamUtils(
            pipe, STEAMUTILS_INTERFACE_VERSION);
        steam_remote_storage_iface = client->GetISteamRemoteStorage(
            user, pipe, STEAMREMOTESTORAGE_INTERFACE_VERSION);
        steam_networking_iface = client->GetISteamNetworking(
            user, pipe, STEAMNETWORKING_INTERFACE_VERSION);
        steam_user_stats_iface = client->GetISteamUserStats(
            user, pipe, STEAMUSERSTATS_INTERFACE_VERSION);
        steam_apps_iface = client->GetISteamApps(
            user, pipe, STEAMAPPS_INTERFACE_VERSION);
        steam_http_iface = client->GetISteamHTTP(
            user, pipe, STEAMHTTP_INTERFACE_VERSION);
        steam_inventory_iface = client->GetISteamInventory(
            user, pipe, STEAMINVENTORY_INTERFACE_VERSION);
        steam_game_search_iface = client->GetISteamGameSearch(
            user, pipe, STEAMGAMESEARCH_INTERFACE_VERSION);
        steam_app_list_iface = client->GetISteamAppList(
            user, pipe, STEAMAPPLIST_INTERFACE_VERSION);
        steam_video_iface = client->GetISteamVideo(
            user, pipe, STEAMVIDEO_INTERFACE_VERSION);
        steam_parental_iface = client->GetISteamParentalSettings(
            user, pipe, STEAMPARENTALSETTINGS_INTERFACE_VERSION);
        steam_network_messages_iface = static_cast<ISteamNetworkingMessages *>(
            client->GetISteamGenericInterface(
                user, pipe, STEAMNETWORKINGMESSAGES_INTERFACE_VERSION));
        if (!steam_network_messages_iface)
            steam_network_messages_iface =
                static_cast<ISteamNetworkingMessages *>(create_interface(
                    STEAMNETWORKINGMESSAGES_INTERFACE_VERSION,
                    &interface_result));
        steam_network_sockets_iface = static_cast<ISteamNetworkingSockets *>(
            client->GetISteamGenericInterface(
                user, pipe, STEAMNETWORKINGSOCKETS_INTERFACE_VERSION));
        if (!steam_network_sockets_iface)
            steam_network_sockets_iface =
                static_cast<ISteamNetworkingSockets *>(create_interface(
                    STEAMNETWORKINGSOCKETS_INTERFACE_VERSION,
                    &interface_result));
        steam_network_utils_iface = static_cast<ISteamNetworkingUtils *>(
            client->GetISteamGenericInterface(
                user, pipe, STEAMNETWORKINGUTILS_INTERFACE_VERSION));
        if (!steam_network_utils_iface)
            steam_network_utils_iface =
                static_cast<ISteamNetworkingUtils *>(create_interface(
                    STEAMNETWORKINGUTILS_INTERFACE_VERSION,
                    &interface_result));
        steam_ugc14_iface = client->GetISteamUGC(
            user, pipe, "STEAMUGC_INTERFACE_VERSION014");
        steam_ugc15_iface = client->GetISteamUGC(
            user, pipe, "STEAMUGC_INTERFACE_VERSION015");
        steam_lobby_iface = client->GetISteamMatchmaking(
            user, pipe, STEAMMATCHMAKING_INTERFACE_VERSION);
        steam_html_iface = client->GetISteamHTMLSurface(
            user, pipe, STEAMHTMLSURFACE_INTERFACE_VERSION);
        steam_app_ticket_iface = static_cast<ISteamAppTicket *>(
            client->GetISteamGenericInterface(
                user, pipe, STEAMAPPTICKET_INTERFACE_VERSION));
    }
    if (!steam_app_ticket_iface)
        steam_app_ticket_iface = static_cast<ISteamAppTicket *>(
            create_interface(STEAMAPPTICKET_INTERFACE_VERSION,
                             &interface_result));

    int listener = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (listener < 0) {
        perror("lsteambridge: socket");
        return 2;
    }
    if (!safe_remove_socket(socket_path)) {
        fprintf(stderr, "lsteambridge: refusing to replace unsafe path: %s\n", socket_path);
        return 2;
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, socket_path, strlen(socket_path) + 1);
    mode_t old_umask = umask(0077);
    int bind_result = bind(listener, reinterpret_cast<sockaddr *>(&address),
                           offsetof(sockaddr_un, sun_path) + strlen(socket_path) + 1);
    umask(old_umask);
    if (bind_result < 0 || chmod(socket_path, 0600) < 0 || listen(listener, 8) < 0) {
        perror("lsteambridge: bind/listen");
        safe_remove_socket(socket_path);
        return 2;
    }

    signal(SIGTERM, stop_server);
    signal(SIGINT, stop_server);
    const char *app_id = getenv("SteamAppId");
    printf("lsteambridge: ready socket=%s pid=%ld pipe=%d user=%d logged_on=%s "
           "steam_id=%llu appid=%s messages=%s sockets=%s netutils=%s html=%s\n",
           socket_path, static_cast<long>(getpid()), pipe, user,
           logged_on(user, pipe) ? "yes" : "no",
           static_cast<unsigned long long>(steam_id), app_id ? app_id : "0",
           steam_network_messages_iface ? "yes" : "fallback",
           steam_network_sockets_iface ? "yes" : "no",
           steam_network_utils_iface ? "yes" : "no",
           steam_html_iface ? "yes" : "no");
    fflush(stdout);

    while (running) {
        if (client) client->RunFrame();
        pollfd listener_poll{listener, POLLIN, 0};
        int poll_result = poll(&listener_poll, 1, 20);
        if (poll_result < 0) {
            if (errno == EINTR) continue;
            perror("lsteambridge: poll");
            break;
        }
        if (!poll_result || !(listener_poll.revents & POLLIN)) continue;
        int connection = accept4(listener, nullptr, nullptr, SOCK_CLOEXEC);
        if (connection < 0) {
            if (errno == EINTR) continue;
            perror("lsteambridge: accept");
            break;
        }
        ucred credentials{};
        socklen_t credentials_size = sizeof(credentials);
        bool trusted = getsockopt(connection, SOL_SOCKET, SO_PEERCRED,
                                  &credentials, &credentials_size) == 0 &&
                       credentials.uid == getuid();
        lsteam_bridge_request request{};
        ssize_t received = trusted ? recv(connection, &request, sizeof(request), 0) : -1;
        if (received == sizeof(request) && request.magic == LSTEAM_BRIDGE_MAGIC &&
            request.version == LSTEAM_BRIDGE_VERSION) {
            if (request.opcode == LSTEAM_BRIDGE_GET_AUTH_TICKET) {
                lsteam_bridge_ticket_response response{};
                response.magic = LSTEAM_BRIDGE_MAGIC;
                response.version = LSTEAM_BRIDGE_VERSION;
                response.opcode = request.opcode;
                response.sequence = request.sequence;
                if (steam_user_iface) {
                    response.ticket_handle = steam_user_iface->GetAuthSessionTicket(
                        response.ticket, sizeof(response.ticket), &response.ticket_size);
                }
                response.status = response.ticket_handle && response.ticket_size ? 0 : -EIO;
                send(connection, &response, sizeof(response), MSG_NOSIGNAL);
                close(connection);
                continue;
            }
            if (request.opcode == LSTEAM_BRIDGE_APP_TICKET_CALL) {
                lsteam_bridge_app_ticket_request ticket_request{};
                lsteam_bridge_app_ticket_response ticket_response{};
                ticket_response.magic = LSTEAM_BRIDGE_MAGIC;
                ticket_response.version = LSTEAM_BRIDGE_VERSION;
                ticket_response.opcode = request.opcode;
                ticket_response.sequence = request.sequence;
                ssize_t ticket_received = recv(connection, &ticket_request,
                                               sizeof(ticket_request), 0);
                if (ticket_received != sizeof(ticket_request) ||
                    !ticket_request.app_id || !ticket_request.capacity) {
                    ticket_response.status = -EINVAL;
                } else {
                    /* Steam may expose this private interface shortly after
                     * the rest of ISteamClient, so retry a failed startup
                     * lookup when the first DRM request arrives. */
                    if (!steam_app_ticket_iface && client)
                        steam_app_ticket_iface = static_cast<ISteamAppTicket *>(
                            client->GetISteamGenericInterface(
                                user, pipe, STEAMAPPTICKET_INTERFACE_VERSION));
                    if (!steam_app_ticket_iface) {
                        interface_result = 0;
                        steam_app_ticket_iface = static_cast<ISteamAppTicket *>(
                            create_interface(STEAMAPPTICKET_INTERFACE_VERSION,
                                             &interface_result));
                    }
                }
                if (!ticket_response.status && !steam_app_ticket_iface) {
                    ticket_response.status = -ENODEV;
                } else if (!ticket_response.status) {
                    uint32_t native_capacity = ticket_request.capacity;
                    if (native_capacity > LSTEAM_BRIDGE_MAX_TICKET)
                        native_capacity = LSTEAM_BRIDGE_MAX_TICKET;
                    ticket_response.returned_size =
                        steam_app_ticket_iface->GetAppOwnershipTicketData(
                            ticket_request.app_id, ticket_response.ticket,
                            native_capacity,
                            &ticket_response.app_id_offset,
                            &ticket_response.steam_id_offset,
                            &ticket_response.signature_offset,
                            &ticket_response.signature_size);
                    if (!ticket_response.returned_size) {
                        ticket_response.status = -EIO;
                    } else if (ticket_response.returned_size >
                                   native_capacity ||
                               ticket_response.returned_size >
                                   LSTEAM_BRIDGE_MAX_TICKET) {
                        ticket_response.status = -EOVERFLOW;
                    } else if (!app_ticket_range_valid(
                                   ticket_response.app_id_offset,
                                   sizeof(uint32_t),
                                   ticket_response.returned_size) ||
                               !app_ticket_range_valid(
                                   ticket_response.steam_id_offset,
                                   sizeof(uint64_t),
                                   ticket_response.returned_size) ||
                               !ticket_response.signature_size ||
                               !app_ticket_range_valid(
                                   ticket_response.signature_offset,
                                   ticket_response.signature_size,
                                   ticket_response.returned_size)) {
                        ticket_response.status = -EPROTO;
                    } else {
                        uint32_t ticket_app_id = 0;
                        uint64_t ticket_steam_id = 0;
                        memcpy(&ticket_app_id,
                               ticket_response.ticket +
                                   ticket_response.app_id_offset,
                               sizeof(ticket_app_id));
                        memcpy(&ticket_steam_id,
                               ticket_response.ticket +
                                   ticket_response.steam_id_offset,
                               sizeof(ticket_steam_id));
                        ticket_response.status =
                            ticket_app_id == ticket_request.app_id &&
                                    (!steam_id || ticket_steam_id == steam_id)
                                ? 0
                                : -EPROTO;
                    }
                }
                if (ticket_response.status) {
                    ticket_response.returned_size = 0;
                    ticket_response.app_id_offset = 0;
                    ticket_response.steam_id_offset = 0;
                    ticket_response.signature_offset = 0;
                    ticket_response.signature_size = 0;
                    memset(ticket_response.ticket, 0,
                           sizeof(ticket_response.ticket));
                }
                send(connection, &ticket_response, sizeof(ticket_response),
                     MSG_NOSIGNAL);
                close(connection);
                continue;
            }
            if (request.opcode == LSTEAM_BRIDGE_INIT_GAME_CONNECTION) {
                lsteam_bridge_game_connection_request game_request{};
                ssize_t game_received = recv(connection, &game_request,
                                             sizeof(game_request), 0);
                lsteam_bridge_blob_response game_response{};
                game_response.magic = LSTEAM_BRIDGE_MAGIC;
                game_response.version = LSTEAM_BRIDGE_VERSION;
                game_response.opcode = request.opcode;
                game_response.sequence = request.sequence;
                if (game_received == sizeof(game_request) && steam_user_iface &&
                    game_request.max_blob > 0) {
                    int capacity = game_request.max_blob;
                    if (capacity > static_cast<int>(sizeof(game_response.blob)))
                        capacity = sizeof(game_response.blob);
                    game_response.blob_size = steam_user_iface->InitiateGameConnection_DEPRECATED(
                        game_response.blob, capacity, CSteamID(static_cast<uint64>(game_request.server_steam_id)),
                        game_request.server_ip, game_request.server_port,
                        game_request.secure != 0);
                    game_response.status = game_response.blob_size ? 0 : -EIO;
                } else {
                    game_response.status = -EINVAL;
                }
                send(connection, &game_response, sizeof(game_response), MSG_NOSIGNAL);
                close(connection);
                continue;
            }
            if (request.opcode == LSTEAM_BRIDGE_GAME_SERVER_CALL) {
                lsteam_bridge_game_server_request game_server_request{};
                lsteam_bridge_game_server_response game_server_response{};
                game_server_response.sequence = request.sequence;
                ssize_t game_server_received = recv(connection, &game_server_request,
                                                     sizeof(game_server_request), 0);
                if (game_server_received == sizeof(game_server_request) && client) {
                    handle_game_server_call(client, &game_server_state,
                                            &game_server_request, &game_server_response);
                } else {
                    game_server_response.magic = LSTEAM_BRIDGE_MAGIC;
                    game_server_response.version = LSTEAM_BRIDGE_VERSION;
                    game_server_response.opcode = request.opcode;
                    game_server_response.sequence = request.sequence;
                    game_server_response.status = -EINVAL;
                }
                send(connection, &game_server_response, sizeof(game_server_response), MSG_NOSIGNAL);
                close(connection);
                continue;
            }
            if (request.opcode == LSTEAM_BRIDGE_VOICE_CALL) {
                lsteam_bridge_voice_request voice_request{};
                lsteam_bridge_voice_response voice_response{};
                voice_response.sequence = request.sequence;
                ssize_t voice_received = recv(connection, &voice_request,
                                              sizeof(voice_request), 0);
                if (voice_received == sizeof(voice_request)) {
                    handle_voice_call(steam_user_iface, &voice_request,
                                      &voice_response);
                } else {
                    voice_response.magic = LSTEAM_BRIDGE_MAGIC;
                    voice_response.version = LSTEAM_BRIDGE_VERSION;
                    voice_response.opcode = request.opcode;
                    voice_response.sequence = request.sequence;
                    voice_response.status = -EINVAL;
                }
                send(connection, &voice_response, sizeof(voice_response),
                     MSG_NOSIGNAL);
                close(connection);
                continue;
            }
            if (request.opcode == LSTEAM_BRIDGE_CLIENT_CALL ||
                request.opcode == LSTEAM_BRIDGE_USER_CALL ||
                request.opcode == LSTEAM_BRIDGE_FRIENDS_CALL ||
                request.opcode == LSTEAM_BRIDGE_UTILS_CALL ||
                request.opcode == LSTEAM_BRIDGE_REMOTE_STORAGE_CALL ||
                request.opcode == LSTEAM_BRIDGE_NETWORKING_CALL ||
                request.opcode == LSTEAM_BRIDGE_GAME_SERVER_STATS_CALL ||
                request.opcode == LSTEAM_BRIDGE_USER_STATS_CALL ||
                request.opcode == LSTEAM_BRIDGE_UGC_CALL ||
                request.opcode == LSTEAM_BRIDGE_APPS_CALL ||
                request.opcode == LSTEAM_BRIDGE_HTTP_CALL ||
                request.opcode == LSTEAM_BRIDGE_INVENTORY_CALL ||
                request.opcode == LSTEAM_BRIDGE_GAME_SEARCH_CALL ||
                request.opcode == LSTEAM_BRIDGE_APP_LIST_CALL ||
                request.opcode == LSTEAM_BRIDGE_VIDEO_CALL ||
                request.opcode == LSTEAM_BRIDGE_PARENTAL_CALL ||
                request.opcode == LSTEAM_BRIDGE_NETWORK_MESSAGES_CALL ||
                request.opcode == LSTEAM_BRIDGE_NETWORK_SOCKETS_CALL ||
                request.opcode == LSTEAM_BRIDGE_NETWORK_UTILS_CALL ||
                request.opcode == LSTEAM_BRIDGE_HTML_CALL ||
                request.opcode == LSTEAM_BRIDGE_LOBBY_CALL) {
                lsteam_bridge_game_server_request interface_request{};
                lsteam_bridge_game_server_response interface_response{};
                interface_response.sequence = request.sequence;
                ssize_t interface_received = recv(connection, &interface_request,
                                                  sizeof(interface_request), 0);
                if (interface_received == sizeof(interface_request)) {
                    if (request.opcode == LSTEAM_BRIDGE_CLIENT_CALL)
                        handle_client_call(client, &interface_request,
                                           &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_USER_CALL)
                        handle_user_call(steam_user_iface, &interface_request,
                                         &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_FRIENDS_CALL)
                        handle_friends_call(steam_friends_iface, &interface_request,
                                            &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_UTILS_CALL)
                        handle_utils_call(steam_utils_iface, &utils_state,
                                          &interface_request,
                                          &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_REMOTE_STORAGE_CALL)
                        handle_remote_storage_call(
                            steam_remote_storage_iface, &remote_storage_state,
                            &interface_request, &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_NETWORKING_CALL)
                        handle_networking_call(
                            steam_networking_iface, &networking_state,
                            &interface_request, &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_GAME_SERVER_STATS_CALL)
                        handle_game_server_stats_call(
                            client, &game_server_state, &game_server_stats_state,
                            &interface_request, &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_USER_STATS_CALL)
                        handle_user_stats_call(
                            steam_user_stats_iface, &interface_request,
                            &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_UGC_CALL)
                        handle_ugc_call(
                            steam_ugc14_iface, steam_ugc15_iface, &ugc_state,
                            &interface_request, &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_APPS_CALL)
                        handle_apps_call(
                            steam_apps_iface, &interface_request,
                            &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_HTTP_CALL)
                        handle_http_call(
                            steam_http_iface, &http_state, &interface_request,
                            &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_INVENTORY_CALL)
                        handle_inventory_call(
                            steam_inventory_iface, &inventory_state,
                            &interface_request, &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_GAME_SEARCH_CALL)
                        handle_game_search_call(
                            steam_game_search_iface, &interface_request,
                            &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_APP_LIST_CALL)
                        handle_app_list_call(
                            steam_app_list_iface, &app_list_state,
                            &interface_request, &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_VIDEO_CALL)
                        handle_video_call(
                            steam_video_iface, &interface_request,
                            &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_PARENTAL_CALL)
                        handle_parental_call(
                            steam_parental_iface, &interface_request,
                            &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_NETWORK_MESSAGES_CALL)
                        handle_network_messages_call(
                            steam_network_messages_iface,
                            steam_networking_iface,
                            &network_messages_state, &interface_request,
                            &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_NETWORK_SOCKETS_CALL)
                        handle_network_sockets_call(
                            steam_network_sockets_iface,
                            &network_sockets_state, &interface_request,
                            &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_NETWORK_UTILS_CALL)
                        handle_network_utils_call(
                            steam_network_utils_iface, &interface_request,
                            &interface_response);
                    else if (request.opcode == LSTEAM_BRIDGE_HTML_CALL)
                        handle_html_call(
                            steam_html_iface, &html_state, &interface_request,
                            &interface_response);
                    else
                        handle_lobby_call(steam_lobby_iface, &interface_request,
                                          &interface_response);
                } else {
                    interface_response.magic = LSTEAM_BRIDGE_MAGIC;
                    interface_response.version = LSTEAM_BRIDGE_VERSION;
                    interface_response.opcode = request.opcode;
                    interface_response.sequence = request.sequence;
                    interface_response.status = -EINVAL;
                }
                send(connection, &interface_response, sizeof(interface_response),
                     MSG_NOSIGNAL);
                close(connection);
                continue;
            }
            if (request.opcode == LSTEAM_BRIDGE_MATCHMAKING_REQUEST ||
                request.opcode == LSTEAM_BRIDGE_MATCHMAKING_CALL) {
                lsteam_bridge_mm_request mm_request{};
                lsteam_bridge_mm_response mm_response{};
                ssize_t mm_received = recv(connection, &mm_request,
                                           sizeof(mm_request), 0);
                if (mm_received != sizeof(mm_request) || !client) {
                    init_mm_response(&request, &mm_response);
                    mm_response.status = -EINVAL;
                } else if (request.opcode == LSTEAM_BRIDGE_MATCHMAKING_REQUEST) {
                    handle_mm_request(client, user, pipe, &matchmaking_state,
                                      &request, &mm_request, &mm_response);
                } else {
                    handle_mm_call(&matchmaking_state, &request, &mm_request,
                                   &mm_response);
                }
                send(connection, &mm_response, sizeof(mm_response), MSG_NOSIGNAL);
                close(connection);
                continue;
            }
            if (request.opcode == LSTEAM_BRIDGE_GET_CALLBACK) {
                lsteam_bridge_callback_response callback_response{};
                callback_response.magic = LSTEAM_BRIDGE_MAGIC;
                callback_response.version = LSTEAM_BRIDGE_VERSION;
                callback_response.opcode = request.opcode;
                callback_response.sequence = request.sequence;
                native_callback_msg callback{};
                int32_t steam_call = 0;
                bool have_callback = false;
                /* HTML paint/navigation callback structs contain native
                 * pointers. Never copy those pointers into the Wine process.
                 * BrowserReady is delivered through API-call result 4501 and
                 * therefore remains fully functional. */
                for (unsigned int attempt = 0; attempt < 32; ++attempt) {
                    callback = {};
                    steam_call = 0;
                    have_callback = get_callback(pipe, &callback, &steam_call);
                    if (have_callback) callback_pipe = pipe;
                    if (!have_callback && game_server_state.pipe &&
                        game_server_state.pipe != pipe) {
                        have_callback = get_callback(game_server_state.pipe,
                                                     &callback, &steam_call);
                        if (have_callback)
                            callback_pipe = game_server_state.pipe;
                    }
                    if (!have_callback) break;
                    if (callback.callback_id < 4502 ||
                        callback.callback_id > 4527)
                        break;
                    free_callback(callback_pipe);
                    callback_pipe = 0;
                    have_callback = false;
                }
                if (!have_callback) {
                    callback_response.status = -EAGAIN;
                } else if (callback.data_size < 0 ||
                           static_cast<uint32_t>(callback.data_size) >
                               LSTEAM_BRIDGE_MAX_CALLBACK ||
                           (callback.data_size && !callback.data)) {
                    callback_response.status = -EOVERFLOW;
                    free_callback(callback_pipe);
                    callback_pipe = 0;
                } else {
                    callback_response.steam_user = callback.steam_user;
                    callback_response.callback_id = callback.callback_id;
                    callback_response.steam_call = steam_call;
                    callback_response.data_size = callback.data_size;
                    if (callback.data_size)
                        memcpy(callback_response.data, callback.data,
                               callback.data_size);
                }
                send(connection, &callback_response, sizeof(callback_response),
                     MSG_NOSIGNAL);
                close(connection);
                continue;
            }
            if (request.opcode == LSTEAM_BRIDGE_FREE_CALLBACK) {
                lsteam_bridge_response callback_response{};
                callback_response.magic = LSTEAM_BRIDGE_MAGIC;
                callback_response.version = LSTEAM_BRIDGE_VERSION;
                callback_response.opcode = request.opcode;
                callback_response.sequence = request.sequence;
                if (!callback_pipe) {
                    callback_response.status = -ENOENT;
                } else {
                    free_callback(callback_pipe);
                    callback_response.status = 0;
                    callback_pipe = 0;
                }
                send(connection, &callback_response, sizeof(callback_response),
                     MSG_NOSIGNAL);
                close(connection);
                continue;
            }
            if (request.opcode == LSTEAM_BRIDGE_GET_API_CALL_RESULT) {
                lsteam_bridge_api_call_request api_request{};
                lsteam_bridge_api_call_response api_response{};
                api_response.magic = LSTEAM_BRIDGE_MAGIC;
                api_response.version = LSTEAM_BRIDGE_VERSION;
                api_response.opcode = request.opcode;
                api_response.sequence = request.sequence;
                ssize_t api_received = recv(connection, &api_request,
                                            sizeof(api_request), 0);
                if (api_received != sizeof(api_request) ||
                    api_request.callback_size < 0 ||
                    static_cast<uint32_t>(api_request.callback_size) >
                        LSTEAM_BRIDGE_MAX_CALLBACK) {
                    api_response.status = -EINVAL;
                } else {
                    bool failed = false;
                    bool ok = get_api_call_result(pipe, api_request.call,
                        api_response.data, api_request.callback_size,
                        api_request.callback_id, &failed);
                    if (!ok && game_server_state.pipe &&
                        game_server_state.pipe != pipe) {
                        failed = false;
                        ok = get_api_call_result(game_server_state.pipe,
                            api_request.call, api_response.data,
                            api_request.callback_size, api_request.callback_id,
                            &failed);
                    }
                    api_response.failed = failed;
                    api_response.data_size = ok ? api_request.callback_size : 0;
                    api_response.status = ok ? 0 : -EIO;
                }
                send(connection, &api_response, sizeof(api_response), MSG_NOSIGNAL);
                close(connection);
                continue;
            }
            lsteam_bridge_response response{};
            response.magic = LSTEAM_BRIDGE_MAGIC;
            response.version = LSTEAM_BRIDGE_VERSION;
            response.opcode = request.opcode;
            response.sequence = request.sequence;
            response.status = request.opcode == LSTEAM_BRIDGE_PING ||
                              request.opcode == LSTEAM_BRIDGE_STATUS ||
                              request.opcode == LSTEAM_BRIDGE_STOP ? 0 : -ENOSYS;
            response.server_pid = getpid();
            response.server_uid = getuid();
            response.steam_pipe = pipe;
            response.steam_user = user;
            response.steam_id = steam_id;
            response.logged_on = logged_on(user, pipe);
            send(connection, &response, sizeof(response), MSG_NOSIGNAL);
            if (request.opcode == LSTEAM_BRIDGE_STOP) running = 0;
        }
        close(connection);
    }

    close(listener);
    safe_remove_socket(socket_path);
    cleanup_mm_state(&matchmaking_state);
    if (steam_html_iface && html_state.initialized)
        steam_html_iface->Shutdown();
    if (game_server_state.owns_handles && game_server_state.user && game_server_state.pipe) {
        release_user(game_server_state.pipe, game_server_state.user);
        release_pipe(game_server_state.pipe);
    }
    release_user(pipe, user);
    release_pipe(pipe);
    dlclose(module);
    return 0;
}
