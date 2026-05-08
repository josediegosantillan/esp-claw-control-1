local shared = dofile("/fatfs/scripts/user/relay_oled_shared.lua")

local cfg = shared.load_config(args)
local requested = type(args) == "table" and args.action or "status"
local state, action = shared.apply_action(cfg, requested)

print(shared.format_action_response(cfg, state, action))
