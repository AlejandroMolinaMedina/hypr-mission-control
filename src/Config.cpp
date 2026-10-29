#include "Config.hpp"
#include "globals.hpp"

#include <format>

#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/values/types/BoolValue.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>

template <typename T, typename D>
static bool add(const char* name, const char* description, D def) {
    const auto RESULT = Config::mgr()->registerPluginValue(PHANDLE, makeShared<T>(name, description, def));
    if (!RESULT)
        HyprlandAPI::addNotification(PHANDLE, std::format("[missioncontrol] no se pudo registrar {}: {}", name, RESULT.error()), CHyprColor{1.0, 0.2, 0.2, 1.0}, 8000);
    return RESULT.has_value();
}

bool registerConfig() {
    using namespace Config::Values;
    bool ok = true;

    // Gestos
    ok &= add<CIntValue>("plugin:missioncontrol:gesture_fingers", "dedos del gesto vertical que abre la vista (0 = sin gesto)", 3);
    ok &= add<CBoolValue>("plugin:missioncontrol:gesture_close", "registrar también el gesto hacia abajo que la cierra", true);
    ok &= add<CIntValue>("plugin:missioncontrol:gesture_distance", "recorrido del touchpad para abrirla del todo", 300);
    ok &= add<CIntValue>("plugin:missioncontrol:fling_speed", "velocidad (unidades/s) a partir de la cual un golpe decide solo", 600);
    ok &= add<CBoolValue>("plugin:missioncontrol:lateral_swipe", "con la vista abierta, el cambio de escritorio por gesto sigue al dedo dentro de ella", true);

    // Animaciones
    ok &= add<CIntValue>("plugin:missioncontrol:open_duration", "duración de la apertura (ms)", 280);
    ok &= add<CIntValue>("plugin:missioncontrol:close_duration", "duración del cierre (ms)", 220);
    ok &= add<CIntValue>("plugin:missioncontrol:slide_duration", "duración del deslizamiento entre escritorios (ms)", 220);

    // Cuadrícula
    ok &= add<CFloatValue>("plugin:missioncontrol:dim", "oscurecimiento del fondo (0-1)", 0.35F);
    ok &= add<CFloatValue>("plugin:missioncontrol:max_scale", "escala máxima de una ventana en la cuadrícula", 0.85F);
    ok &= add<CIntValue>("plugin:missioncontrol:gap", "separación entre ventanas", 36);
    ok &= add<CIntValue>("plugin:missioncontrol:margin_x", "margen a los lados", 70);
    ok &= add<CIntValue>("plugin:missioncontrol:margin_top", "margen superior si no hay tira de escritorios", 70);
    ok &= add<CIntValue>("plugin:missioncontrol:margin_bottom", "margen inferior", 50);
    ok &= add<CBoolValue>("plugin:missioncontrol:show_titles", "título bajo la ventana señalada", true);
    ok &= add<CIntValue>("plugin:missioncontrol:font_size", "tamaño de letra de títulos y nombres", 12);

    // Tira de escritorios
    ok &= add<CBoolValue>("plugin:missioncontrol:strip", "mostrar la tira de escritorios", true);
    ok &= add<CIntValue>("plugin:missioncontrol:strip_top", "distancia de la tira al borde superior", 42);
    ok &= add<CFloatValue>("plugin:missioncontrol:card_height", "alto de cada escritorio, en fracción del monitor", 0.12F);
    ok &= add<CIntValue>("plugin:missioncontrol:card_gap", "separación entre escritorios", 22);
    ok &= add<CStringValue>("plugin:missioncontrol:workspace_label", "nombre de cada escritorio; {} es su número", "Desktop {}");

    if (ok)
        g_pConfig = makeUnique<SConfig>();
    return ok;
}
