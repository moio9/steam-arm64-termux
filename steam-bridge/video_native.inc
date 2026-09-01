static void handle_video_call(
    ISteamVideo *video, lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_VIDEO_CALL;
    response->status = 0;
    if (!video) {
        response->status = -ENODEV;
        return;
    }

    switch (request->method) {
    case 0:
        video->GetVideoURL(request->arg0);
        break;
    case 1: {
        int viewers = 0;
        response->result = video->IsBroadcasting(&viewers);
        response->arg0 = static_cast<uint32_t>(viewers);
        break;
    }
    case 2:
        video->GetOPFSettings(request->arg0);
        break;
    case 3: {
        int32_t capacity = static_cast<int32_t>(request->arg1);
        if (capacity < 0 || static_cast<uint32_t>(capacity) >
                              sizeof(response->data)) {
            response->status = -E2BIG;
            break;
        }
        char output[LSTEAM_BRIDGE_MAX_TICKET]{};
        int32_t size = capacity;
        response->result = video->GetOPFStringForApp(
            request->arg0, capacity ? output : nullptr, &size);
        response->arg0 = size < 0 ? 0 : static_cast<uint32_t>(size);
        if (capacity) {
            size_t length = strnlen(output, capacity);
            response->data_size = length < static_cast<size_t>(capacity)
                ? length + 1 : static_cast<uint32_t>(capacity);
            if (response->data_size)
                memcpy(response->data, output, response->data_size);
        }
        break;
    }
    default:
        response->status = -ENOSYS;
        break;
    }
}
