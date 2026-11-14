# Prueba automática en un VPS

`hypr-mc-vps-check` comprueba, desde cualquier servidor con Docker, si el plugin sigue compilando con cada versión nueva de Hyprland que sale en Arch, y avisa a un webhook.

1. Consulta en archlinux.org la versión de Hyprland de los repos. Si ya la probó, termina.
2. Si es nueva, en un contenedor `archlinux` desechable instala Hyprland (con el sistema al día, como un Arch actualizado), clona el plugin, lo compila y comprueba que todas las funciones que pide existen en ese Hyprland.
3. Manda un `POST` con JSON al webhook:

```json
{
  "estado": 0,
  "mensaje": "El plugin compila y enlaza con Hyprland 0.57.2-1",
  "hyprland": "0.57.2-1",
  "hyprland_commit": "11cbc6c5…",
  "plugin_commit": "0457b9f",
  "fecha": "2026-11-13T20:00:00Z",
  "log": "…últimas 60 líneas de la prueba…"
}
```

`estado` es `0` si el plugin compila y enlaza y `1` si no. Si la prueba no se pudo hacer (sin red, Docker caído, error de pacman…), no manda nada, sale con `3` y lo reintenta en la siguiente ejecución.

## Requisitos

Docker (o Podman), `curl`, `flock` (util-linux) y bash. No hace falta Arch en el servidor.

## Instalación

```sh
sudo mkdir -p /opt/hypr-mc
sudo cp hypr-mc-vps-check hypr-mc-vps.conf.example /opt/hypr-mc/
sudo cp /opt/hypr-mc/hypr-mc-vps.conf.example /opt/hypr-mc/hypr-mc-vps.conf
sudo nano /opt/hypr-mc/hypr-mc-vps.conf     # pon tu WEBHOOK_URL
```

El usuario que lo ejecute tiene que poder usar Docker (estar en el grupo `docker`, o ser root).

Primera prueba a mano (`--force` prueba aunque ya se haya probado esa versión):

```sh
/opt/hypr-mc/hypr-mc-vps-check --force
```

## Cron

`crontab -e`, cada 6 horas:

```
0 */6 * * * /opt/hypr-mc/hypr-mc-vps-check >> $HOME/hypr-mc-vps.log 2>&1
```

Una ejecución sin versión nueva solo hace una consulta a archlinux.org. Si cron la lanza mientras otra prueba sigue en marcha, la nueva termina sin hacer nada.

En `~/.local/state/hypr-mc-vps/` quedan la última versión probada, el último resultado (`ultimo-resultado.json`) y el log completo de la última prueba (`ultimo.log`).
