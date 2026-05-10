local shared = dofile("/fatfs/scripts/user/relay_oled_shared.lua")

local message = "Prueba reply_markup desde Lua"
if type(args) == "table" and type(args.message) == "string" and args.message ~= "" then
    message = args.message
end

local payload = {
    message = message,
    parse_mode = "Markdown",
    reply_markup = shared.telegram_panel_markup(),
}

if not shared.can_send_telegram(args) then
    error("[telegram_reply_markup_probe] missing telegram channel/chat_id in args")
end

local ok, out, err = shared.send_telegram_message(args, payload)
if not ok then
    error(string.format("[telegram_reply_markup_probe] tg_send_message failed: %s | %s",
        tostring(err), tostring(out)))
end

print("OK telegram reply_markup probe sent")
