# Guia para recuperar y mantener `edge_agent`

## Objetivo

Esta guia deja un flujo reproducible para trabajar sobre este repo sin mezclar configuraciones de placa, `sdkconfig` y artefactos de build.

Cubre:

- como preparar el entorno en Windows
- como regenerar la configuracion de placa con `esp_board_manager`
- como validar la configuracion efectiva antes de compilar
- como hacer un build limpio
- como flashear sin usar offsets viejos
- que archivos son fuente y cuales son generados
- que comportamiento actual tienen relays, botones y almacenamiento FATFS

## Estado real del repo hoy

Estos puntos salen del contenido versionado y de los artefactos actuales de este workspace:

- ESP-IDF recomendado: `5.5.4`
- ruta real del proyecto: `C:\esp_projects\esp_claw_control\esp-claw\application\edge_agent`
- placa actualmente generada en `components/gen_bmgr_codes`: `esp32_S3_DevKitC_1`
- target efectivo en `board_manager.defaults`: `esp32s3`
- flash declarada en `board_manager.defaults`: `16MB`
- perfil declarado en `board_manager.defaults`: `QIO` y `120M`
- perfil efectivo del build actual: `dio` y `80m`
- particion base en `sdkconfig.defaults`: `partitions_16MB.csv`

Referencia actual:

```ini
# components/gen_bmgr_codes/board_manager.defaults
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESP_BOARD_NAME="esp32_S3_DevKitC_1"
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHFREQ_120M=y
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_SPIRAM_MODE_OCT=y
```

```ini
# sdkconfig.defaults
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_16MB.csv"
CONFIG_LV_BUILD_EXAMPLES=n
CONFIG_LV_BUILD_DEMOS=n
```

```json
# build/flasher_args.json
"flash_mode": "dio"
"flash_size": "16MB"
"flash_freq": "80m"
```

## Checklist rapido

Usa esta secuencia para dejar el proyecto listo para compilar y flashear con la placa actualmente soportada por el repo.

```powershell
Set-Location C:\esp_projects\esp_claw_control\esp-claw\application\edge_agent
$env:IDF_PATH='C:\esp\v5.5.4\esp-idf'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
$env:IDF_PYTHON_ENV_PATH='C:\Espressif\tools\python\v5.5.4\venv'
$env:IDF_EXTRA_ACTIONS_PATH='C:\esp_projects\esp_claw_control\esp-claw\application\edge_agent\managed_components\espressif__esp_board_manager'
$env:PYTHONUTF8='1'
$env:PYTHONIOENCODING='utf-8'
idf.py bmgr -l
idf.py bmgr -c ./boards -b esp32_S3_DevKitC_1
Remove-Item -Recurse -Force .\build -ErrorAction SilentlyContinue
idf.py build
idf.py flash monitor
```

Resultado esperado:

- `bmgr` lista placas disponibles
- `components/gen_bmgr_codes` queda regenerado
- `board_manager.defaults` converge en `esp32s3`
- el build usa la placa `esp32_S3_DevKitC_1`
- el flasheo usa los argumentos del build actual

## 1. Preparar el entorno

### Opcion recomendada

Abrir una terminal de ESP-IDF ya exportada y trabajar desde la raiz real del proyecto:

```powershell
Set-Location C:\esp_projects\esp_claw_control\esp-claw\application\edge_agent
```

### Variables necesarias

`idf.py bmgr` depende de que `IDF_EXTRA_ACTIONS_PATH` apunte al componente descargado en `managed_components`.

```powershell
$env:IDF_PATH='C:\esp\v5.5.4\esp-idf'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
$env:IDF_PYTHON_ENV_PATH='C:\Espressif\tools\python\v5.5.4\venv'
$env:IDF_EXTRA_ACTIONS_PATH='C:\esp_projects\esp_claw_control\esp-claw\application\edge_agent\managed_components\espressif__esp_board_manager'
$env:PYTHONUTF8='1'
$env:PYTHONIOENCODING='utf-8'
Set-Location 'C:\esp_projects\esp_claw_control\esp-claw\application\edge_agent'
```

Si se usa una PowerShell normal y no una terminal de ESP-IDF, tambien hay que asegurar que `cmake`, `ninja` y las toolchains esten en `PATH`.

## 2. Seleccionar la placa correcta

La placa soportada y alineada con este repo es:

```text
esp32_S3_DevKitC_1
```

Antes de regenerar, conviene listar las placas visibles:

```powershell
idf.py bmgr -l
```

Comando recomendado:

```powershell
idf.py bmgr -c ./boards -b esp32_S3_DevKitC_1
```

Notas:

- `idf.py bmgr` es el flujo preferido
- `idf.py gen-bmgr-config` sigue existiendo como alias legacy
- no documentes ni uses placas que no existan en `boards/`

## 3. Limpiar antes de recompilar

Cuando hubo cambios de placa, target, `sdkconfig` o particiones, no conviene reutilizar un `build` viejo.

Secuencia recomendada:

1. regenerar con `idf.py bmgr -c ./boards -b esp32_S3_DevKitC_1`
2. borrar `build`
3. compilar desde cero

Comandos:

```powershell
idf.py bmgr -c ./boards -b esp32_S3_DevKitC_1
Remove-Item -Recurse -Force .\build -ErrorAction SilentlyContinue
idf.py build
```

## 4. Verificar la configuracion efectiva

Antes de construir, revisar estos puntos:

- `components/gen_bmgr_codes/board_manager.defaults` debe declarar `CONFIG_IDF_TARGET="esp32s3"`
- `components/gen_bmgr_codes/board_manager.defaults` debe declarar `CONFIG_ESP_BOARD_NAME="esp32_S3_DevKitC_1"`
- `components/gen_bmgr_codes/board_manager.defaults` debe reflejar `16MB`
- `sdkconfig.defaults` no debe contradecir el layout esperado
- `sdkconfig` efectivo manda sobre el perfil final de flash que termina en `build/flasher_args.json`

Importante:

- este repo incluye `tools/cmake/flash_partition_defaults.cmake`
- ese script deriva `partitions_<flash_size>.csv` a partir de `components/gen_bmgr_codes/board_manager.defaults`
- por eso, si cambia `CONFIG_ESPTOOLPY_FLASHSIZE_*`, hay que volver a regenerar y recompilar
- `board_manager.defaults` puede no coincidir con el `flash_mode` y `flash_freq` finales si `sdkconfig` preserva otros valores efectivos

Si necesitas cargar credenciales y defaults funcionales:

```powershell
idf.py menuconfig
```

Parametros que suelen requerir ajuste:

- Wi-Fi SSID y password
- proveedor, clave y modelo de LLM
- tokens de Telegram o QQ
- claves de busqueda web
- zona horaria

## 5. Build limpio recomendado

Flujo normal:

```powershell
idf.py build
```

Artefactos actuales esperables:

- `edge_agent.bin`
- `build/flasher_args.json`
- `build/flash_project_args`

Layout actual:

```text
flash mode: dio
flash freq: 80m
flash size: 16MB
0x0       bootloader/bootloader.bin
0x8000    partition_table/partition-table.bin
0xf000    ota_data_initial.bin
0x20000   edge_agent.bin
0xb20000  storage.bin
```

Notas practicas:

- en este proyecto conviene dejar `CONFIG_LV_BUILD_EXAMPLES=n`
- en este proyecto conviene dejar `CONFIG_LV_BUILD_DEMOS=n`
- con demos y ejemplos activos, el build en Windows crece mucho y puede fallar al empaquetar `lvgl` o `freetype`

## 6. Flasheo recomendado

### Camino simple

```powershell
idf.py flash monitor
```

### Camino manual

Solo usarlo si necesitas reproducir exactamente los argumentos del build actual.

Fuente de verdad:

- `build/flasher_args.json`
- `build/flash_project_args`

Regla operativa:

- no reutilizar offsets copiados de otra corrida
- no mezclar perfiles de `8MB` y `16MB`
- no asumir `dio 40m` o cualquier perfil viejo sin revisar el build actual

## 7. Estado actual de relays y botones

Configuracion actual:

- rele 1 en `GPIO4`, activo en alto
- rele 2 en `GPIO6`, activo en bajo
- boton 1 en `GPIO5`, activo en bajo
- boton 2 en `GPIO7`, activo en bajo

Comportamiento actual:

- en cada boot ambos reles se fuerzan a `OFF`
- durante el uso normal el estado compartido se guarda en `/fatfs/relay_state.txt`
- botones y Telegram operan sobre el mismo estado persistido
- si se corta la energia, el sistema no restaura el ultimo estado; vuelve con ambos reles apagados

Nota operativa:

- el boton fisico puede responder mas lento que Telegram porque usa `single_click` por sondeo y debounce

## 8. Como modificar la definicion de placa

La variante actualmente usada vive en:

- `boards/espressif/esp32_S3_DevKitC_1/board_info.yaml`
- `boards/espressif/esp32_S3_DevKitC_1/board_devices.yaml`
- `boards/espressif/esp32_S3_DevKitC_1/board_peripherals.yaml`
- `boards/espressif/esp32_S3_DevKitC_1/sdkconfig.defaults.board`
- `boards/espressif/esp32_S3_DevKitC_1/setup_device.c`

Flujo seguro:

1. editar los archivos fuente en `boards/espressif/esp32_S3_DevKitC_1/`
2. volver a correr `idf.py bmgr -c ./boards -b esp32_S3_DevKitC_1`
3. validar el resultado generado en `components/gen_bmgr_codes/board_manager.defaults`
4. borrar `build`
5. recompilar

No editar a mano `components/gen_bmgr_codes` como solucion permanente, porque es codigo generado.

## 9. Codigo fuente vs codigo generado

Archivos fuente del proyecto:

- `boards/...`
- `sdkconfig.defaults`
- `partitions_16MB.csv`
- `partitions_8MB.csv`
- `tools/cmake/esp_idf_patch.cmake`
- `tools/cmake/flash_partition_defaults.cmake`
- `tools/cmake/board_manager_patch.cmake`
- `tools/bmgr_patch.py`
- `fatfs_image/scripts/user/*`
- `fatfs_image/router_rules/router_rules.json`

Archivos generados o regenerables:

- `components/gen_bmgr_codes/*`
- `build/*`
- `sdkconfig` cuando exista localmente

Regla de trabajo:

- los cambios duraderos van a los archivos fuente
- lo generado se valida, pero no se toma como punto de edicion manual

## 10. Problemas conocidos y prevencion

### Problema 1. `bmgr` no existe o no reconoce `-c`

Causa probable: `IDF_EXTRA_ACTIONS_PATH` no apunta a `managed_components/espressif__esp_board_manager`.

Correccion:

```powershell
$env:IDF_EXTRA_ACTIONS_PATH='C:\esp_projects\esp_claw_control\esp-claw\application\edge_agent\managed_components\espressif__esp_board_manager'
idf.py bmgr -l
idf.py bmgr -c ./boards -b esp32_S3_DevKitC_1
```

### Problema 2. El target termina en otro chip

Causa probable: `sdkconfig` o `build` arrastrados de una corrida previa.

Correccion:

```powershell
idf.py bmgr -c ./boards -b esp32_S3_DevKitC_1
Remove-Item -Recurse -Force .\build -ErrorAction SilentlyContinue
idf.py build
```

### Problema 3. El layout de flash queda inconsistente

Causa probable: mezcla entre `board_manager.defaults`, `sdkconfig.defaults` y offsets manuales viejos.

Correccion:

- regenerar con `bmgr`
- borrar `build`
- recompilar
- tomar offsets solo desde los artefactos del build actual

### Problema 4. El build falla o se vuelve enorme por LVGL

Causa probable:

- `CONFIG_LV_BUILD_EXAMPLES=y`
- `CONFIG_LV_BUILD_DEMOS=y`

Correccion:

- dejar ambos en `n` en `sdkconfig.defaults`
- si `sdkconfig` local ya existe, desactivarlos tambien ahi
- borrar `build`
- recompilar

### Problema 5. Warning de `ESP_ROM_ELF_DIR` al final del build

Sintoma:

- el build termina bien pero CMake deja un warning al generar `gdbinit`

Impacto:

- no bloquea la compilacion
- no bloquea el binario final
- solo afecta la generacion auxiliar de `gdbinit`

## 11. Secuencia corta para el dia a dia

```powershell
Set-Location C:\esp_projects\esp_claw_control\esp-claw\application\edge_agent
$env:IDF_EXTRA_ACTIONS_PATH='C:\esp_projects\esp_claw_control\esp-claw\application\edge_agent\managed_components\espressif__esp_board_manager'
idf.py bmgr -c ./boards -b esp32_S3_DevKitC_1
idf.py build
idf.py flash monitor
```

## 12. Archivos clave

- `README.md`
- `RELAY_OLED_TELEGRAM_SETUP.md`
- `CMakeLists.txt`
- `sdkconfig.defaults`
- `sdkconfig`
- `partitions_16MB.csv`
- `partitions_8MB.csv`
- `fatfs_image/scripts/user/relay_oled_shared.lua`
- `fatfs_image/scripts/user/relay_oled_command.lua`
- `fatfs_image/scripts/user/relay_oled_service.lua`
- `fatfs_image/router_rules/router_rules.json`
- `components/gen_bmgr_codes/board_manager.defaults`
- `boards/espressif/esp32_S3_DevKitC_1/`

## 13. Regla de trabajo para no romper el proyecto

Primero se cambia la fuente de configuracion.

- si es una definicion de placa, se edita `boards/...`
- si es una preferencia base del proyecto, se edita `sdkconfig.defaults`
- si es comportamiento de relays, se editan `fatfs_image/scripts/user/*` y `fatfs_image/router_rules/router_rules.json`

Despues se regenera con `bmgr` cuando corresponde.

Despues se limpia `build` si hubo cambios de placa, target o flash.

Al final se compila y se flashea.

Si se invierte ese orden, este repo puede quedar en estados mezclados.
