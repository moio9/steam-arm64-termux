static void handle_parental_call(
    ISteamParentalSettings *parental,
    lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_PARENTAL_CALL;
    response->status = 0;
    if (!parental) {
        response->status = -ENODEV;
        return;
    }

    switch (request->method) {
    case 0: response->result = parental->BIsParentalLockEnabled(); break;
    case 1: response->result = parental->BIsParentalLockLocked(); break;
    case 2: response->result = parental->BIsAppBlocked(request->arg0); break;
    case 3: response->result = parental->BIsAppInBlockList(request->arg0); break;
    case 4:
        response->result = parental->BIsFeatureBlocked(
            static_cast<EParentalFeature>(request->arg0));
        break;
    case 5:
        response->result = parental->BIsFeatureInBlockList(
            static_cast<EParentalFeature>(request->arg0));
        break;
    default: response->status = -ENOSYS; break;
    }
}
