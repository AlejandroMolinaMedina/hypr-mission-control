#include "Gestures.hpp"
#include "Config.hpp"
#include "MissionControl.hpp"

// Las cabeceras estándar, antes del truco de abajo (lo rompe dentro de ellas)
#include <any>
#include <sstream>
#include <string>
#include <vector>

#define private   public
#define protected public
#include <hyprland/src/managers/input/trackpad/TrackpadGestures.hpp>
#undef private
#undef protected
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/WorkspaceSwipeGesture.hpp>

// ── Gesto vertical: abrir / cerrar ─────────────────────────────────────────

class CMissionGesture : public ITrackpadGesture {
  public:
    explicit CMissionGesture(bool opens) : m_opens(opens) {
        ;
    }

    void begin(const STrackpadGestureBegin& e) override {
        ITrackpadGesture::begin(e);
        m_active = false;

        if (m_opens) {
            // También retoma una vista que se estaba cerrando
            if (!g_pMissionControl) {
                const auto MON = Desktop::focusState()->monitor();
                if (!MON)
                    return;
                g_pMissionControl = makeUnique<CMissionControl>(MON);
            }
        } else if (!g_pMissionControl || g_pMissionControl->isClosing())
            return; // hacia abajo solo cierra una vista abierta

        g_pMissionControl->gestureBegin();
        m_active = true;
        update({.swipe = e.swipe, .pinch = e.pinch, .direction = e.direction, .scale = e.scale});
    }

    void update(const STrackpadGestureUpdate& e) override {
        // Siempre en positivo hacia arriba (hacia abierta), sea cual sea la dirección registrada
        if (m_active && g_pMissionControl && e.swipe)
            g_pMissionControl->gestureUpdate(-e.swipe->delta.y * m_scale, e.swipe->timeMs);
    }

    void end(const STrackpadGestureEnd& e) override {
        if (m_active && g_pMissionControl)
            g_pMissionControl->gestureEnd(e.swipe && e.swipe->cancelled);
        m_active = false;
    }

  private:
    bool m_opens  = true;
    bool m_active = false;
};

void registerGestures() {
    const auto FINGERS = *g_pConfig->gestureFingers;
    if (FINGERS <= 0)
        return;

    // Si el usuario ya tiene un gesto con esos dedos y dirección, el suyo va
    // primero y este queda tapado (Hyprland avisa en el log)
    (void)g_pTrackpadGestures->addGesture(makeUnique<CMissionGesture>(true), FINGERS, TRACKPAD_GESTURE_DIR_UP, Input::HL_MODIFIER_NONE, 1.F, false);
    if (*g_pConfig->gestureClose)
        (void)g_pTrackpadGestures->addGesture(makeUnique<CMissionGesture>(false), FINGERS, TRACKPAD_GESTURE_DIR_DOWN, Input::HL_MODIFIER_NONE, 1.F, false);
}

void unregisterGestures() {
    // Por tipo, no por dedos y dirección: el usuario puede tener uno igual
    const auto ACTIVE = g_pTrackpadGestures->m_activeGesture;
    if (ACTIVE && dynamic_cast<CMissionGesture*>(ACTIVE->gesture.get())) {
        g_pTrackpadGestures->m_activeGesture.reset();
        g_pTrackpadGestures->m_gestureFindFailed = true;
    }
    std::erase_if(g_pTrackpadGestures->m_gestures, [](const auto& data) { return dynamic_cast<CMissionGesture*>(data->gesture.get()); });
}

// ── Gesto horizontal con la vista abierta ──────────────────────────────────

class CLateralGesture : public ITrackpadGesture {
  public:
    void begin(const STrackpadGestureBegin& e) override {
        ITrackpadGesture::begin(e);
        if (g_pMissionControl)
            g_pMissionControl->lateralBegin();
    }
    void update(const STrackpadGestureUpdate& e) override {
        if (g_pMissionControl && e.swipe)
            g_pMissionControl->lateralUpdate(e.swipe->delta.x * m_scale, e.swipe->timeMs);
    }
    void end(const STrackpadGestureEnd& e) override {
        if (g_pMissionControl)
            g_pMissionControl->lateralEnd(e.swipe && e.swipe->cancelled);
    }
};

struct SSwapped {
    WP<CTrackpadGestures::SGestureData> data;
    UP<ITrackpadGesture>                original;
};
static std::vector<SSwapped> g_swapped;

void installLateralGestures() {
    if (!*g_pConfig->lateralSwipe || !g_swapped.empty())
        return;

    for (const auto& data : g_pTrackpadGestures->m_gestures) {
        if (!dynamic_cast<CWorkspaceSwipeGesture*>(data->gesture.get()))
            continue;
        if (data == g_pTrackpadGestures->m_activeGesture)
            continue; // en uso: se queda el nativo
        g_swapped.push_back({.data = data, .original = std::move(data->gesture)});
        data->gesture = makeUnique<CLateralGesture>();
    }
}

void restoreLateralGestures(bool force) {
    const auto ACTIVE = g_pTrackpadGestures->m_activeGesture;

    std::erase_if(g_swapped, [&](SSwapped& s) {
        const auto DATA = s.data.lock();
        if (!DATA)
            return true;
        if (DATA == ACTIVE && !force)
            return false;
        DATA->gesture = std::move(s.original);
        return true;
    });

    if (force && ACTIVE && !dynamic_cast<CWorkspaceSwipeGesture*>(ACTIVE->gesture.get())) {
        // El gesto en curso ya no es el que empezó: se da por terminado
        g_pTrackpadGestures->m_activeGesture.reset();
        g_pTrackpadGestures->m_gestureFindFailed = true;
    }

}

bool lateralGesturesPending() {
    return !g_swapped.empty();
}

void forgetLateralGestures() {
    g_swapped.clear();
}
