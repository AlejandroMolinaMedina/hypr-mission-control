#include "MissionControl.hpp"
#include "Config.hpp"
#include "Gestures.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <linux/input-event-codes.h>

// Las cabeceras estándar, antes del truco de abajo (lo rompe dentro de ellas)
#include <any>
#include <format>
#include <sstream>
#include <string>

#define private   public
#define protected public
#include <hyprland/src/render/Renderer.hpp>
#undef private
#undef protected
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/view/window/Window.hpp>
#include <hyprland/src/desktop/view/window/WindowPresentation.hpp>
#include <hyprland/src/desktop/view/window/WindowMetadata.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/RendererHintsPassElement.hpp>
#include <hyprland/src/render/pass/TexPassElement.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/render/scene/SceneSelection.hpp>
#include <hyprland/src/workspace/HLWorkspace.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/config/shared/actions/ConfigActions.hpp>
#include <hyprland/protocols/wlr-layer-shell-unstable-v1.hpp>

// ── Medidas internas (las ajustables están en Config.hpp) ──────────────────

constexpr double  LABEL_GAP  = 6;  // entre una tarjeta y su nombre
constexpr double  GRID_GAP   = 44; // entre la tira (con nombres) y la cuadrícula
constexpr double  HOVER_RING = 4;  // contorno de la ventana bajo el ratón
constexpr int64_t NEW_CARD   = -2; // el "+"

static double ms(int64_t v) {
    return std::max<int64_t>(v, 1) / 1000.0;
}

using Desktop::View::IGeometric;

static double easeOutCubic(double t) {
    return 1.0 - std::pow(1.0 - t, 3.0);
}

static CBox lerpBox(const CBox& a, const CBox& b, double t) {
    return CBox{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.w + (b.w - a.w) * t, a.h + (b.h - a.h) * t};
}

// ── Ciclo de vida ──────────────────────────────────────────────────────────

CMissionControl::CMissionControl(PHLMONITOR monitor) : m_monitor(monitor) {
    layout();
    m_mouse = g_pInputManager->getMouseCoordsInternal() - monitor->m_position;

    // Mientras está abierta, el ratón y Esc son suyos: ni las ventanas
    // reciben los clics ni follow_mouse cambia el foco al moverlo
    m_mouseMove = Event::bus()->m_events.input.mouse.move.listen([this](Vector2D, Event::SCallbackInfo& info) {
        if (m_closing || m_finished)
            return;
        info.cancelled = true;
        if (const auto MON = m_monitor.lock()) {
            m_mouse = g_pInputManager->getMouseCoordsInternal() - MON->m_position;
            updateHover();
        }
    });

    m_mouseButton = Event::bus()->m_events.input.mouse.button.listen([this](IPointer::SButtonEvent e, Event::SCallbackInfo& info) {
        if (m_closing || m_finished)
            return;
        info.cancelled = true;
        if (e.state != WL_POINTER_BUTTON_STATE_PRESSED || e.button != BTN_LEFT || m_gesture.active)
            return;
        updateHover();
        if (m_hoveredCard != -1)
            goToWorkspace(m_hoveredCard);
        else
            close(m_hovered.lock()); // fuera de las ventanas: cierra sin cambiar el foco
    });

    m_keyboardKey = Event::bus()->m_events.input.keyboard.key.listen([this](IKeyboard::SKeyEvent e, Event::SCallbackInfo& info) {
        if (m_closing || m_finished || e.keycode != KEY_ESC)
            return;
        info.cancelled = true;
        if (e.state == WL_KEYBOARD_KEY_STATE_PRESSED)
            close();
    });

    // Fuera del bucle de gestos de Hyprland: la vista suele abrirse desde
    // dentro de uno (3 dedos ↑) y ahí no se debe tocar su lista
    g_pEventLoopManager->doLater([self = this] {
        if (g_pMissionControl.get() == self && !self->m_finished)
            installLateralGestures();
    });

    damage();
}

CMissionControl::~CMissionControl() {
    // Se destruye desde doLater o al descargar el plugin: fuera de cualquier gesto
    restoreLateralGestures();
    damage();
}

bool CMissionControl::owns(const PHLMONITOR& monitor) const {
    return !m_finished && m_monitor.lock() == monitor;
}

bool CMissionControl::isClosing() const {
    return m_closing || m_finished;
}

void CMissionControl::open() {
    if (m_finished)
        return;
    m_closing = false;
    animateTo(1.0);
}

void CMissionControl::close(PHLWINDOW focus) {
    if (m_finished)
        return;
    m_closing        = true;
    m_gesture.active = false;
    if (focus) {
        // Se enfoca ya: Hyprland empieza a subirla (o a maximizarla) y, como
        // cada fotograma parte de su posición real, la miniatura la acompaña
        m_hovered = focus;
        Desktop::focusState()->fullWindowFocus(focus, Desktop::FOCUS_REASON_CLICK);
    }
    animateTo(0.0);
}

void CMissionControl::finish() {
    if (m_finished)
        return;
    m_finished = true;
    damage();
    // No se destruye a sí misma en mitad del render: más tarde, y solo si
    // no la han sustituido ya por otra
    g_pEventLoopManager->doLater([self = this] {
        if (g_pMissionControl.get() == self)
            g_pMissionControl.reset();
    });
}

void CMissionControl::damage() {
    if (const auto MON = m_monitor.lock())
        g_pHyprRenderer->damageMonitor(MON);
}

// ── Animación y gesto ──────────────────────────────────────────────────────

void CMissionControl::animateTo(double target) {
    // Lo que queda por recorrer marca la duración: soltar casi al final no
    // debe tardar lo mismo que una animación completa
    const double FULL = ms(target >= 1.0 ? *g_pConfig->openDuration : *g_pConfig->closeDuration);
    m_anim            = {
                   .active   = true,
                   .from     = m_progress,
                   .to       = target,
                   .duration = std::max(0.08, FULL * std::abs(target - m_progress)),
                   .start    = Time::steadyNow(),
    };
    damage();
}

void CMissionControl::gestureBegin() {
    if (m_finished)
        return;
    m_anim.active = false;
    m_closing     = false;
    m_gesture     = {.active = true, .base = m_progress, .total = 0.0, .samples = {}};
    damage();
}

void CMissionControl::gestureUpdate(double delta, uint32_t timeMs) {
    if (!m_gesture.active)
        return;
    m_gesture.total += delta;
    m_progress = std::clamp(m_gesture.base + m_gesture.total / std::max<int64_t>(*g_pConfig->gestureDistance, 1), 0.0, 1.0);

    std::erase_if(m_gesture.samples, [timeMs](const auto& s) { return timeMs - s.first > 100; });
    m_gesture.samples.emplace_back(timeMs, m_gesture.total);
    damage();
}

void CMissionControl::gestureEnd(bool cancelled) {
    if (!m_gesture.active)
        return;
    m_gesture.active = false;

    double speed = 0.0;
    if (m_gesture.samples.size() >= 2) {
        const auto& [t0, r0] = m_gesture.samples.front();
        const auto& [t1, r1] = m_gesture.samples.back();
        if (t1 > t0)
            speed = (r1 - r0) / ((t1 - t0) / 1000.0);
    }

    bool keepOpen;
    if (cancelled)
        keepOpen = m_gesture.base >= 1.0;
    else if (std::abs(speed) > *g_pConfig->flingSpeed) // un golpe rápido decide solo
        keepOpen = speed > 0;
    else // si no, cuánto se ha recorrido
        keepOpen = m_progress > (m_gesture.base >= 1.0 ? 0.75 : 0.25);

    if (keepOpen)
        open();
    else
        close(m_gesture.base >= 1.0 ? m_hovered.lock() : nullptr);
}

void CMissionControl::onPreRender(const PHLMONITOR& monitor) {
    if (m_finished || m_monitor.lock() != monitor)
        return;

    if (m_anim.active) {
        const double ELAPSED = std::chrono::duration<double>(Time::steadyNow() - m_anim.start).count();
        const double K       = std::clamp(ELAPSED / m_anim.duration, 0.0, 1.0);
        m_progress           = m_anim.from + (m_anim.to - m_anim.from) * easeOutCubic(K);
        if (K >= 1.0) {
            m_anim.active = false;
            if (m_anim.to <= 0.0) {
                finish();
                return;
            }
        }
    }

    if (m_lateral.animating) {
        const double ELAPSED = std::chrono::duration<double>(Time::steadyNow() - m_lateral.start).count();
        const double K       = std::clamp(ELAPSED / m_lateral.duration, 0.0, 1.0);
        m_lateral.value      = m_lateral.from + (m_lateral.to - m_lateral.from) * easeOutCubic(K);
        if (K >= 1.0) {
            m_lateral.animating = false;
            if (m_lateral.to != 0.0)
                lateralArrive(m_lateral.to > 0 ? 1 : -1);
        }
    }

    // Ventanas que se abren o se cierran mientras tanto. Al cerrar no: la
    // enfocada cambia de tamaño y la cuadrícula se movería bajo los pies
    if (!m_closing)
        layout();

    // Contenido en vivo: se redibuja en cada fotograma mientras esté abierta.
    // El daño se pide al terminar este fotograma: pedido ahora, lo consume el
    // propio fotograma en curso y no se programa el siguiente
    if (!m_redrawPending) {
        m_redrawPending = true;
        g_pEventLoopManager->doLater([self = this] {
            if (g_pMissionControl.get() != self)
                return;
            self->m_redrawPending = false;
            self->damage();
        });
    }
}

// ── Escritorios ────────────────────────────────────────────────────────────

// workspace_label con "{}" sustituido por el número
static std::string workspaceLabel(int64_t id) {
    std::string label = *g_pConfig->workspaceLabel;
    if (const auto POS = label.find("{}"); POS != std::string::npos)
        label.replace(POS, 2, std::to_string(id));
    return label;
}

// Primer número de escritorio libre por encima de "after"
static int64_t firstFreeWorkspaceAfter(int64_t after) {
    std::set<int64_t> used;
    for (const auto& ws : State::Workspace::state()->workspacesCopy()) {
        if (const auto ID = ws->numberedID())
            used.insert(*ID);
    }
    int64_t id = after + 1;
    while (used.contains(id))
        ++id;
    return id;
}

void CMissionControl::layoutStrip() {
    const auto MON = m_monitor.lock();
    if (!MON)
        return;

    m_cards.clear();
    m_newButton = {};
    if (!*g_pConfig->strip) {
        m_stripBottom = *g_pConfig->marginTop;
        return;
    }

    std::vector<std::pair<int64_t, PHLWORKSPACE>> workspaces;
    for (const auto& ws : State::Workspace::state()->workspacesCopy()) {
        const auto ID = ws->numberedID();
        if (ID && ws->m_monitor.lock() == MON)
            workspaces.emplace_back(*ID, ws);
    }
    std::ranges::sort(workspaces, {}, &std::pair<int64_t, PHLWORKSPACE>::first);

    const double TOP   = *g_pConfig->stripTop;
    const double GAPC  = *g_pConfig->cardGap;
    const double H     = std::round(MON->m_size.y * std::clamp<double>(*g_pConfig->cardHeight, 0.03, 0.4));
    const double W     = std::round(H * MON->m_size.x / MON->m_size.y);
    const double PLUS  = std::round(H * 0.5);
    const double TOTAL = workspaces.size() * (W + GAPC) + PLUS;
    double       x     = std::round((MON->m_size.x - TOTAL) / 2);

    for (const auto& [id, ws] : workspaces) {
        m_cards.push_back({.workspace = ws, .id = id, .box = {x, TOP, W, H}});
        x += W + GAPC;
    }
    m_newButton   = {x, TOP + (H - PLUS) / 2, PLUS, PLUS};
    m_stripBottom = TOP + H + LABEL_GAP + *g_pConfig->fontSize * 1.6 + GRID_GAP;
}

int64_t CMissionControl::cardAt(const Vector2D& pos) const {
    for (const auto& c : m_cards) {
        if (c.box.containsPoint(pos))
            return c.id;
    }
    return m_newButton.w > 0 && m_newButton.containsPoint(pos) ? NEW_CARD : -1;
}

void CMissionControl::goToWorkspace(int64_t id) {
    const auto MON = m_monitor.lock();
    if (!MON || m_finished)
        return;
    if (id == NEW_CARD) {
        const auto CURRENT = MON->m_activeWorkspace ? MON->m_activeWorkspace->numberedID() : std::nullopt;
        id                 = firstFreeWorkspaceAfter(CURRENT ? *CURRENT : 0);
    }
    // Como en el Mac: se va a ese escritorio y la vista se cierra. Las ventanas
    // vuelven a su sitio en el nuevo escritorio
    (void)Config::Actions::changeWorkspace(std::to_string(id));
    layout();
    close();
}

SP<Render::ITexture> CMissionControl::text(const std::string& str, int pt, bool bold, int maxWidth) {
    const auto KEY = std::format("{}|{}|{}|{}", str, pt, bold, maxWidth);
    if (const auto IT = m_texts.find(KEY); IT != m_texts.end())
        return IT->second;
    const auto MON   = m_monitor.lock();
    const auto SCALE = MON ? MON->m_scale : 1.F;
    auto       tex   = g_pHyprRenderer->renderText(str, CHyprColor(1, 1, 1, 1), std::round(pt * SCALE), false, "", std::round(maxWidth * SCALE), bold ? 700 : 400);
    m_texts[KEY]     = tex;
    return tex;
}

// ── Deslizamiento lateral ──────────────────────────────────────────────────

void CMissionControl::lateralBegin() {
    const auto MON = m_monitor.lock();
    if (m_finished || m_closing || m_progress < 1.0 || !MON || !MON->m_activeWorkspace)
        return;

    // Vecinos: los escritorios numerados de este monitor, por número. Pasado
    // el último, uno nuevo (el primer número libre), como el gesto nativo
    const auto CURRENT = MON->m_activeWorkspace->numberedID();
    if (!CURRENT)
        return;
    PHLWORKSPACE prev;
    int64_t      next = -1;
    for (const auto& ws : State::Workspace::state()->workspacesCopy()) {
        const auto ID = ws->numberedID();
        if (!ID || ws->m_monitor.lock() != MON)
            continue;
        if (*ID < *CURRENT && (!prev || *ID > *prev->numberedID()))
            prev = ws;
        if (*ID > *CURRENT && (next == -1 || *ID < next))
            next = *ID;
    }
    if (next == -1)
        next = firstFreeWorkspaceAfter(*CURRENT);

    m_lateral.prev      = prev;
    m_lateral.nextID    = next;
    m_lateral.active    = true;
    m_lateral.animating = false;
    m_lateral.base      = m_lateral.value;
    m_lateral.total     = 0.0;
    m_lateral.samples.clear();
    m_hovered.reset();
    layout();
    damage();
}

void CMissionControl::lateralUpdate(double dx, uint32_t timeMs) {
    if (!m_lateral.active)
        return;
    m_lateral.total += dx;

    // Dedos a la izquierda = escritorio siguiente (el contenido va con el dedo)
    double value = m_lateral.base - m_lateral.total / std::max<int64_t>(*g_pConfig->gestureDistance, 1);
    if (value < 0 && !m_lateral.prev)
        value = std::max(-0.12, value * 0.25); // no hay anterior: resistencia elástica
    m_lateral.value = std::clamp(value, -1.0, 1.0);

    std::erase_if(m_lateral.samples, [timeMs](const auto& s) { return timeMs - s.first > 100; });
    m_lateral.samples.emplace_back(timeMs, m_lateral.total);
    damage();
}

void CMissionControl::lateralEnd(bool cancelled) {
    if (!m_lateral.active)
        return;
    m_lateral.active = false;

    double speed = 0.0; // en el sentido de value
    if (m_lateral.samples.size() >= 2) {
        const auto& [t0, r0] = m_lateral.samples.front();
        const auto& [t1, r1] = m_lateral.samples.back();
        if (t1 > t0)
            speed = -(r1 - r0) / ((t1 - t0) / 1000.0);
    }

    double target;
    if (cancelled)
        target = 0;
    else if (std::abs(speed) > *g_pConfig->flingSpeed)
        target = speed > 0 ? 1 : -1;
    else
        target = std::abs(m_lateral.value) < 0.5 ? 0 : (m_lateral.value > 0 ? 1 : -1);
    if (target < 0 && !m_lateral.prev)
        target = 0;

    m_lateral.animating = true;
    m_lateral.from      = m_lateral.value;
    m_lateral.to        = target;
    m_lateral.duration  = std::max(0.08, ms(*g_pConfig->slideDuration) * std::abs(target - m_lateral.value));
    m_lateral.start     = Time::steadyNow();
    damage();
}

void CMissionControl::lateralArrive(int direction) {
    // Se cambia de escritorio de verdad, por debajo de la vista (que lo tapa
    // todo); a partir de aquí el nuevo es el actual y el desplazamiento vuelve a 0
    if (direction < 0) {
        if (const auto PREV = m_lateral.prev.lock())
            (void)Config::Actions::changeWorkspace(PREV);
    } else
        (void)Config::Actions::changeWorkspace(std::to_string(m_lateral.nextID));

    m_lateral.value = 0.0;
    m_lateral.prevSlots.clear();
    m_lateral.nextSlots.clear();
    layout();
}

// ── Maquetación ────────────────────────────────────────────────────────────

void CMissionControl::layout() {
    const auto MON = m_monitor.lock();
    if (!MON)
        return;

    m_workspace = MON->m_activeWorkspace;
    layoutStrip();
    m_slots = layoutFor(MON->m_activeWorkspace);

    if (m_lateral.active || m_lateral.animating) {
        m_lateral.prevSlots = layoutFor(m_lateral.prev.lock());
        m_lateral.nextSlots.clear();
        for (const auto& ws : State::Workspace::state()->workspacesCopy()) {
            if (ws->numberedID() && sc<int64_t>(*ws->numberedID()) == m_lateral.nextID)
                m_lateral.nextSlots = layoutFor(ws);
        }
    }
}

std::vector<CMissionControl::SSlot> CMissionControl::layoutFor(const PHLWORKSPACE& workspace) const {
    std::vector<SSlot> slots;
    const auto         MON = m_monitor.lock();
    if (!MON || !workspace)
        return slots;

    std::vector<PHLWINDOW> windows;
    for (const auto& w : Desktop::windowState()->windows()) {
        if (!w->mapped() || w->isHidden() || w->m_workspace != workspace)
            continue;
        windows.push_back(w);
    }

    // De arriba abajo y de izquierda a derecha: la cuadrícula se parece a la
    // colocación real
    const auto GOAL = [&MON](const PHLWINDOW& w) { return CBox{w->position(IGeometric::GEOMETRIC_GOAL) - MON->m_position, w->size(IGeometric::GEOMETRIC_GOAL)}; };
    std::ranges::sort(windows, [&GOAL](const auto& a, const auto& b) {
        const auto A = GOAL(a), B = GOAL(b);
        return A.y != B.y ? A.y < B.y : A.x < B.x;
    });

    if (windows.empty())
        return slots;

    const double MARGINX = *g_pConfig->marginX, GAP = *g_pConfig->gap, MAX_SCALE = *g_pConfig->maxScale;
    const CBox   AREA    = {MARGINX, m_stripBottom, MON->m_size.x - 2 * MARGINX, MON->m_size.y - m_stripBottom - *g_pConfig->marginBottom};

    // Se prueba con 1, 2, 3... filas y se queda la que deja las miniaturas
    // más grandes. Nunca se agranda una ventana
    size_t              bestRows = 1, bestCols = windows.size();
    double              bestArea = -1;
    std::vector<double> bestScales;
    for (size_t rows = 1; rows <= windows.size(); ++rows) {
        const size_t        COLS  = (windows.size() + rows - 1) / rows;
        const double        CELLW = AREA.w / COLS, CELLH = AREA.h / rows;
        std::vector<double> scales;
        double              total = 0;
        for (const auto& w : windows) {
            const auto   B = GOAL(w);
            const double S = std::max(0.01, std::min({MAX_SCALE, (CELLW - GAP) / std::max(1.0, B.w), (CELLH - GAP) / std::max(1.0, B.h)}));
            scales.push_back(S);
            total += B.w * S * B.h * S;
        }
        if (total > bestArea) {
            bestArea   = total;
            bestRows   = rows;
            bestCols   = COLS;
            bestScales = std::move(scales);
        }
    }

    // Las filas se centran; la última puede ir con menos ventanas
    const double CELLH = AREA.h / bestRows;
    for (size_t row = 0; row < bestRows; ++row) {
        const size_t FIRST = row * bestCols, LAST = std::min(windows.size(), FIRST + bestCols);
        double       rowWidth = GAP * (LAST - FIRST - 1);
        for (size_t i = FIRST; i < LAST; ++i)
            rowWidth += GOAL(windows[i]).w * bestScales[i];

        double       x  = AREA.x + (AREA.w - rowWidth) / 2;
        const double CY = AREA.y + CELLH * row + CELLH / 2;
        for (size_t i = FIRST; i < LAST; ++i) {
            const auto B = GOAL(windows[i]);
            const auto W = B.w * bestScales[i], H = B.h * bestScales[i];
            slots.push_back({.window = windows[i], .target = {x, CY - H / 2, W, H}});
            x += W + GAP;
        }
    }
    return slots;
}

const CMissionControl::SSlot* CMissionControl::slotFor(const PHLWINDOW& window) const {
    for (const auto& s : m_slots) {
        if (s.window.lock() == window)
            return &s;
    }
    return nullptr;
}

CBox CMissionControl::boxFor(const PHLWINDOW& window, const SSlot& slot, double offsetX) const {
    const auto MON  = m_monitor.lock();
    const CBox REAL = {window->position(IGeometric::GEOMETRIC_CURRENT) - MON->m_position, window->size(IGeometric::GEOMETRIC_CURRENT)};
    return lerpBox(REAL, slot.target, m_progress).translate(Vector2D{offsetX, 0.0});
}

PHLWINDOW CMissionControl::windowAt(const Vector2D& pos) const {
    for (const auto& s : m_slots) {
        if (s.target.containsPoint(pos))
            return s.window.lock();
    }
    return nullptr;
}

void CMissionControl::updateHover() {
    const bool STILL   = m_progress >= 1.0 && !m_gesture.active && !m_lateral.active && !m_lateral.animating;
    const auto HOVERED = STILL ? windowAt(m_mouse) : nullptr;
    const auto CARD    = STILL && !HOVERED ? cardAt(m_mouse) : -1;
    if (CARD != m_hoveredCard) {
        m_hoveredCard = CARD;
        damage();
    }
    if (HOVERED != m_hovered.lock()) {
        m_hovered = HOVERED;
        damage();
    }
}

// ── Render ─────────────────────────────────────────────────────────────────

void CMissionControl::render(Render::CRenderContext& ctx, const Time::steady_tp& now) {
    const auto MON = m_monitor.lock();
    if (!MON)
        return;

    const auto& R     = g_pHyprRenderer;
    const float SCALE = MON->m_scale;

    // Hyprland descarta lo que queda tapado por regiones opacas, pero las
    // calcula con la posición real de cada ventana, no con la de la vista:
    // una maximizada "taparía" a las demás. Aquí se dibuja todo
    ctx.m_data.noSimplify = true;

    // Fondo de pantalla y capas de abajo, como siempre
    R->renderMonitorBackground(ctx, MON, now, nullptr, Render::eSceneMode::MONITOR);
    for (const auto& ls : MON->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM])
        R->renderLayer(ctx, ls.lock(), MON, now);

    // Velo que se oscurece al abrir
    if (m_progress > 0) {
        CRectPassElement::SRectData dim;
        dim.box   = {0, 0, MON->m_transformedSize.x, MON->m_transformedSize.y};
        dim.color = CHyprColor(0, 0, 0, std::clamp<double>(*g_pConfig->dim, 0, 1) * m_progress);
        Render::IHyprRenderer::addPassElement(ctx, makeUnique<CRectPassElement>(dim));
    }

    const auto addRect = [&ctx, SCALE](const CBox& box, const CHyprColor& color, double round) {
        CRectPassElement::SRectData rect;
        rect.box   = CBox{box}.scale(SCALE).round();
        rect.color = color;
        rect.round = std::round(round * SCALE);
        Render::IHyprRenderer::addPassElement(ctx, makeUnique<CRectPassElement>(rect));
    };
    const auto addText = [&ctx, SCALE](const SP<Render::ITexture>& tex, double centerX, double y, float alpha) {
        if (!tex)
            return;
        CTexPassElement::SRenderData data;
        data.tex = tex;
        data.box = CBox{std::round(centerX * SCALE - tex->m_size.x / 2), std::round(y * SCALE), tex->m_size.x, tex->m_size.y};
        data.a   = alpha;
        Render::IHyprRenderer::addPassElement(ctx, makeUnique<CTexPassElement>(data));
    };
    const auto withTransform = [&ctx](float scale, const Vector2D& translatePx, const auto& draw) {
        Render::SRenderModifData modif;
        modif.modifs.emplace_back(Render::SRenderModifData::RMOD_TYPE_SCALE, scale);
        modif.modifs.emplace_back(Render::SRenderModifData::RMOD_TYPE_TRANSLATE, translatePx);
        Render::IHyprRenderer::addPassElement(ctx, makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{modif}));
        draw();
        Render::IHyprRenderer::addPassElement(ctx, makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{Render::SRenderModifData{}}));
    };
    // Opacidad completa y sin el desplazamiento del cambio de escritorio
    // nativo: Hyprland desvanece las ventanas tapadas por una maximizada (y las
    // de otros escritorios) y las mueve al cambiar; aquí las coloca la vista
    const auto presentationFor = [](const PHLWINDOW& w) {
        auto p                     = w->presentation().renderPresentation(Render::eSceneMode::MONITOR);
        p.alpha                    = 1.F;
        p.fadeAlpha                = 1.F;
        p.workspaceAlpha           = 1.F;
        p.alphaVisible             = true;
        p.workspaceOffset          = {};
        p.workspaceOffsetAnimating = false;
        return p;
    };

    // Tira de escritorios: baja desde arriba al abrir
    const double STRIPY  = -(1.0 - m_progress) * m_stripBottom;
    const auto   CURRENT = MON->m_activeWorkspace ? MON->m_activeWorkspace->numberedID() : std::nullopt;
    for (const auto& card : m_cards) {
        const auto WS = card.workspace.lock();
        if (!WS)
            continue;
        const CBox BOX       = CBox{card.box}.translate(Vector2D{0.0, STRIPY});
        const bool ISCURRENT = CURRENT && sc<int64_t>(*CURRENT) == card.id;
        const bool HOVER     = m_hoveredCard == card.id;

        if (ISCURRENT || HOVER)
            addRect(CBox{BOX.x - 3, BOX.y - 3, BOX.w + 6, BOX.h + 6}, CHyprColor(1, 1, 1, (HOVER ? 0.9 : 0.55) * m_progress), 9);
        addRect(BOX, CHyprColor(0.12, 0.12, 0.15, 1.0), 6);

        // El escritorio en miniatura, en vivo: su fondo y sus ventanas en su sitio
        withTransform(BOX.w / MON->m_size.x, BOX.pos() * SCALE, [&] {
            // Solo la capa del fondo de pantalla: renderMonitorBackground empieza
            // limpiando la pantalla entera (sin escalar) y borraba lo anterior
            for (const auto& ls : MON->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND])
                R->renderLayer(ctx, ls.lock(), MON, now);
            for (const auto& w : Desktop::windowState()->windows()) {
                if (w->mapped() && !w->isHidden() && w->m_workspace == WS)
                    R->renderWindow(ctx, w, MON, presentationFor(w), now, true, Render::RENDER_PASS_ALL);
            }
        });

        addText(text(workspaceLabel(card.id), *g_pConfig->fontSize, ISCURRENT), BOX.x + BOX.w / 2, BOX.y + BOX.h + LABEL_GAP, m_progress);
    }

    // "+": escritorio nuevo
    if (m_newButton.w > 0) {
        const CBox BOX = CBox{m_newButton}.translate(Vector2D{0.0, STRIPY});
        addRect(BOX, CHyprColor(1, 1, 1, (m_hoveredCard == NEW_CARD ? 0.30 : 0.16) * m_progress), 8);
        const double ARM = BOX.w * 0.22, THICK = 2.5;
        addRect({BOX.x + BOX.w / 2 - ARM, BOX.y + BOX.h / 2 - THICK / 2, ARM * 2, THICK}, CHyprColor(1, 1, 1, 0.9 * m_progress), 0);
        addRect({BOX.x + BOX.w / 2 - THICK / 2, BOX.y + BOX.h / 2 - ARM, THICK, ARM * 2}, CHyprColor(1, 1, 1, 0.9 * m_progress), 0);
    }

    // Ventanas de un escritorio, desplazadas en horizontal (deslizamiento lateral)
    const auto HOVERED     = m_hovered.lock();
    const auto drawWindows = [&](const std::vector<SSlot>& slots, double offsetX) {
        const auto findSlot = [&slots](const PHLWINDOW& w) -> const SSlot* {
            for (const auto& s : slots) {
                if (s.window.lock() == w)
                    return &s;
            }
            return nullptr;
        };

        // En el orden de apilado, con la de debajo del ratón (o la elegida al cerrar) encima
        std::vector<PHLWINDOW> order;
        for (const auto& w : Desktop::windowState()->windows()) {
            if (w != HOVERED && findSlot(w))
                order.push_back(w);
        }
        if (HOVERED && findSlot(HOVERED))
            order.push_back(HOVERED);

        for (const auto& w : order) {
            const auto* SLOT = findSlot(w);
            if (!SLOT || !w->mapped())
                continue;

            const CBox REAL = {w->position(IGeometric::GEOMETRIC_CURRENT) - MON->m_position, w->size(IGeometric::GEOMETRIC_CURRENT)};
            const CBox BOX  = boxFor(w, *SLOT, offsetX);
            if (REAL.w <= 0 || REAL.h <= 0 || BOX.x > MON->m_size.x || BOX.x + BOX.w < 0)
                continue;

            if (w == HOVERED && m_progress > 0) {
                CRectPassElement::SRectData ring;
                ring.box   = CBox{BOX.x - HOVER_RING, BOX.y - HOVER_RING, BOX.w + 2 * HOVER_RING, BOX.h + 2 * HOVER_RING}.scale(SCALE);
                ring.color = CHyprColor(1, 1, 1, 0.85 * m_progress);
                ring.round = std::round(14 * SCALE);
                Render::IHyprRenderer::addPassElement(ctx, makeUnique<CRectPassElement>(ring));
            }

            // La ventana real, escalada y movida: box_px * s + t = destino_px
            const float              S = BOX.w / REAL.w;
            Render::SRenderModifData modif;
            modif.modifs.emplace_back(Render::SRenderModifData::RMOD_TYPE_SCALE, S);
            modif.modifs.emplace_back(Render::SRenderModifData::RMOD_TYPE_TRANSLATE, (BOX.pos() - REAL.pos() * S) * SCALE);
            Render::IHyprRenderer::addPassElement(ctx, makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{modif}));

            R->renderWindow(ctx, w, MON, presentationFor(w), now, true, Render::RENDER_PASS_ALL);

            Render::IHyprRenderer::addPassElement(ctx, makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{Render::SRenderModifData{}}));
        }
    };

    // El actual se aparta con el dedo y el vecino entra por el otro lado
    const double SHIFT = -m_lateral.value * MON->m_size.x;
    if (m_lateral.value < 0)
        drawWindows(m_lateral.prevSlots, SHIFT - MON->m_size.x);
    else if (m_lateral.value > 0)
        drawWindows(m_lateral.nextSlots, SHIFT + MON->m_size.x);
    drawWindows(m_slots, SHIFT);

    // Título bajo la ventana señalada, sobre una pastilla oscura
    if (*g_pConfig->showTitles && HOVERED && !m_closing && m_lateral.value == 0) {
        if (const auto* SLOT = slotFor(HOVERED)) {
            const auto TITLE = HOVERED->metadata().title();
            const auto TEX   = text(TITLE.empty() ? HOVERED->metadata().appID() : TITLE, *g_pConfig->fontSize, false, std::max(160.0, SLOT->target.w));
            if (TEX) {
                const double W = TEX->m_size.x / SCALE, H = TEX->m_size.y / SCALE;
                const double CX = SLOT->target.x + SLOT->target.w / 2, Y = SLOT->target.y + SLOT->target.h + 12;
                addRect({CX - W / 2 - 10, Y - 4, W + 20, H + 8}, CHyprColor(0.08, 0.08, 0.1, 0.8 * m_progress), (H + 8) / 2);
                addText(TEX, CX, Y, m_progress);
            }
        }
    }

    // La barra y lo que va por encima, sin tocar
    for (const auto LAYER : {ZWLR_LAYER_SHELL_V1_LAYER_TOP, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY}) {
        for (const auto& ls : MON->m_layerSurfaceLayers[LAYER])
            R->renderLayer(ctx, ls.lock(), MON, now);
    }
    for (const auto& layer : MON->m_layerSurfaceLayers) {
        for (const auto& ls : layer)
            R->renderLayer(ctx, ls.lock(), MON, now, true);
    }
}

