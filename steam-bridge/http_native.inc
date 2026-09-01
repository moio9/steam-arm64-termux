#include <string>
#include <vector>

static constexpr uint32_t LSTEAM_BRIDGE_HTTP_MAX_BODY = 64u * 1024u * 1024u;

struct bridge_http_state {
    uint32_t body_handle = 0;
    bool body_valid = false;
    bool body_result = false;
    std::vector<uint8_t> body;

    uint32_t header_handle = 0;
    bool header_valid = false;
    bool header_result = false;
    std::string header_name;
    std::vector<uint8_t> header;

    uint32_t stream_handle = 0;
    uint32_t stream_offset = 0;
    uint32_t stream_size = 0;
    bool stream_valid = false;
    bool stream_result = false;
    std::vector<uint8_t> stream;

    uint32_t post_handle = 0;
    uint32_t post_total = 0;
    uint32_t post_received = 0;
    std::string post_content_type;
    std::vector<uint8_t> post;
};

static void http_copy_chunk(const std::vector<uint8_t> &source,
                            uint32_t offset,
                            lsteam_bridge_game_server_response *response)
{
    response->arg0 = source.size();
    if (offset > source.size()) {
        response->status = -EINVAL;
        return;
    }
    size_t count = source.size() - offset;
    if (count > sizeof(response->data)) count = sizeof(response->data);
    if (count) memcpy(response->data, source.data() + offset, count);
    response->data_size = count;
}

static void handle_http_call(
    ISteamHTTP *http, bridge_http_state *state,
    lsteam_bridge_game_server_request *request,
    lsteam_bridge_game_server_response *response)
{
    response->magic = LSTEAM_BRIDGE_MAGIC;
    response->version = LSTEAM_BRIDGE_VERSION;
    response->opcode = LSTEAM_BRIDGE_HTTP_CALL;
    response->status = 0;
    request->text[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    request->value[LSTEAM_BRIDGE_MAX_TEXT - 1] = 0;
    if (!http) {
        response->status = -ENODEV;
        return;
    }

    HTTPRequestHandle handle = request->arg0;
    switch (request->method) {
    case 0:
        response->result = http->CreateHTTPRequest(
            static_cast<EHTTPMethod>(request->arg0), request->text);
        break;
    case 1:
        response->result = http->SetHTTPRequestContextValue(
            handle, request->steam_id);
        break;
    case 2:
        response->result = http->SetHTTPRequestNetworkActivityTimeout(
            handle, request->arg1);
        break;
    case 3:
        response->result = http->SetHTTPRequestHeaderValue(
            handle, request->text, request->value);
        break;
    case 4:
        response->result = http->SetHTTPRequestGetOrPostParameter(
            handle, request->text, request->value);
        break;
    case 5:
    case 6: {
        SteamAPICall_t call = k_uAPICallInvalid;
        bool ok = request->method == 5
            ? http->SendHTTPRequest(handle, &call)
            : http->SendHTTPRequestAndStreamResponse(handle, &call);
        response->result = ok;
        response->steam_id = call;
        break;
    }
    case 7:
        response->result = http->DeferHTTPRequest(handle);
        break;
    case 8:
        response->result = http->PrioritizeHTTPRequest(handle);
        break;
    case 9: {
        uint32_t size = 0;
        response->result = http->GetHTTPResponseHeaderSize(
            handle, request->text, &size);
        response->arg0 = size;
        break;
    }
    case 10: {
        uint32_t offset = request->arg1;
        if (!offset || !state->header_valid ||
            state->header_handle != handle ||
            state->header_name != request->text) {
            uint32_t size = 0;
            state->header_handle = handle;
            state->header_name = request->text;
            state->header_valid = true;
            state->header_result = http->GetHTTPResponseHeaderSize(
                handle, request->text, &size);
            if (size > LSTEAM_BRIDGE_HTTP_MAX_BODY) {
                state->header_result = false;
                response->status = -EFBIG;
                break;
            }
            state->header.assign(size, 0);
            if (state->header_result && size)
                state->header_result = http->GetHTTPResponseHeaderValue(
                    handle, request->text, state->header.data(), size);
        }
        response->result = state->header_result;
        if (state->header_result)
            http_copy_chunk(state->header, offset, response);
        break;
    }
    case 11: {
        uint32_t size = 0;
        response->result = http->GetHTTPResponseBodySize(handle, &size);
        response->arg0 = size;
        break;
    }
    case 12: {
        uint32_t offset = request->arg1;
        if (!offset || !state->body_valid || state->body_handle != handle) {
            uint32_t size = 0;
            state->body_handle = handle;
            state->body_valid = true;
            state->body_result = http->GetHTTPResponseBodySize(handle, &size);
            if (size > LSTEAM_BRIDGE_HTTP_MAX_BODY) {
                state->body_result = false;
                response->status = -EFBIG;
                break;
            }
            state->body.assign(size, 0);
            if (state->body_result && size)
                state->body_result = http->GetHTTPResponseBodyData(
                    handle, state->body.data(), size);
        }
        response->result = state->body_result;
        if (state->body_result) http_copy_chunk(state->body, offset, response);
        break;
    }
    case 13: {
        uint32_t source_offset = request->arg1;
        uint32_t size = request->arg2;
        uint32_t chunk_offset = request->arg3;
        if (size > LSTEAM_BRIDGE_HTTP_MAX_BODY || chunk_offset > size) {
            response->status = size > LSTEAM_BRIDGE_HTTP_MAX_BODY ? -EFBIG : -EINVAL;
            break;
        }
        if (!chunk_offset || !state->stream_valid ||
            state->stream_handle != handle ||
            state->stream_offset != source_offset ||
            state->stream_size != size) {
            state->stream_handle = handle;
            state->stream_offset = source_offset;
            state->stream_size = size;
            state->stream_valid = true;
            state->stream.assign(size, 0);
            state->stream_result = !size ||
                http->GetHTTPStreamingResponseBodyData(
                    handle, source_offset, state->stream.data(), size);
        }
        response->result = state->stream_result;
        if (state->stream_result)
            http_copy_chunk(state->stream, chunk_offset, response);
        break;
    }
    case 14:
        state->body_valid = state->body_handle != handle;
        state->header_valid = state->header_handle != handle;
        state->stream_valid = state->stream_handle != handle;
        response->result = http->ReleaseHTTPRequest(handle);
        break;
    case 15: {
        float percent = 0.0f;
        response->result = http->GetHTTPDownloadProgressPct(handle, &percent);
        memcpy(response->data, &percent, sizeof(percent));
        response->data_size = sizeof(percent);
        break;
    }
    case 16: {
        uint32_t offset = request->arg1;
        uint32_t total = request->arg2;
        if (total > LSTEAM_BRIDGE_HTTP_MAX_BODY ||
            request->data_size > sizeof(request->data) ||
            offset > total || request->data_size > total - offset) {
            response->status = total > LSTEAM_BRIDGE_HTTP_MAX_BODY ? -EFBIG : -EINVAL;
            break;
        }
        if (!offset) {
            state->post_handle = handle;
            state->post_total = total;
            state->post_received = 0;
            state->post_content_type = request->text;
            state->post.assign(total, 0);
        }
        if (state->post_handle != handle || state->post_total != total ||
            state->post_received != offset) {
            response->status = -EINVAL;
            break;
        }
        if (request->data_size)
            memcpy(state->post.data() + offset, request->data,
                   request->data_size);
        state->post_received += request->data_size;
        if (state->post_received == total) {
            response->result = http->SetHTTPRequestRawPostBody(
                handle, state->post_content_type.c_str(),
                state->post.data(), total);
            state->post.clear();
            state->post_received = 0;
        } else {
            response->result = true;
        }
        break;
    }
    case 17:
        response->result = http->CreateCookieContainer(request->arg0 != 0);
        break;
    case 18:
        response->result = http->ReleaseCookieContainer(request->arg0);
        break;
    case 19:
        if (!request->data_size ||
            request->data_size > sizeof(request->data) ||
            request->data[request->data_size - 1]) {
            response->status = -EINVAL;
        } else {
            response->result = http->SetCookie(
                request->arg0, request->text, request->value,
                reinterpret_cast<const char *>(request->data));
        }
        break;
    case 20:
        response->result = http->SetHTTPRequestCookieContainer(
            handle, request->arg1);
        break;
    case 21:
        response->result = http->SetHTTPRequestUserAgentInfo(
            handle, request->text);
        break;
    case 22:
        response->result = http->SetHTTPRequestRequiresVerifiedCertificate(
            handle, request->arg1 != 0);
        break;
    case 23:
        response->result = http->SetHTTPRequestAbsoluteTimeoutMS(
            handle, request->arg1);
        break;
    case 24: {
        bool timed_out = false;
        response->result = http->GetHTTPRequestWasTimedOut(handle, &timed_out);
        response->arg0 = timed_out;
        break;
    }
    default:
        response->status = -ENOSYS;
        break;
    }
}
