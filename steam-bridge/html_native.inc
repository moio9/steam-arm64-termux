struct bridge_html_state {
    bool initialized = false;
};

static void handle_html_call(
    ISteamHTMLSurface *html, bridge_html_state *state,
    lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_HTML_CALL;
    response->status = 0;
    request->text[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    request->value[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    if (!html) {
        response->status = -ENODEV;
        return;
    }

    switch (request->method) {
    case 0:
        if (!state->initialized) state->initialized = html->Init();
        response->result = state->initialized;
        break;
    case 1:
        /* Keep the process-wide native surface alive until bridge shutdown.
         * Some games acquire/release this interface more than once. */
        response->result = 1;
        break;
    case 2:
        if (!state->initialized) state->initialized = html->Init();
        if (!state->initialized) {
            response->status = -EIO;
            break;
        }
        response->result = html->CreateBrowser(request->text, request->value);
        if (!response->result) response->status = -EIO;
        break;
    case 3:
        html->RemoveBrowser(request->arg0);
        response->result = 1;
        break;
    default:
        response->status = -ENOSYS;
        break;
    }
}
