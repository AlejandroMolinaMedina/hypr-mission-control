#pragma once

// Gestos del touchpad.
//
// - El plugin registra su propio gesto vertical (gesture_fingers ↑ abre,
//   ↓ cierra si gesture_close) al cargarse y tras cada recarga de la config,
//   porque la recarga borra todos los gestos.
// - Mientras la vista está abierta (lateral_swipe), los gestos horizontales de
//   cambio de escritorio del usuario (action = "workspace", con los dedos,
//   modificadores y escala que tengan) se sustituyen por uno que mueve la
//   vista siguiendo al dedo. Al cerrarla se devuelven los originales tal cual.

// Registra el gesto vertical según la configuración actual
void registerGestures();

// Los quita (al descargar el plugin: no pueden quedar apuntando a su código)
void unregisterGestures();

// Sustituye los gestos horizontales de cambio de escritorio por el de la vista
void installLateralGestures();

// Devuelve los originales. Si alguno está en pleno uso no se toca (cambiarlo
// a mitad de gesto rompería su estado) y queda pendiente: main.cpp reintenta
// antes de cada fotograma. Con force se devuelven todos y se aborta el gesto
// en curso (al descargar el plugin)
void restoreLateralGestures(bool force = false);
bool lateralGesturesPending();

// Tras recargar la config, los gestos guardados ya no existen
void forgetLateralGestures();
