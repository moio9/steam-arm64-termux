start_steam_bridge() {
    [[ $steam_bridge == proxy ]] || return 0
    [[ -x $bridge_root/run-server.sh && -x $bridge_root/lsteambridge-client ]] || {
        printf '%s\n' 'proton-bionic-termux: lsteambridge is not built' >&2
        return 1
    }
    LSTEAM_BRIDGE_SOCKET=$bridge_socket "$bridge_root/run-server.sh" >>"$log" 2>&1
    export LSTEAM_BRIDGE_MODE=proxy
    export LSTEAM_BRIDGE_SOCKET=$bridge_socket
    export LSTEAM_BRIDGE_TRACE=$bridge_trace
}

stop_steam_bridge() {
    [[ $steam_bridge == proxy ]] || return 0
    LSTEAM_BRIDGE_SOCKET=$bridge_socket \
        "$bridge_root/lsteambridge-client" stop >>"$log" 2>&1 || true
    rm -f -- "$bridge_root/server.pid" "$bridge_root/server.appid"
}
