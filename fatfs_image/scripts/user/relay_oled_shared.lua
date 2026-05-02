local M = {}

local relay_manager = require("relay_manager")

local function int_default(value, default_value)
    if type(value) == "number" then
        return math.floor(value)
    end
    return default_value
end

local function int_min(value, default_value, min_value)
    local resolved = int_default(value, default_value)
    if resolved < min_value then
        return min_value
    end
    return resolved
end

local function bool_default(value, default_value)
    if type(value) == "boolean" then
        return value
    end
    if type(value) == "number" then
        return value ~= 0
    end
    if type(value) == "string" then
        local normalized = string.lower(value)
        if normalized == "1" or normalized == "true" or normalized == "yes" or normalized == "on" then
            return true
        end
        if normalized == "0" or normalized == "false" or normalized == "no" or normalized == "off" then
            return false
        end
    end
    return default_value
end

function M.load_config(raw_args)
    local a = type(raw_args) == "table" and raw_args or {}

    return {
        relay_gpio = int_default(a.relay_gpio, 4),
        relay_active_level = int_default(a.relay_active_level, 1),
        button_gpio = int_default(a.button_gpio, 5),
        button_active_level = int_default(a.button_active_level, 0),
        i2c_port = int_default(a.i2c_port, 0),
        i2c_sda = int_default(a.i2c_sda, 8),
        i2c_scl = int_default(a.i2c_scl, 9),
        i2c_freq_hz = int_min(a.i2c_freq_hz, 400000, 1000),
        oled_addr = int_default(a.oled_addr, 0x3C),
        oled_width = int_min(a.oled_width, 128, 1),
        oled_height = int_min(a.oled_height, 64, 1),
        enable_oled = bool_default(a.enable_oled, true),
        poll_ms = int_min(a.poll_ms, 50, 10),
        status_refresh_ms = int_min(a.status_refresh_ms, 1000, 100),
        default_on = bool_default(a.default_on, false),
    }
end

function M.ensure_runtime(cfg)
    relay_manager.configure({
        relay_gpio = cfg.relay_gpio,
        relay_active_level = cfg.relay_active_level,
        default_on = cfg.default_on,
    })
end

function M.read_state(cfg)
    M.ensure_runtime(cfg)
    return relay_manager.get()
end

function M.write_state(cfg, is_on)
    M.ensure_runtime(cfg)
    return relay_manager.set(is_on and true or false)
end

function M.toggle_state(cfg)
    M.ensure_runtime(cfg)
    return relay_manager.toggle()
end

function M.resolve_action(action)
    if type(action) ~= "string" or action == "" then
        return "status"
    end

    local normalized = string.lower(action)
    if normalized == "on" or normalized == "encender" then
        return "on"
    end
    if normalized == "off" or normalized == "apagar" then
        return "off"
    end
    if normalized == "toggle" or normalized == "cambiar" then
        return "toggle"
    end
    return "status"
end

function M.apply_action(cfg, action)
    local normalized = M.resolve_action(action)

    if normalized == "on" then
        return M.write_state(cfg, true), normalized
    end
    if normalized == "off" then
        return M.write_state(cfg, false), normalized
    end
    if normalized == "toggle" then
        return M.toggle_state(cfg), normalized
    end
    return M.read_state(cfg), normalized
end

function M.format_status(cfg, is_on)
    return string.format(
        "relay=%s gpio=%d button=%d oled=%s i2c=%d sda=%d scl=%d addr=0x%02X",
        is_on and "ON" or "OFF",
        cfg.relay_gpio,
        cfg.button_gpio,
        cfg.enable_oled and "ON" or "OFF",
        cfg.i2c_port,
        cfg.i2c_sda,
        cfg.i2c_scl,
        cfg.oled_addr
    )
end

return M
