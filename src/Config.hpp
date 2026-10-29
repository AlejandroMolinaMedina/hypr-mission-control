#pragma once

// Opciones del plugin: hl.config({ plugin = { missioncontrol = { ... } } }).
// La lista completa, con sus valores por defecto, está en el README.

#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/helpers/memory/Memory.hpp>

#include <string>

struct SConfig {
    // Gestos
    CConfigValue<Config::INTEGER> gestureFingers{"plugin:missioncontrol:gesture_fingers"};
    CConfigValue<Config::BOOL>    gestureClose{"plugin:missioncontrol:gesture_close"};
    CConfigValue<Config::INTEGER> gestureDistance{"plugin:missioncontrol:gesture_distance"};
    CConfigValue<Config::INTEGER> flingSpeed{"plugin:missioncontrol:fling_speed"};
    CConfigValue<Config::BOOL>    lateralSwipe{"plugin:missioncontrol:lateral_swipe"};

    // Animaciones (ms)
    CConfigValue<Config::INTEGER> openDuration{"plugin:missioncontrol:open_duration"};
    CConfigValue<Config::INTEGER> closeDuration{"plugin:missioncontrol:close_duration"};
    CConfigValue<Config::INTEGER> slideDuration{"plugin:missioncontrol:slide_duration"};

    // Cuadrícula de ventanas
    CConfigValue<Config::FLOAT>   dim{"plugin:missioncontrol:dim"};
    CConfigValue<Config::FLOAT>   maxScale{"plugin:missioncontrol:max_scale"};
    CConfigValue<Config::INTEGER> gap{"plugin:missioncontrol:gap"};
    CConfigValue<Config::INTEGER> marginX{"plugin:missioncontrol:margin_x"};
    CConfigValue<Config::INTEGER> marginTop{"plugin:missioncontrol:margin_top"};
    CConfigValue<Config::INTEGER> marginBottom{"plugin:missioncontrol:margin_bottom"};
    CConfigValue<Config::BOOL>    showTitles{"plugin:missioncontrol:show_titles"};
    CConfigValue<Config::INTEGER> fontSize{"plugin:missioncontrol:font_size"};

    // Tira de escritorios
    CConfigValue<Config::BOOL>    strip{"plugin:missioncontrol:strip"};
    CConfigValue<Config::INTEGER> stripTop{"plugin:missioncontrol:strip_top"};
    CConfigValue<Config::FLOAT>   cardHeight{"plugin:missioncontrol:card_height"};
    CConfigValue<Config::INTEGER> cardGap{"plugin:missioncontrol:card_gap"};
    CConfigValue<Config::STRING>  workspaceLabel{"plugin:missioncontrol:workspace_label"};
};

// Registra las opciones en Hyprland (al cargar el plugin) y crea g_pConfig
bool registerConfig();

inline UP<SConfig> g_pConfig;
