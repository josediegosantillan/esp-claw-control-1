-- relay_timer.lua  v1.0
-- Enciende el relé indicado, espera N minutos y lo apaga.
-- Notifica por Telegram al inicio y al finalizar.

local capability = require("capability")
local delay      = require("delay")
local shared     = dofile("/fatfs/scripts/user/relay_oled_shared.lua")

local a = type(args) == "table" and args or {}

local function int_arg(key, default)
    local v = a[key]
    return type(v) == "number" and math.floor(v) or default
end

local function str_arg(key, default)
    local v = a[key]
    return (type(v) == "string" and v ~= "") and v or default
end

local relay_index = int_arg("relay_index", 1)
local minutes     = int_arg("minutes", 5)
local chat_id     = str_arg("chat_id", "")

local RELAY_NAME  = { [1] = "Taller", [2] = "Patio" }
local relay_name  = RELAY_NAME[relay_index] or "Relé"

local cfg = shared.load_config(a)

local function send_tg(text)
    if chat_id == "" then return end
    local ok, out, err = capability.call(
        "tg_send_message",
        { chat_id = chat_id, message = text, parse_mode = "Markdown" },
        { channel = "telegram", chat_id = chat_id }
    )
    if not ok then
        print(string.format("[relay_timer] tg_send_message failed: %s | %s",
            tostring(err), tostring(out)))
    end
end

-- 1. Encender el relé
shared.write_state(cfg, relay_index, true)

-- 2. Notificar inicio
send_tg(string.format(
    "⏱ *Temporizador activado*\n_%s_ encendida — se apagará en *%d min*.",
    relay_name, minutes))

-- 3. Esperar
delay.ms(minutes * 60 * 1000)

-- 4. Apagar el relé
shared.write_state(cfg, relay_index, false)

-- 5. Notificar fin
send_tg(string.format(
    "✅ *Temporizador completado*\n_%s_ apagada automáticamente.",
    relay_name))
