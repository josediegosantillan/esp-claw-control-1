local button = require("button")
local delay = require("delay")
local i2c = require("i2c")
local system = require("system")

local shared = dofile("/fatfs/scripts/user/relay_oled_shared.lua")

local cfg = shared.load_config(args)

local function has_addr(addrs, target)
    for _, addr in ipairs(addrs) do
        if addr == target then
            return true
        end
    end
    return false
end

local function load_ssd1306_driver()
    local candidates = {
        "/fatfs/scripts/ssd1306.lua",
        "/fatfs/scripts/builtin/ssd1306.lua",
        "ssd1306.lua",
        "builtin/ssd1306.lua",
    }

    local errors = {}
    for _, path in ipairs(candidates) do
        local ok, mod = pcall(dofile, path)
        if ok then
            return mod
        end
        errors[#errors + 1] = string.format("%s => %s", path, tostring(mod))
    end

    error("[relay_service] failed to load SSD1306 driver: " .. table.concat(errors, " | "))
end

local current_state = shared.read_state(cfg)
local last_display_state = nil
local last_refresh_ms = 0
local last_state_poll_ms = 0

local btn_handles = {}
local bus = nil
local dev = nil
local oled = nil

local function clamp(value, min_value, max_value)
    if value < min_value then
        return min_value
    end
    if value > max_value then
        return max_value
    end
    return value
end

local function wifi_percent_from_rssi(rssi)
    if type(rssi) ~= "number" then
        return 0
    end
    return math.floor(((clamp(rssi, -100, -50) + 100) * 2) + 0.5)
end

local function read_runtime_status()
    local info = {}
    local ok, result = pcall(system.info)
    if ok and type(result) == "table" then
        info = result
    end

    local ip = nil
    local ip_ok, ip_result = pcall(system.ip)
    if ip_ok then
        ip = ip_result
    end

    local wifi_online = ip ~= nil and ip ~= ""
    local wifi_percent = wifi_percent_from_rssi(info.wifi_rssi)

    return {
        system_online = wifi_online,
        wifi_online = wifi_online,
        wifi_percent = wifi_online and wifi_percent or 0,
        ip = wifi_online and ip or nil,
    }
end

local function draw_hline(y)
    oled:fill_rect(0, y, cfg.oled_width, 1, true)
end

local function draw_crab_icon(x, y)
    oled:fill_rect(x + 7, y + 6, 10, 6, true)
    oled:fill_rect(x + 5, y + 8, 2, 2, true)
    oled:fill_rect(x + 17, y + 8, 2, 2, true)

    oled:pixel(x + 10, y + 7, false)
    oled:pixel(x + 13, y + 7, false)

    oled:fill_rect(x + 2, y + 4, 3, 2, true)
    oled:fill_rect(x + 19, y + 4, 3, 2, true)
    oled:pixel(x + 1, y + 3, true)
    oled:pixel(x + 22, y + 3, true)
    oled:pixel(x + 0, y + 2, true)
    oled:pixel(x + 23, y + 2, true)

    oled:pixel(x + 8, y + 12, true)
    oled:pixel(x + 10, y + 13, true)
    oled:pixel(x + 12, y + 13, true)
    oled:pixel(x + 14, y + 13, true)
    oled:pixel(x + 16, y + 12, true)
    oled:pixel(x + 6, y + 12, true)
    oled:pixel(x + 4, y + 13, true)
    oled:pixel(x + 18, y + 12, true)
    oled:pixel(x + 20, y + 13, true)
end

local function draw_label_value(y, label, value)
    oled:draw_text(0, y, label, true)
    oled:draw_text(54, y, value, true)
end

local function draw_ip_line(y, ip)
    oled:draw_text(0, y, "IP:", true)
    oled:draw_text(18, y, ip or "--", true)
end

local function states_equal(a, b)
    return a
        and b
        and a.relay1 == b.relay1
        and a.relay2 == b.relay2
end

local function oled_update(states)
    if not oled then
        return
    end
    if states_equal(last_display_state, states) and (last_refresh_ms % cfg.status_refresh_ms) ~= 0 then
        return
    end

    local runtime = read_runtime_status()

    oled:clear(false)
    oled:draw_text(0, 2, "ESP-CLAW", true)
    draw_crab_icon(102, 1)
    draw_hline(17)
    draw_label_value(22, "SISTEMA:", runtime.system_online and "ONLINE" or "OFFLINE")
    draw_label_value(32, "WIFI:", string.format("%d%%", runtime.wifi_percent))
    draw_label_value(42, "R1/R2:", string.format("%s/%s", states.relay1 and "ON" or "OFF", states.relay2 and "ON" or "OFF"))
    draw_ip_line(52, runtime.ip)
    oled:show()
    last_display_state = {
        relay1 = states.relay1,
        relay2 = states.relay2,
    }
end

local function cleanup()
    for i = 1, #btn_handles do
        local handle = btn_handles[i]
        if handle then
            pcall(button.off, handle)
            pcall(button.close, handle)
            btn_handles[i] = nil
        end
    end
    if oled then
        pcall(function() oled:close() end)
        oled = nil
    end
    if dev then
        pcall(function() dev:close() end)
        dev = nil
    end
    if bus then
        pcall(function() bus:close() end)
        bus = nil
    end
end

local function init_relay()
    shared.ensure_runtime(cfg)
    current_state = shared.reset_state(cfg)
end

local function init_button(relay_index, button_gpio, button_active_level)
    local handle, err = button.new(button_gpio, button_active_level)
    if not handle then
        error("[relay_service] button.new failed: " .. tostring(err))
    end
    btn_handles[#btn_handles + 1] = handle

    button.on(handle, "press_down", function()
        print(string.format("[relay_service] button%d press_down gpio=%d", relay_index, button_gpio))
    end)

    button.on(handle, "press_up", function()
        print(string.format("[relay_service] button%d press_up gpio=%d", relay_index, button_gpio))
    end)

    button.on(handle, "single_click", function()
        current_state = shared.toggle_state(cfg, relay_index)
        oled_update(current_state)
        print(string.format("[relay_service] button%d single_click => r1=%s r2=%s",
            relay_index,
            current_state.relay1 and "ON" or "OFF",
            current_state.relay2 and "ON" or "OFF"))
    end)
end

local function init_oled()
    if not cfg.enable_oled then
        print("[relay_service] OLED disabled by config")
        return
    end

    bus = i2c.new(cfg.i2c_port, cfg.i2c_sda, cfg.i2c_scl, cfg.i2c_freq_hz)
    local addrs = bus:scan()
    if not has_addr(addrs, cfg.oled_addr) then
        print(string.format("[relay_service] OLED not found at 0x%02X, running headless", cfg.oled_addr))
        pcall(function() bus:close() end)
        bus = nil
        return
    end

    dev = bus:device(cfg.oled_addr)
    local ssd1306 = load_ssd1306_driver()
    oled = ssd1306.new(dev, {
        width = cfg.oled_width,
        height = cfg.oled_height,
        addr = cfg.oled_addr,
    })
    oled:init()
    oled_update(current_state)
end

local function run()
    shared.ensure_runtime(cfg)
    current_state = shared.reset_state(cfg)
    print("[relay_service] starting " .. shared.format_status(cfg, current_state))
    init_relay()
    init_button(1, cfg.button_gpio, cfg.button_active_level)
    init_button(2, cfg.button2_gpio, cfg.button2_active_level)
    print(string.format("[relay_service] buttons ready: b1 gpio=%d active=%d | b2 gpio=%d active=%d",
        cfg.button_gpio,
        cfg.button_active_level,
        cfg.button2_gpio,
        cfg.button2_active_level))
    init_oled()

    while true do
        button.dispatch()

        last_refresh_ms = last_refresh_ms + cfg.poll_ms
        last_state_poll_ms = last_state_poll_ms + cfg.poll_ms

        if last_state_poll_ms >= 200 then
            last_state_poll_ms = 0
            local state_from_manager = shared.read_state(cfg)
            if not states_equal(state_from_manager, current_state) then
                current_state = state_from_manager
                oled_update(current_state)
                print(string.format("[relay_service] manager state => r1=%s r2=%s",
                    current_state.relay1 and "ON" or "OFF",
                    current_state.relay2 and "ON" or "OFF"))
            end
        end

        if oled and last_refresh_ms >= cfg.status_refresh_ms then
            last_refresh_ms = 0
            oled_update(current_state)
        end

        delay.delay_ms(math.min(cfg.poll_ms, 20))
    end
end

local ok, err = xpcall(run, debug.traceback)
cleanup()
if not ok then
    error(err)
end
