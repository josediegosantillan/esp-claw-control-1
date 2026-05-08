local M = {}

local gpio = require("gpio")
local STATE_PATH = "/fatfs/relay_state.txt"

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

local function state_to_level(is_on, active_level)
    return is_on and active_level or (active_level == 0 and 1 or 0)
end

local function level_to_state(level, active_level)
    return level == active_level
end

local function get_relay_config(cfg, relay_index)
    if relay_index == 2 then
        return {
            gpio = cfg.relay2_gpio,
            active_level = cfg.relay2_active_level,
            default_on = cfg.default_on2,
            button_gpio = cfg.button2_gpio,
        }
    end

    return {
        gpio = cfg.relay_gpio,
        active_level = cfg.relay_active_level,
        default_on = cfg.default_on,
        button_gpio = cfg.button_gpio,
    }
end

local function set_relay_level(relay_cfg, is_on)
    gpio.set_direction(relay_cfg.gpio, "output")
    gpio.set_level(relay_cfg.gpio, state_to_level(is_on, relay_cfg.active_level))
    return is_on and true or false
end

local function default_states(cfg)
    return {
        relay1 = cfg.default_on and true or false,
        relay2 = cfg.default_on2 and true or false,
    }
end

local function read_persisted_states(cfg)
    local file = io.open(STATE_PATH, "r")
    if not file then
        return default_states(cfg)
    end

    local raw = file:read("*a")
    file:close()
    if type(raw) ~= "string" or raw == "" then
        return default_states(cfg)
    end

    local r1, r2 = string.match(raw, "relay1=(%d)%s+relay2=(%d)")
    if not r1 or not r2 then
        return default_states(cfg)
    end

    return {
        relay1 = r1 == "1",
        relay2 = r2 == "1",
    }
end

local function write_persisted_states(states)
    local file, err = io.open(STATE_PATH, "w")
    if not file then
        error("[relay_shared] failed to open state file: " .. tostring(err))
    end

    file:write(string.format(
        "relay1=%d relay2=%d\n",
        states.relay1 and 1 or 0,
        states.relay2 and 1 or 0
    ))
    file:close()
end

local function ensure_relay_output(relay_cfg)
    gpio.set_direction(relay_cfg.gpio, "output")
end

local function apply_states(cfg, states)
    set_relay_level(get_relay_config(cfg, 1), states.relay1)
    set_relay_level(get_relay_config(cfg, 2), states.relay2)
    return states
end

function M.load_config(raw_args)
    local a = type(raw_args) == "table" and raw_args or {}

    return {
        relay_gpio = int_default(a.relay_gpio, 4),
        relay_active_level = int_default(a.relay_active_level, 1),
        button_gpio = int_default(a.button_gpio, 5),
        button_active_level = int_default(a.button_active_level, 0),
        relay2_gpio = int_default(a.relay2_gpio, 6),
        relay2_active_level = int_default(a.relay2_active_level, 0),
        button2_gpio = int_default(a.button2_gpio, 7),
        button2_active_level = int_default(a.button2_active_level, int_default(a.button_active_level, 0)),
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
        default_on2 = bool_default(a.default_on2, false),
        relay_index = int_default(a.relay_index, 0),
    }
end

function M.ensure_runtime(cfg)
    ensure_relay_output(get_relay_config(cfg, 1))
    ensure_relay_output(get_relay_config(cfg, 2))
end

function M.reset_state(cfg)
    local states = default_states(cfg)
    write_persisted_states(states)
    return apply_states(cfg, states)
end

function M.read_state(cfg)
    return read_persisted_states(cfg)
end

function M.write_state(cfg, relay_index, is_on)
    M.ensure_runtime(cfg)
    local states = read_persisted_states(cfg)
    if relay_index == 2 then
        states.relay2 = is_on and true or false
    else
        states.relay1 = is_on and true or false
    end
    write_persisted_states(states)
    return apply_states(cfg, states)
end

function M.toggle_state(cfg, relay_index)
    local states = M.read_state(cfg)
    if relay_index == 2 then
        return M.write_state(cfg, relay_index, not states.relay2)
    end
    return M.write_state(cfg, relay_index, not states.relay1)
end

function M.resolve_action(action)
    if type(action) ~= "string" or action == "" then
        return "status"
    end

    local normalized = string.lower(action)
    if normalized == "menu" or normalized == "help" or normalized == "ayuda" then
        return "menu"
    end
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
    local relay_index = cfg.relay_index == 2 and 2 or 1

    if normalized == "menu" then
        return M.read_state(cfg), normalized
    end
    if normalized == "on" then
        return M.write_state(cfg, relay_index, true), normalized
    end
    if normalized == "off" then
        return M.write_state(cfg, relay_index, false), normalized
    end
    if normalized == "toggle" then
        return M.toggle_state(cfg, relay_index), normalized
    end
    return M.read_state(cfg), normalized
end

function M.format_relay_summary(states)
    return string.format(
        "Taller: %s | Patio: %s",
        states.relay1 and "ON" or "OFF",
        states.relay2 and "ON" or "OFF"
    )
end

function M.format_status(cfg, states)
    return string.format(
        "r1=%s gpio=%d button=%d | r2=%s gpio=%d button=%d | oled=%s i2c=%d sda=%d scl=%d addr=0x%02X",
        states.relay1 and "ON" or "OFF",
        cfg.relay_gpio,
        cfg.button_gpio,
        states.relay2 and "ON" or "OFF",
        cfg.relay2_gpio,
        cfg.button2_gpio,
        cfg.enable_oled and "ON" or "OFF",
        cfg.i2c_port,
        cfg.i2c_sda,
        cfg.i2c_scl,
        cfg.oled_addr
    )
end

function M.format_menu(cfg, states)
    return table.concat({
        "ESP-Claw Telegram",
        M.format_relay_summary(states),
        "",
        "Panel con botones:",
        "/panel",
        "",
        "Taller:",
        "/relay on",
        "/relay off",
        "/relay toggle",
        "/relay status",
        "",
        "Patio:",
        "/relay2 on",
        "/relay2 off",
        "/relay2 toggle",
        "/relay2 status",
        "",
        "Tambien podes usar:",
        "encender luz taller",
        "apagar luz taller",
        "encender luz patio",
        "apagar luz patio"
    }, "\n")
end

function M.format_action_response(cfg, states, action)
    if action == "menu" then
        return M.format_menu(cfg, states)
    end

    return string.format(
        "OK %s | %s | %s",
        action,
        M.format_relay_summary(states),
        M.format_status(cfg, states)
    )
end

return M
