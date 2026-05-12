local capability = require("capability")

local STATE_PATH = "/fatfs/porton_state.txt"
local LAST_CHAT_PATH = "/fatfs/porton_last_chat.txt"

local a = type(args) == "table" and args or {}
local action = type(a.action) == "string" and string.lower(a.action) or "status"
local peer_mac = type(a.peer_mac) == "string" and a.peer_mac or ""
local chat_id = type(a.chat_id) == "string" and a.chat_id or ""

local function read_estimated_state()
    local file = io.open(STATE_PATH, "r")
    if not file then
        return "unknown"
    end

    local raw = file:read("*a")
    file:close()
    raw = type(raw) == "string" and string.lower((raw:gsub("%s+", ""))) or ""
    if raw == "on" or raw == "off" or raw == "unknown" then
        return raw
    end
    return "unknown"
end

local function write_estimated_state(value)
    local file, err = io.open(STATE_PATH, "w")
    if not file then
        error("[espnow_porton] failed to open state file: " .. tostring(err))
    end
    file:write(value .. "\n")
    file:close()
end

local function write_last_chat_route()
    if chat_id == "" then
        return
    end

    local file, err = io.open(LAST_CHAT_PATH, "w")
    if not file then
        error("[espnow_porton] failed to open last chat file: " .. tostring(err))
    end
    file:write(chat_id .. "\n")
    file:write(peer_mac .. "\n")
    file:close()
end

local function status_label(value)
    if value == "on" then
        return "ENCENDIDA"
    end
    if value == "off" then
        return "APAGADA"
    end
    return "DESCONOCIDO"
end

local function send_payload(text)
    local ok, out, err = capability.call("espnow_send_text", {
        peer_mac = peer_mac,
        text = text,
    })
    if not ok then
        error(string.format("[espnow_porton] espnow_send_text failed: %s | %s",
            tostring(err), tostring(out)))
    end
    return out
end

if peer_mac == "" then
    error("[espnow_porton] missing args.peer_mac")
end

local estimated = read_estimated_state()

if action == "status" then
    write_last_chat_route()
    send_payload("rele status")
    print(string.format(
        "Consultando estado real del porton por ESP-NOW...\nUltimo estado guardado: %s",
        status_label(estimated)))
    return
end

if action == "on" then
    write_last_chat_route()
    send_payload("rele on")
    estimated = "on"
    write_estimated_state(estimated)
    print("OK luz porton encendida por ESP-NOW\nEstado estimado: ENCENDIDA")
    return
end

if action == "off" then
    write_last_chat_route()
    send_payload("rele off")
    estimated = "off"
    write_estimated_state(estimated)
    print("OK luz porton apagada por ESP-NOW\nEstado estimado: APAGADA")
    return
end

if action == "toggle" then
    write_last_chat_route()
    send_payload("rele toggle")
    if estimated == "on" then
        estimated = "off"
    elseif estimated == "off" then
        estimated = "on"
    else
        estimated = "unknown"
    end
    write_estimated_state(estimated)
    print(string.format(
        "OK luz porton alternada por ESP-NOW\nEstado estimado: %s",
        status_label(estimated)))
    return
end

error("[espnow_porton] unsupported action: " .. tostring(action))
