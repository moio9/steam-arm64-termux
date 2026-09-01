/* Native SteamUser voice calls. Audio uses its own bounded 64 KiB protocol. */
static void handle_voice_call(ISteamUser *steam_user,
                              lsteam_bridge_voice_request *request,
                              lsteam_bridge_voice_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_VOICE_CALL;
    response->status = 0;
    response->result = k_EVoiceResultNotInitialized;
    if (!steam_user) {
        response->status = -ENODEV;
        return;
    }
    if (request->data_size > LSTEAM_BRIDGE_MAX_VOICE) {
        response->status = -EINVAL;
        return;
    }

    switch (request->method) {
    case 0:
        steam_user->StartVoiceRecording();
        response->result = k_EVoiceResultOK;
        break;
    case 1:
        steam_user->StopVoiceRecording();
        response->result = k_EVoiceResultOK;
        break;
    case 2: {
        uint32 compressed = 0, uncompressed = 0;
        response->result = steam_user->GetAvailableVoice(
            (request->flags & 1u) ? &compressed : nullptr,
            (request->flags & 2u) ? &uncompressed : nullptr,
            request->arg0);
        response->compressed_size = compressed;
        response->uncompressed_size = uncompressed;
        break;
    }
    case 3: {
        if (request->arg0 > LSTEAM_BRIDGE_MAX_VOICE ||
            request->arg1 > LSTEAM_BRIDGE_MAX_VOICE - request->arg0) {
            response->result = k_EVoiceResultBufferTooSmall;
            response->status = -E2BIG;
            break;
        }
        uint32 compressed = 0, uncompressed = 0;
        uint8_t *uncompressed_data = response->data + request->arg0;
        response->result = steam_user->GetVoice(
            (request->flags & 1u) != 0,
            (request->flags & 1u) ? response->data : nullptr,
            request->arg0,
            (request->flags & 1u) ? &compressed : nullptr,
            (request->flags & 2u) != 0,
            (request->flags & 2u) ? uncompressed_data : nullptr,
            request->arg1,
            (request->flags & 2u) ? &uncompressed : nullptr,
            request->arg2);
        response->compressed_size = compressed;
        response->uncompressed_size = uncompressed;
        if (compressed > request->arg0 || uncompressed > request->arg1 ||
            compressed > LSTEAM_BRIDGE_MAX_VOICE - uncompressed) {
            if (response->result != k_EVoiceResultBufferTooSmall)
                response->status = -EOVERFLOW;
            break;
        }
        if (uncompressed && compressed != request->arg0)
            memmove(response->data + compressed, uncompressed_data,
                    uncompressed);
        response->data_size = compressed + uncompressed;
        break;
    }
    case 4: {
        if (request->arg0 > LSTEAM_BRIDGE_MAX_VOICE) {
            response->result = k_EVoiceResultBufferTooSmall;
            response->status = -E2BIG;
            break;
        }
        uint32 written = 0;
        response->result = steam_user->DecompressVoice(
            request->data_size ? request->data : nullptr,
            request->data_size, response->data, request->arg0, &written,
            request->arg1);
        response->uncompressed_size = written;
        if (written > request->arg0 || written > LSTEAM_BRIDGE_MAX_VOICE) {
            if (response->result != k_EVoiceResultBufferTooSmall)
                response->status = -EOVERFLOW;
            break;
        }
        response->data_size = written;
        break;
    }
    case 5:
        response->result = steam_user->GetVoiceOptimalSampleRate();
        break;
    default:
        response->status = -ENOSYS;
        break;
    }
}
