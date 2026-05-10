# Relay, OLED y Telegram

Este proyecto incluye una base para controlar dos reles con:

- dos pulsadores fisicos en GPIO
- pantalla OLED SSD1306 I2C de 0.96"
- comandos por Telegram

Ademas, el panel de Telegram puede controlar una `luz porton` remota por `ESP-NOW` hacia un ESP32 esclavo.

## Pines por defecto

- Rele 1: `GPIO4`
- Boton 1: `GPIO5`
- Rele 2: `GPIO6`
- Boton 2: `GPIO7`
- OLED SDA: `GPIO8`
- OLED SCL: `GPIO9`
- I2C port: `0`
- OLED address: `0x3C`

Si queres otros GPIO, se cambian en:

- la regla `relay_oled_service_startup`
- las reglas `telegram_relay_*`
- las reglas `telegram_relay2_*`

## Cableado sugerido

### OLED SSD1306 I2C 0.96

- `VCC` -> `3.3V`
- `GND` -> `GND`
- `SDA` -> `GPIO8`
- `SCL` -> `GPIO9`

### Pulsadores fisicos

- un lado del pulsador 1 -> `GPIO5`
- un lado del pulsador 2 -> `GPIO7`
- el otro lado de ambos -> `GND`

El script asume pulsadores activos en nivel bajo.

Notas:

- el componente `button` habilita pull-up interno para entradas activas en bajo
- si tu montaje es largo o ruidoso, igual puede convenir un pull-up externo de `10k`
- el evento usado es `single_click`, por eso la respuesta del boton no es instantanea como Telegram

### Reles

- `VCC` del modulo -> `5V`
- `GND` del modulo -> `GND`
- `S` del rele 1 -> `GPIO4`
- `S` del rele 2 -> `GPIO6`

Notas:

- no alimentes la bobina del rele desde `3.3V`
- el `GND` del ESP32 y el `GND` del modulo rele deben estar en comun
- muchos modulos aceptan senal de `3.3V` en `S`, pero no siempre es garantizado
- si el disparo es inestable, usa un transistor, driver o level shifter

## Polaridad actual

- rele 1 (`GPIO4`) activo en alto
- rele 2 (`GPIO6`) activo en bajo

## Comandos de Telegram

Las reglas agregadas escuchan en canal `telegram`:

- `/relay on`
- `/relay off`
- `/relay toggle`
- `/relay status`
- `/relay2 on`
- `/relay2 off`
- `/relay2 toggle`
- `/relay2 status`
- `/relay encender`
- `/relay apagar`
- `/relay estado`
- `/porton on`
- `/porton off`
- `/porton toggle`
- `/porton status`

Tambien hay alias de lenguaje natural ya cargados para luz de taller y luz de patio.
Tambien hay alias para:

- `encender luz porton`
- `apagar luz porton`
- `Luz porton estado`

## Arranque automatico

En cada boot se ejecuta el servicio:

- `user/relay_oled_service.lua`

Comportamiento actual al arrancar:

- configura ambos GPIO de rele como salida
- fuerza ambos reles a `OFF`
- sobrescribe `/fatfs/relay_state.txt` con ambos reles apagados
- despues habilita botones, OLED y monitoreo de estado

Consecuencia importante:

- si se corta la energia, al volver no se restaura el ultimo estado previo
- el sistema siempre arranca con ambos reles apagados
- no debe haber un pulso valido de encendido durante el boot

## Estado compartido de los reles

La fuente de verdad actual para el control normal no es memoria volatil ni lectura directa del GPIO.

Ahora se usa:

- `/fatfs/relay_state.txt`

Puntos importantes:

- `relay_oled_shared.lua` persiste `relay1` y `relay2`
- `write_state()` actualiza el archivo y luego aplica el nivel correcto en GPIO
- `toggle_state()` conmuta segun el estado persistido compartido
- botones y Telegram usan la misma fuente de verdad
- `luz porton` no usa GPIO local; envia payloads `ESP-NOW` al esclavo y guarda un estado estimado en `/fatfs/porton_state.txt`
- cuando el esclavo responde `ok relay=on` o `ok relay=off`, el maestro reenvia esa confirmacion al ultimo chat de Telegram que emitio el comando, usando `/fatfs/porton_last_chat.txt`

Consecuencia operativa:

- si Telegram cambia un rele, el servicio de botones ve ese cambio
- si un boton cambia un rele, Telegram informa el mismo estado
- al iniciar, el archivo se resetea a ambos `OFF`

## Scripts agregados

- `fatfs_image/scripts/user/relay_oled_shared.lua`
- `fatfs_image/scripts/user/relay_oled_command.lua`
- `fatfs_image/scripts/user/relay_oled_service.lua`

## OLED

La pantalla SSD1306 de `128x64` muestra:

- encabezado `ESP-CLAW`
- icono simple tipo cangrejo a la derecha
- linea divisoria horizontal
- `SISTEMA: ONLINE/OFFLINE`
- `WIFI: xx%`
- `R1/R2: ON/OFF`
- `IP: <direccion-ip>` o `--`

## De donde sale cada dato

- `SISTEMA` se considera `ONLINE` cuando `system.ip()` devuelve una IP valida
- `WIFI` se calcula como porcentaje a partir de `system.info().wifi_rssi`
- `R1/R2` sale del estado persistido actual compartido por botones y Telegram
- `IP` muestra la IP STA real si existe y `--` si no la hay

## Latencia esperable del boton

Los botones no conmuntan el rele por interrupcion inmediata.

La demora visible sale de tres cosas:

- `button.dispatch()` funciona por sondeo
- el evento configurado es `single_click`
- hay debounce y una ventana de confirmacion del click

Por eso Telegram normalmente responde mas rapido que el boton fisico.

## Portal de provisioning

Durante el arranque, el equipo puede levantar un AP de provisioning con SSID tipo:

- `esp-claw-xxxxxx`

Comportamiento actual:

- si el equipo todavia no logra conectar por STA, el AP de provisioning queda activo como fallback
- si el equipo conecta correctamente a la red Wi-Fi configurada, el firmware fuerza modo `STA` y apaga el AP de provisioning

Consecuencia:

- la red `esp-claw-xxxxxx` no deberia seguir visible una vez que la conexion STA quedo establecida
- si despues se cae la red Wi-Fi, ya no queda el portal local inmediatamente activo salvo que el flujo de Wi-Fi vuelva a levantarlo
