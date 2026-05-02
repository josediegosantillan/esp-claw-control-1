# Relay, OLED y Telegram

Este proyecto incluye una base para controlar un relé con:

- botón físico en un GPIO
- pantalla OLED SSD1306 I2C de 0.96"
- comandos por Telegram

## Pines por defecto

- Relé: `GPIO4`
- Botón: `GPIO5`
- OLED SDA: `GPIO8`
- OLED SCL: `GPIO9`
- I2C port: `0`
- OLED address: `0x3C`

Si querés otros GPIO, se cambian en:

- la regla `relay_oled_service_startup`
- las reglas `telegram_relay_*`

## Cableado sugerido

### OLED SSD1306 I2C 0.96

- `VCC` -> `3.3V`
- `GND` -> `GND`
- `SDA` -> `GPIO8`
- `SCL` -> `GPIO9`

### Botón físico

- un lado del botón -> `GPIO5`
- otro lado -> `GND`

Usá un pull-up externo a `3.3V` de `10k` si tu montaje no tiene uno.
El script asume botón activo en nivel bajo.

### Relé

- `VCC` del módulo -> `5V`
- `GND` del módulo -> `GND`
- `S` -> `GPIO4`

Notas:

- no alimentes la bobina del relé desde `3.3V`
- el `GND` del ESP32 y el `GND` del módulo relé deben estar en común
- muchos módulos aceptan señal de `3.3V` en `S`, pero no siempre es garantizado
- si el disparo es inestable, usá un transistor, driver o level shifter

## Comandos de Telegram

Las reglas agregadas escuchan en canal `telegram`:

- `/relay on`
- `/relay off`
- `/relay toggle`
- `/relay status`
- `/relay encender`
- `/relay apagar`
- `/relay estado`

## Arranque automático

En cada boot se ejecuta el servicio:

- `user/relay_oled_service.lua`

Ese servicio:

- consulta y aplica el estado real del relé a través de `relay_manager`
- atiende el botón físico
- actualiza el OLED
- refresca la vista con estado real de red y relé

## Portal de provisioning

Durante el arranque, el equipo puede levantar un AP de provisioning con SSID tipo:

- `esp-claw-xxxxxx`

Comportamiento actual:

- si el equipo todavía no logra conectar por STA, el AP de provisioning queda activo como fallback
- si el equipo conecta correctamente a la red Wi-Fi configurada, el firmware fuerza modo `STA` y apaga el AP de provisioning

Consecuencia:

- la red `esp-claw-xxxxxx` no debería seguir visible una vez que la conexión STA quedó establecida
- si después se cae la red Wi-Fi, ya no queda el portal local inmediatamente activo salvo que el flujo de Wi-Fi vuelva a levantarlo

## Scripts agregados

- `fatfs_image/scripts/user/relay_oled_shared.lua`
- `fatfs_image/scripts/user/relay_oled_command.lua`
- `fatfs_image/scripts/user/relay_oled_service.lua`

## Fuente de verdad del relé

La fuente de verdad ya no es un archivo de texto compartido.
Ahora el estado real vive en `relay_manager`, y los scripts Lua operan sobre ese runtime compartido.

Puntos importantes:

- `relay_oled_shared.lua` llama a `relay_manager.configure(...)`
- `read_state()` usa `relay_manager.get()`
- `write_state()` usa `relay_manager.set(...)`
- `toggle_state()` usa `relay_manager.toggle()`

Consecuencia operativa:

- nunca asumir que el relé sigue en el último estado pedido por Telegram
- antes de responder `ON` u `OFF`, consultar siempre el estado real actual
- cuando se presiona el botón, también cambia el mismo estado compartido que usan los comandos remotos

## Layout actual del OLED

La pantalla SSD1306 de `128x64` muestra un layout compacto inspirado en el boceto del proyecto:

- encabezado `ESP-CLAW`
- ícono simple tipo cangrejo a la derecha
- línea divisoria horizontal
- `SISTEMA: ONLINE/OFFLINE`
- `WIFI: xx%`
- `RELE: ON/OFF`
- `IP: <direccion-ip>` o `--`

## De dónde sale cada dato

- `SISTEMA` se considera `ONLINE` cuando `system.ip()` devuelve una IP válida
- `WIFI` se calcula como porcentaje a partir de `system.info().wifi_rssi`
- `RELE` sale del estado real expuesto por `relay_manager`
- `IP` muestra la IP STA real si existe y `--` si no la hay

## Nota sobre la “lección aprendida”

La regla sigue siendo la misma, aunque cambió la implementación:

- no responder el estado del relé por memoria o por suposición
- leer siempre la fuente de verdad actual antes de informar el estado

Antes esa fuente era `relay_state.txt`.
Ahora esa fuente es `relay_manager`.
