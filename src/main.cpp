// hypr-mission-control: vista general al estilo Mission Control de macOS
// para Hyprland.
//
// El plugin registra su propio gesto (ver Gestures.hpp y Config.hpp). Además,
// desde la configuración Lua (hl.plugin.missioncontrol.*):
//   toggle() / open() / close()        abrir y cerrar con animación
//   is_open()                          si está abierta (o abriéndose)
//   gesture_begin("open" | "close")    para gestos propios en Lua: empieza a
//                                      seguir al dedo; false si no procede
//   gesture_update(delta, time_ms)     desplazamiento del evento (positivo = abrir)
//   gesture_end(cancelled)             suelta: termina de abrirse o se cierra

#include <lua.hpp>

// Las cabeceras estándar, antes del truco de abajo (lo rompe dentro de ellas)
#include <any>
#include <sstream>
#include <string>

#define private   public
#define protected public
#include <hyprland/src/render/Renderer.hpp>
#undef private
#undef protected
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/output/Monitor.hpp>

#include "Config.hpp"
#include "Gestures.hpp"
#include "globals.hpp"
#include "MissionControl.hpp"

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

// ── Hook: el escritorio del monitor con la vista abierta lo dibuja el plugin ─

static CFunctionHook* g_pRenderWorkspaceHook = nullptr;
using FRenderWorkspace = void (*)(void*, Render::CRenderContext&, PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&, const CBox&);

static void hkRenderWorkspace(void* self, Render::CRenderContext& ctx, PHLMONITOR monitor, PHLWORKSPACE workspace, const Time::steady_tp& now, const CBox& geometry) {
    // Las instantáneas de una ventana (animaciones de Hyprland) no la incluyen;
    // una captura de pantalla sí, como en el Mac
    if (g_pMissionControl && g_pMissionControl->owns(monitor) && !ctx.m_renderingSnapshot)
        g_pMissionControl->render(ctx, now);
    else
        ((FRenderWorkspace)g_pRenderWorkspaceHook->m_original)(self, ctx, monitor, workspace, now, geometry);
}

// ── Acciones ───────────────────────────────────────────────────────────────

static bool isOpen() {
    return g_pMissionControl && !g_pMissionControl->isClosing();
}

static void openOverview() {
    if (g_pMissionControl && !g_pMissionControl->isClosing())
        return;
    if (!g_pMissionControl) {
        const auto MON = Desktop::focusState()->monitor();
        if (!MON)
            return;
        g_pMissionControl = makeUnique<CMissionControl>(MON);
    }
    g_pMissionControl->open();
}

static void closeOverview() {
    if (isOpen())
        g_pMissionControl->close();
}

static SDispatchResult dispatch(std::string arg) {
    if (arg == "open")
        openOverview();
    else if (arg == "close")
        closeOverview();
    else if (isOpen())
        closeOverview();
    else
        openOverview();
    return {};
}

// ── Lua ────────────────────────────────────────────────────────────────────

static int luaToggle(lua_State*) {
    dispatch("toggle");
    return 0;
}

static int luaOpen(lua_State*) {
    openOverview();
    return 0;
}

static int luaClose(lua_State*) {
    closeOverview();
    return 0;
}

static int luaIsOpen(lua_State* L) {
    lua_pushboolean(L, isOpen());
    return 1;
}

static int luaGestureBegin(lua_State* L) {
    const std::string MODE = luaL_checkstring(L, 1);
    bool              started = false;

    if (MODE == "open") {
        // También vale para retomar una vista que se estaba cerrando
        if (!g_pMissionControl) {
            if (const auto MON = Desktop::focusState()->monitor())
                g_pMissionControl = makeUnique<CMissionControl>(MON);
        }
        if (g_pMissionControl) {
            g_pMissionControl->gestureBegin();
            started = true;
        }
    } else if (MODE == "close" && isOpen()) {
        g_pMissionControl->gestureBegin();
        started = true;
    } else if (MODE != "close")
        return luaL_error(L, "gesture_begin: se esperaba \"open\" o \"close\"");

    lua_pushboolean(L, started);
    return 1;
}

static int luaGestureUpdate(lua_State* L) {
    const double   DELTA = luaL_checknumber(L, 1);
    const uint32_t MS    = sc<uint32_t>(luaL_optinteger(L, 2, 0));
    if (g_pMissionControl)
        g_pMissionControl->gestureUpdate(DELTA, MS);
    return 0;
}

static int luaGestureEnd(lua_State* L) {
    const bool CANCELLED = lua_toboolean(L, 1);
    if (g_pMissionControl)
        g_pMissionControl->gestureEnd(CANCELLED);
    return 0;
}

// ── Carga y descarga ───────────────────────────────────────────────────────

static void fail(const std::string& reason) {
    HyprlandAPI::addNotification(PHANDLE, "[missioncontrol] " + reason, CHyprColor{1.0, 0.2, 0.2, 1.0}, 8000);
    throw std::runtime_error("[missioncontrol] " + reason);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    // Compilado contra otras cabeceras = estructuras distintas = cuelgue
    if (std::string{__hyprland_api_get_hash()} != std::string{__hyprland_api_get_client_hash()})
        fail("compilado para otra versión de Hyprland: hay que recompilarlo");

    // renderWorkspace tiene dos sobrecargas: interesa la que recibe el monitor
    for (const auto& fn : HyprlandAPI::findFunctionsByName(PHANDLE, "renderWorkspace")) {
        if (fn.demangled.contains("IHyprRenderer::renderWorkspace") && fn.demangled.contains("CMonitor")) {
            g_pRenderWorkspaceHook = HyprlandAPI::createFunctionHook(PHANDLE, fn.address, (void*)hkRenderWorkspace);
            break;
        }
    }
    if (!g_pRenderWorkspaceHook || !g_pRenderWorkspaceHook->hook())
        fail("no se pudo enganchar renderWorkspace");

    if (!registerConfig())
        fail("no se pudieron registrar las opciones");
    registerGestures();

    static auto PRERENDER = Event::bus()->m_events.render.pre.listen([](PHLMONITOR monitor) {
        if (g_pMissionControl)
            g_pMissionControl->onPreRender(monitor);
        else if (lateralGesturesPending())
            restoreLateralGestures(); // los que estaban en uso al cerrar
    });

    // Recargar la config borra todos los gestos: se vuelven a registrar, y si
    // la vista está abierta, a sustituir los horizontales
    static auto RELOADED = Event::bus()->m_events.config.reloaded.listen([] {
        forgetLateralGestures();
        registerGestures();
        if (g_pMissionControl && !g_pMissionControl->isClosing())
            installLateralGestures();
    });

    HyprlandAPI::addDispatcherV2(PHANDLE, "missioncontrol", ::dispatch);
    HyprlandAPI::addLuaFunction(PHANDLE, "missioncontrol", "toggle", ::luaToggle);
    HyprlandAPI::addLuaFunction(PHANDLE, "missioncontrol", "open", ::luaOpen);
    HyprlandAPI::addLuaFunction(PHANDLE, "missioncontrol", "close", ::luaClose);
    HyprlandAPI::addLuaFunction(PHANDLE, "missioncontrol", "is_open", ::luaIsOpen);
    HyprlandAPI::addLuaFunction(PHANDLE, "missioncontrol", "gesture_begin", ::luaGestureBegin);
    HyprlandAPI::addLuaFunction(PHANDLE, "missioncontrol", "gesture_update", ::luaGestureUpdate);
    HyprlandAPI::addLuaFunction(PHANDLE, "missioncontrol", "gesture_end", ::luaGestureEnd);

    return {"missioncontrol", "Vista general al estilo Mission Control de macOS", "Al3xmm14", "1.0.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pMissionControl.reset();
    // Ningún gesto puede quedar apuntando a código del plugin
    restoreLateralGestures(true);
    unregisterGestures();
    g_pConfig.reset();
}
