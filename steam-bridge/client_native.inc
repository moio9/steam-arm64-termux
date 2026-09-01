/* Native ISteamClient calls whose arguments must cross the Wine/Bionic
 * boundary before a local game-server user is created. */
static void handle_client_call(ISteamClient *client,
                               lsteam_bridge_game_server_request *request,
                               lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_CLIENT_CALL;
    response->status = 0;
    if (!client) {
        response->status = -ENODEV;
        return;
    }

    switch (request->method) {
    case 0: {
        SteamIPAddress_t address{};
        if (request->data_size == sizeof(uint32_t)) {
            memcpy(&address.x.m_unIPv4, request->data, sizeof(uint32_t));
            address.m_eType = k_ESteamIPTypeIPv4;
        } else if (request->data_size == sizeof(address)) {
            memcpy(&address, request->data, sizeof(address));
        } else {
            response->status = -EINVAL;
            break;
        }
        client->SetLocalIPBinding(address,
                                  static_cast<uint16_t>(request->arg0));
        response->result = 1;
        break;
    }
    default:
        response->status = -ENOSYS;
        break;
    }
}
