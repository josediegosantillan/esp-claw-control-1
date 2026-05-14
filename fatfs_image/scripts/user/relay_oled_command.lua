-- =============================================================
-- relay_oled_command.lua  v3.0
-- Basado en la arquitectura original: dofile + response global
-- =============================================================

local shared = dofile("/fatfs/scripts/user/relay_oled_shared.lua")

local cfg       = shared.load_config(args)
local requested = type(args) == "table" and args.action or "status"
local state, action = shared.apply_action(cfg, requested)

-- ─────────────────────────────────────────────────────────────
-- ESTADO DEL PORTÓN (ESP-NOW, archivo separado)
-- ─────────────────────────────────────────────────────────────

local function read_porton_state()
  local file = io.open("/fatfs/porton_state.txt", "r")
  if not file then return "DESCONOCIDO", "⚪" end
  local raw = file:read("*a")
  file:close()
  raw = type(raw) == "string" and string.lower(raw:gsub("%s+", "")) or ""
  if raw == "on"  then return "ENCENDIDA", "🟢" end
  if raw == "off" then return "APAGADA",   "⚫" end
  return "DESCONOCIDO", "⚪"
end

-- ─────────────────────────────────────────────────────────────
-- TECLADO INLINE (panel)
-- ─────────────────────────────────────────────────────────────

local PANEL_MARKUP = {
  inline_keyboard = {
    { { text = "💡 Taller",             callback_data = "noop"           } },
    { { text = "✅ Encender",           callback_data = "relay:on"       },
      { text = "⛔ Apagar",             callback_data = "relay:off"      } },
    { { text = "🌿 Patio",              callback_data = "noop"           } },
    { { text = "✅ Encender",           callback_data = "relay2:on"      },
      { text = "⛔ Apagar",             callback_data = "relay2:off"     } },
    { { text = "🚪 Luz portón",         callback_data = "noop"           } },
    { { text = "✅ Encender",           callback_data = "porton:on"      },
      { text = "⛔ Apagar",             callback_data = "porton:off"     } },
    { { text = "📊 Estado general",     callback_data = "status:all"     } },
    { { text = "🔄 Actualizar panel",   callback_data = "panel:refresh"  } },
  }
}

-- Acciones que muestran el panel completo después de ejecutarse
local PANEL_ACTIONS = {
  on = true, off = true, toggle = true,
  encender = true, apagar = true
}

-- status:all se trata igual que status
if requested == "status:all" then requested = "status" end

-- ─────────────────────────────────────────────────────────────
-- HELPERS DE FORMATO
-- ─────────────────────────────────────────────────────────────

local function relay_line(on, name)
  return (on and "🟢" or "⚫") .. " *" .. name .. ":* " .. (on and "ENCENDIDA" or "APAGADA")
end

local function build_status_lines(s)
  local r1 = s and s.relay1
  local r2 = s and s.relay2
  local porton_label, porton_icon = read_porton_state()
  return table.concat({
    relay_line(r1, "Taller  (R1)"),
    relay_line(r2, "Patio   (R2)"),
    porton_icon .. " *Luz portón:* " .. porton_label,
  }, "\n")
end

local ACTION_VERB = {
  on       = "encendida",
  encender = "encendida",
  off      = "apagada",
  apagar   = "apagada",
  toggle   = "alternada",
}

local RELAY_NAME = { [1] = "Taller", [2] = "Patio" }

-- ─────────────────────────────────────────────────────────────
-- LÓGICA PRINCIPAL
-- ─────────────────────────────────────────────────────────────

if requested == "menu" or requested == "panel" then
  -- ── PANEL / MENÚ ─────────────────────────────────────────
  response = {
    text = "🦞 *ESP-Claw* — Panel de control\nTaller = Relé 1   •   Patio = Relé 2   •   Luz portón = ESP-NOW",
    reply_markup = PANEL_MARKUP
  }

elseif requested == "help" or requested == "ayuda" then
  -- ── AYUDA ────────────────────────────────────────────────
  response = {
    text = table.concat({
      "🦞 *ESP-Claw* — Comandos disponibles",
      "",
      "*/panel*  — panel con botones",
      "*/estado* - estado general de reles",
      "*/menu*   - alias de /panel",
      "*/relay on|off|toggle|status*",
      "*/relay2 on|off|toggle|status*",
      "*/porton on|off|toggle|status*",
      "",
      "Lenguaje natural:",
      "  encender / apagar luz taller",
      "  encender / apagar luz patio",
      "  encender / apagar luz porton",
    }, "\n")
  }

elseif requested == "status" then
  -- ── ESTADO GENERAL ───────────────────────────────────────
  response = {
    text = table.concat({
      "📊 *Estado actual*",
      "",
      build_status_lines(state),
      "",
      "_Portón: último estado confirmado por el esclavo._",
    }, "\n"),
    reply_markup = PANEL_MARKUP
  }

elseif PANEL_ACTIONS[requested] then
  -- ── ACCIÓN EJECUTADA → confirma + muestra panel ──────────
  local ridx  = type(args) == "table" and (args.relay_index or 1) or 1
  local rname = RELAY_NAME[ridx] or "Relé"
  local verb  = ACTION_VERB[requested] or requested
  local r_on  = state and ((ridx == 1 and state.relay1) or (ridx == 2 and state.relay2))
  local icon  = r_on and "✅" or "⛔"

  response = {
    text = table.concat({
      icon .. " *" .. rname .. " " .. verb .. "*",
      "",
      build_status_lines(state),
    }, "\n"),
    reply_markup = PANEL_MARKUP
  }

else
  -- ── FALLBACK ─────────────────────────────────────────────
  response = {
    text = shared.format_action_response(cfg, state, action),
    reply_markup = PANEL_MARKUP
  }
end

if response and response.text then
  print(response.text)
end
