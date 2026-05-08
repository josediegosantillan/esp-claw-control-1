# Recuperacion Wi-Fi por Serial

Esta guia explica como recuperar o cambiar la configuracion Wi-Fi del equipo usando la consola serie del firmware.

## Cuando sirve

Usa este metodo si:

- el equipo no conecta al Wi-Fi
- se guardo un `SSID` o clave incorrectos
- no se abre el portal de provision porque ya habia credenciales guardadas

## Requisitos

- tener acceso fisico al equipo
- conectarlo por USB al PC
- abrir un monitor serie del ESP32-S3

Opciones comunes:

- `idf.py monitor`
- cualquier terminal serie conectado al puerto `COM` correcto

## Como saber que estas en la consola correcta

Cuando el firmware termina de arrancar, deberias ver algo como esto:

```text
app_claw_cli: Starting console REPL
app>
```

Si ves el prompt `app>`, ya podes escribir comandos.

## Ver estado actual del Wi-Fi

Escribi:

```text
wifi --status
```

Eso muestra si:

- hay credenciales guardadas
- esta conectado o no
- que modo Wi-Fi esta activo
- que SSID quedo guardado

## Cambiar SSID y clave Wi-Fi

Escribi:

```text
wifi --set --ssid MiRed --password MiClave --apply
```

Ejemplo:

```text
wifi --set --ssid MERCUSYS_6A69 --password 12345678 --apply
```

Ese comando:

- guarda las credenciales nuevas
- intenta aplicarlas en el momento

## Guardar sin aplicar en el momento

Si solo queres guardar y aplicar despues:

```text
wifi --set --ssid MiRed --password MiClave
```

Luego:

```text
wifi --apply
```

## Ver redes cercanas

Para escanear redes disponibles:

```text
wifi --scan
```

Sirve para confirmar el nombre exacto del `SSID`.

## Flujo recomendado de recuperacion

1. Abrir la consola serie.
2. Esperar el prompt `app>`.
3. Ejecutar `wifi --scan`.
4. Verificar el nombre exacto de la red.
5. Ejecutar `wifi --set --ssid TU_RED --password TU_CLAVE --apply`.
6. Ejecutar `wifi --status`.
7. Confirmar que el equipo obtuvo IP.

## Si la clave estaba mal y queres dejarlo sin Wi-Fi guardado

Si se implemento el borrado por boton, podes mantener `BOOT` 6 segundos para limpiar credenciales y reiniciar a modo portal.

Si preferis hacerlo por serie, hoy la opcion soportada en este proyecto es cargar credenciales nuevas con:

```text
wifi --set --ssid MiRed --password MiClave --apply
```

## Notas

- Si el `SSID` tiene espacios, escribilo completo despues de `--ssid`.
- Si la clave tiene caracteres especiales, conviene probar primero con cuidado desde consola serie.
- Si no aparece el prompt `app>`, entonces el monitor serie no esta conectado a la consola correcta o el firmware no termino de arrancar.
