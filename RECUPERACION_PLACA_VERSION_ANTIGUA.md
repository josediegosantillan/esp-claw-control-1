# Recuperacion De Placa Con Version Antigua

Esta guia sirve para cargar este proyecto en una `ESP32-S3` que ya tenia una version anterior del mismo firmware o una variante cercana.

## Recomendacion

Si la placa viene de una version vieja, conviene **flashearla integra**.

Motivo:

- la particion `storage` puede traer archivos incompatibles con la version actual
- eso puede romper arranque parcial aunque el binario principal compile y flashee bien
- los errores tipicos son:
  - `Failed to parse memory index: /fatfs/memory/memory_index.json`
  - `Scheduler init load failed`
  - `ESP_FAIL` al usar el agente
  - configuraciones viejas de Wi-Fi, LLM, Telegram o memoria persistente

En una placa con version antigua, lo mas seguro es:

1. borrar toda la flash
2. grabar `bootloader`, `partition-table`, `ota_data`, `app` y `storage.bin`

## Cuando No Hace Falta Flasheo Integro

Solo conviene evitar el borrado total si necesitas preservar datos de la placa vieja, por ejemplo:

- credenciales Wi-Fi
- configuracion LLM
- token de Telegram
- historial o memoria persistente

En ese caso hay que hacer una migracion selectiva, pero es mas riesgoso y puede dejar archivos incompatibles.

## Procedimiento Recomendado

### 1. Compilar

```powershell
idf.py build
```

### 2. Borrar Toda La Flash

Reemplaza `COM10` por el puerto correcto si cambia.

```powershell
idf.py -p COM10 erase-flash
```

### 3. Grabar Todo

```powershell
C:\Espressif\tools\python\v5.5.4\venv\Scripts\python.exe C:\esp\v5.5.4\esp-idf\components\esptool_py\esptool\esptool.py -p COM10 -b 460800 --before default_reset --after hard_reset --chip esp32s3 write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 build\bootloader\bootloader.bin 0x20000 build\edge_agent.bin 0x8000 build\partition_table\partition-table.bin 0xf000 build\ota_data_initial.bin 0xb20000 build\storage.bin
```

### 4. Abrir Monitor

```powershell
idf.py -p COM10 monitor
```

## Que Se Pierde Con El Borrado Total

- credenciales Wi-Fi guardadas
- configuracion LLM
- token de Telegram
- memoria persistente
- sesiones e indice de memoria
- cualquier archivo previo en `/fatfs`

## Que Debe Pasar Despues Del Flasheo Limpio

Si no hay Wi-Fi guardado, la placa normalmente levantara el portal de provision:

- SSID: `esp-claw-xxxxx`
- IP: `192.168.4.1`

Luego debes volver a configurar:

1. Wi-Fi
2. LLM
3. Telegram
4. cualquier ajuste extra del proyecto

## Verificaciones Utiles

Despues del primer arranque, conviene revisar que ya no aparezcan errores como:

- `Failed to parse memory index`
- `Scheduler init load failed`
- `LLM is not fully configured` cuando ya guardaste la config

Y confirmar que aparezcan mensajes sanos como:

- `Wi-Fi STA ready`
- `Starting LLM provider=...`
- `claw_core: Initialized`
- `Time sync succeeded`

## Nota Practica

Para placas con version anterior de este proyecto, la respuesta corta es:

**Si, conviene flashearlo integro.**

Es el camino mas rapido, mas limpio y con menos riesgo de arrastrar archivos incompatibles en `storage`.
