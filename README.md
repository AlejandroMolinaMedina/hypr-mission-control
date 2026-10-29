# hypr-mission-control

Vista general al estilo **Mission Control de macOS** para [Hyprland](https://hypr.land), como plugin.

*A macOS-style Mission Control overview for Hyprland: live windows spread out in a grid, a strip of desktops on top, and trackpad gestures that follow your fingers.*

- **Ventanas en vivo**, dibujadas por la GPU dentro del compositor: nada de capturas. Un vídeo o una terminal siguen moviéndose dentro de su miniatura.
- **Sigue al dedo**: el gesto vertical (3 dedos por defecto) abre y cierra la vista de forma progresiva; al soltar decide según el recorrido o la velocidad.
- **Tira de escritorios** arriba, con cada escritorio en miniatura (en vivo) y un "+" para crear uno nuevo.
- **Cambio de escritorio dentro de la vista**: con ella abierta, tu gesto horizontal de cambio de escritorio mueve la vista siguiendo al dedo.
- **Sin parpadeos al elegir ventana**: cada fotograma parte de la posición real de las ventanas, así que el cierre acompaña a las animaciones de Hyprland.

Ratón: clic en una ventana para ir a ella, en un escritorio para cambiar a él, fuera para cerrar. `Esc` también cierra.

## Requisitos

- Hyprland **0.57** o posterior, con configuración en **Lua** (`hyprland.lua`).
- Para compilar el plugin en tu máquina: `gcc`, `make`, `pkgconf` y `jq`. Las cabeceras de Hyprland vienen con el paquete `hyprland`.

## Instalación

### Arch Linux (AUR)

```sh
yay -S hypr-mission-control-git
```

### A mano

```sh
git clone https://github.com/AlejandroMolinaMedina/hypr-mission-control
cd hypr-mission-control
make install PREFIX=~/.local     # o: sudo make install (en /usr/local)
```

`make install` copia el código fuente y el cargador `hypr-mission-control`; **no instala el plugin compilado**. Un plugin de Hyprland solo carga en la versión exacta para la que se compiló, así que el cargador lo compila en tu máquina para la versión que está corriendo, lo guarda en `~/.cache/hypr-mission-control/` y lo carga. Cuando actualizas Hyprland, la versión nueva corre a partir del siguiente inicio de sesión, y en ese momento el cargador vuelve a compilarlo (unos segundos).

## Configuración

En `~/.config/hypr/hyprland.lua`:

```lua
-- Cargarlo al iniciar sesión
hl.on("hyprland.start", function()
	hl.exec_cmd("hypr-mission-control")
end)

-- Opciones (todas opcionales). El "if" es necesario: al arrancar Hyprland el
-- plugin todavía no está cargado y sus opciones no existen; el cargador
-- recarga la config en cuanto lo carga, y entonces se aplican
if hl.plugin.missioncontrol then
	hl.config({
		plugin = {
			missioncontrol = {
				gesture_fingers = 3,
				workspace_label = "Escritorio {}",
			},
		},
	})
end
```

Con eso ya funciona: **3 dedos hacia arriba** abre la vista y **3 hacia abajo** la cierra.

### Opciones

| Opción | Por defecto | Qué hace |
|---|---|---|
| `gesture_fingers` | `3` | Dedos del gesto vertical que abre la vista. `0` = el plugin no registra gesto (ver [API Lua](#api-lua)). |
| `gesture_close` | `true` | Registrar también el gesto hacia abajo que la cierra. |
| `gesture_distance` | `300` | Recorrido del touchpad para abrirla del todo (y para pasar un escritorio entero al deslizar de lado). |
| `fling_speed` | `600` | Velocidad (unidades/s) a partir de la cual un golpe rápido decide solo. |
| `lateral_swipe` | `true` | Con la vista abierta, tus gestos de cambio de escritorio (`action = "workspace"`) la mueven siguiendo al dedo. |
| `open_duration` | `280` | Duración de la apertura, en ms. |
| `close_duration` | `220` | Duración del cierre, en ms. |
| `slide_duration` | `220` | Duración del deslizamiento entre escritorios, en ms. |
| `dim` | `0.35` | Oscurecimiento del fondo (0–1). |
| `max_scale` | `0.85` | Escala máxima de una ventana en la cuadrícula. |
| `gap` | `36` | Separación entre ventanas. |
| `margin_x` | `70` | Margen a los lados. |
| `margin_top` | `70` | Margen superior cuando no hay tira de escritorios. |
| `margin_bottom` | `50` | Margen inferior. |
| `show_titles` | `true` | Título bajo la ventana señalada. |
| `font_size` | `12` | Tamaño de letra de títulos y nombres. |
| `strip` | `true` | Mostrar la tira de escritorios. |
| `strip_top` | `42` | Distancia de la tira al borde superior (deja ver tu barra). |
| `card_height` | `0.12` | Alto de cada escritorio de la tira, en fracción del monitor. |
| `card_gap` | `22` | Separación entre escritorios. |
| `workspace_label` | `"Desktop {}"` | Nombre de cada escritorio; `{}` es su número. |

Las medidas van en píxeles lógicos.

### Gestos

El plugin registra su gesto vertical al cargarse y después de cada recarga de la config. Si ya tienes en tu config un gesto con los mismos dedos y dirección, **el tuyo tiene prioridad** y el del plugin queda tapado (Hyprland lo avisa en su log).

Con la vista abierta, el plugin sustituye temporalmente tus gestos horizontales de cambio de escritorio (los que usan `action = "workspace"`, con sus dedos, modificadores y escala) por uno que mueve la vista. Al cerrarla te los devuelve tal cual. Se desactiva con `lateral_swipe = false`.

### API Lua

Para atajos o gestos propios, desde `hl.plugin.missioncontrol`:

| Función | Qué hace |
|---|---|
| `toggle()` / `open()` / `close()` | Abrir y cerrar con animación. |
| `is_open()` | Si está abierta o abriéndose. |
| `gesture_begin("open" \| "close")` | Empieza a seguir al dedo. Devuelve `false` si no procede (p. ej. `"close"` con la vista cerrada). |
| `gesture_update(delta, time_ms)` | Desplazamiento de cada evento, en positivo hacia "abierta". |
| `gesture_end(cancelled)` | Al soltar: termina de abrirse o se cierra. |

Atajo de teclado:

```lua
hl.bind("SUPER + TAB", function() hl.plugin.missioncontrol.toggle() end)
```

Gesto propio: por ejemplo, que 3 dedos hacia abajo cierre la vista si está abierta y, si no, haga otra cosa. Pon `gesture_close = false` y:

```lua
local cerrando = false
local function subida(e) return e.delta and -e.delta.y or 0 end
hl.gesture({
	fingers = 3,
	direction = "down",
	action = {
		start = function(e)
			local mc = hl.plugin.missioncontrol
			cerrando = mc ~= nil and mc.gesture_begin("close")
			if cerrando then mc.gesture_update(subida(e), e.time_ms) end
		end,
		update = function(e)
			if cerrando then hl.plugin.missioncontrol.gesture_update(subida(e), e.time_ms) end
		end,
		finish = function(e)
			if cerrando then
				hl.plugin.missioncontrol.gesture_end(e.cancelled == true)
			elseif not e.cancelled then
				hl.dispatch(hl.dsp.workspace.toggle_special("minimized"))
			end
			cerrando = false
		end,
	},
})
```

## Uso del cargador

```sh
hypr-mission-control            # compilar si hace falta, cargar y recargar la config
hypr-mission-control build      # solo compilar
hypr-mission-control unload     # descargar
```

Si algo falla, avisa con una notificación de Hyprland. El log de la última compilación está en `~/.cache/hypr-mission-control/build.log`.

## Limitaciones

- La vista se abre en el monitor enfocado y muestra los escritorios numerados de ese monitor.
- Las ventanas de escritorios especiales (scratchpads) no aparecen.
- Es un plugin: corre dentro de Hyprland y usa su API interna. Una versión nueva de Hyprland puede romper la compilación hasta que se actualice el plugin; en ese caso el cargador avisa y Hyprland sigue funcionando sin él.

## Licencia

BSD-3-Clause, como Hyprland. Ver [LICENSE](LICENSE).
