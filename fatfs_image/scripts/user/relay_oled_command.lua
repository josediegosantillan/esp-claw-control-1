local shared = dofile("/fatfs/scripts/user/relay_oled_shared.lua")

local cfg = shared.load_config(args)
local requested = type(args) == "table" and args.action or "status"
local state, action = shared.apply_action(cfg, requested)

local function read_porton_estimated_state()
  local file = io.open("/fatfs/porton_state.txt", "r")
  if not file then
    return "DESCONOCIDO", "⚪"
  end

  local raw = file:read("*a")
  file:close()
  raw = type(raw) == "string" and string.lower((raw:gsub("%s+", ""))) or ""
  if raw == "on" then
    return "ENCENDIDA", "🟢"
  end
  if raw == "off" then
    return "APAGADA", "⚫"
  end
  return "DESCONOCIDO", "⚪"
end

-- Panel inline keyboard (same markup used by /panel and after every action)
local PANEL_MARKUP = {
  inline_keyboard = {
    {
      { text = "💡 Taller",       callback_data = "noop" }
    },
    {
      { text = "✅ Encender",     callback_data = "relay:on"     },
      { text = "⛔ Apagar",       callback_data = "relay:off"    }
    },
    {
      { text = "🔀 Alternar",     callback_data = "relay:toggle" },
      { text = "🔍 Estado",       callback_data = "relay:status" }
    },
    {
      { text = "🌿 Patio",        callback_data = "noop" }
    },
    {
      { text = "✅ Encender",     callback_data = "relay2:on"     },
      { text = "⛔ Apagar",       callback_data = "relay2:off"    }
    },
    {
      { text = "🔀 Alternar",     callback_data = "relay2:toggle" },
      { text = "🔍 Estado",       callback_data = "relay2:status" }
    },
    {
      { text = "🚪 Luz porton",   callback_data = "noop" }
    },
    {
      { text = "✅ Encender",     callback_data = "porton:on"     },
      { text = "⛔ Apagar",       callback_data = "porton:off"    }
    },
    {
      { text = "🔀 Alternar",     callback_data = "porton:toggle" },
      { text = "🔍 Estado",       callback_data = "porton:status" }
    },
    {
      { text = "🔄 Actualizar panel", callback_data = "panel:refresh" }
    }
  }
}

-- Actions that should reply with a panel after executing
local PANEL_ACTIONS = {
  on = true, off = true, toggle = true,
  encender = true, apagar = true
}

if requested == "menu" then
  -- /menu: send a fresh panel message
  response = {
    text = "🦞 *ESP-Claw* — Panel de control\nTaller = Relé 1   •   Patio = Relé 2   •   Luz porton = ESP-NOW",
    reply_markup = PANEL_MARKUP
  }

elseif requested == "help" or requested == "ayuda" then
  response = {
    text = table.concat({
      "🦞 *ESP-Claw* — Comandos disponibles",
      "",
      "*/panel*  — abre el panel con botones",
      "*/menu*   — igual que /panel",
      "*/relay on|off|toggle|status*",
      "*/relay2 on|off|toggle|status*",
      "*/porton on|off|toggle|status*",
      "",
      "Lenguaje natural:",
      "  encender luz taller / apagar luz taller",
      "  encender luz patio  / apagar luz patio",
      "  Luz taller estado   / Luz patio estado",
      "  encender luz porton / apagar luz porton",
      "  Luz porton estado",
    }, "\n")
  }

elseif requested == "status" then
  -- Read both relay states from shared state
  local r1 = state and state.relay1
  local r2 = state and state.relay2
  local porton_label, porton_icon = read_porton_estimated_state()
  local icon1 = r1 and "🟢" or "⚫"
  local icon2 = r2 and "🟢" or "⚫"
  local label1 = r1 and "ENCENDIDA" or "APAGADA"
  local label2 = r2 and "ENCENDIDA" or "APAGADA"

  response = {
    text = table.concat({
      "📊 *Estado actual*",
      "",
      icon1 .. " *Taller (R1):* " .. label1,
      icon2 .. " *Patio  (R2):* " .. label2,
      porton_icon .. " *Luz porton:* " .. porton_label,
      "_Porton: estado estimado segun ultimo comando enviado._",
    }, "\n"),
    reply_markup = PANEL_MARKUP
  }

elseif PANEL_ACTIONS[requested] then
  -- Action was executed: confirm and re-send panel
  local base = shared.format_action_response(cfg, state, action)
  response = {
    text = base,
    reply_markup = PANEL_MARKUP
  }

else
  -- Fallback: just use the default shared response
  print(shared.format_action_response(cfg, state, action))
end

if response and response.text then
  print(response.text)
end
