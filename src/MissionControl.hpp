#pragma once

// Vista general al estilo Mission Control de macOS, dentro del compositor.
//
// Mientras está abierta, el escritorio del monitor no se dibuja de la forma
// normal: se dibuja el fondo, un velo y cada ventana real (en vivo, con sus
// bordes y su contenido actual) escalada y movida a un hueco de la cuadrícula.
// La posición de partida de cada ventana se lee en cada fotograma, así que
// al cerrar acompaña a las animaciones de Hyprland sin saltos.

#include "globals.hpp"

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/helpers/time/Time.hpp>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Render {
    class CRenderContext;
    class ITexture;
}

class CMissionControl {
  public:
    explicit CMissionControl(PHLMONITOR monitor);
    ~CMissionControl();

    // Animado hasta abierta (1) o cerrada (0). Al cerrar se puede enfocar una ventana
    void open();
    void close(PHLWINDOW focus = nullptr);
    bool isClosing() const;
    bool owns(const PHLMONITOR& monitor) const;

    // El dedo manda: begin fija el punto de partida, update recibe el
    // desplazamiento de cada evento (positivo = hacia abierta) y end decide
    // si termina de abrirse o se cierra según recorrido y velocidad
    void gestureBegin();
    void gestureUpdate(double delta, uint32_t timeMs);
    void gestureEnd(bool cancelled);

    // Deslizamiento lateral con la vista abierta (gesto horizontal de 3 y 4
    // dedos): el escritorio actual se aparta y el vecino entra siguiendo al
    // dedo. dx en unidades del touchpad (positivo = dedos a la derecha)
    void lateralBegin();
    void lateralUpdate(double dx, uint32_t timeMs);
    void lateralEnd(bool cancelled);

    void render(Render::CRenderContext& ctx, const Time::steady_tp& now);
    void onPreRender(const PHLMONITOR& monitor);

  private:
    struct SSlot {
        PHLWINDOWREF window;
        CBox         target; // lógico, relativo al monitor
    };

    // Tira de escritorios de arriba: una tarjeta por escritorio del monitor
    // (fondo y ventanas en miniatura, en vivo) y un "+" para uno nuevo
    struct SCard {
        PHLWORKSPACEREF workspace;
        int64_t         id = -1;
        CBox            box; // lógico, relativo al monitor
    };
    std::vector<SCard> m_cards;
    CBox               m_newButton;
    double             m_stripBottom = 0; // donde empieza la cuadrícula de ventanas
    int64_t            m_hoveredCard = -1; // id del escritorio, NEW_CARD el "+", -1 ninguno

    PHLMONITORREF      m_monitor;
    PHLWORKSPACEREF    m_workspace;
    std::vector<SSlot> m_slots;

    double             m_progress = 0.0; // 0 = escritorio normal, 1 = vista abierta

    struct {
        bool            active   = false;
        double          from     = 0.0;
        double          to       = 0.0;
        double          duration = 0.0; // s
        Time::steady_tp start;
    } m_anim;

    struct {
        bool                                     active = false; // el dedo manda
        double                                   value  = 0.0;   // -1 = anterior, 0 = actual, 1 = siguiente
        double                                   base   = 0.0;
        double                                   total  = 0.0;
        std::vector<std::pair<uint32_t, double>> samples;
        PHLWORKSPACEREF                          prev;
        int64_t                                  nextID = -1; // número del siguiente; puede no existir aún
        std::vector<SSlot>                       prevSlots, nextSlots;
        bool                                     animating = false;
        double                                   from = 0.0, to = 0.0, duration = 0.0;
        Time::steady_tp                          start;
    } m_lateral;

    struct {
        bool                                    active = false;
        double                                  base   = 0.0;
        double                                  total  = 0.0;
        std::vector<std::pair<uint32_t, double>> samples; // (ms, recorrido)
    } m_gesture;

    bool                m_closing = false;
    bool                m_finished = false;
    bool                m_redrawPending = false;
    PHLWINDOWREF        m_hovered;
    Vector2D            m_mouse; // lógico, relativo al monitor

    CHyprSignalListener m_mouseMove, m_mouseButton, m_keyboardKey;

    void                layout();
    void                layoutStrip();
    int64_t             cardAt(const Vector2D& pos) const;
    void                goToWorkspace(int64_t id);
    SP<Render::ITexture> text(const std::string& str, int pt, bool bold, int maxWidth = 0);
    std::unordered_map<std::string, SP<Render::ITexture>> m_texts;
    std::vector<SSlot>  layoutFor(const PHLWORKSPACE& workspace) const;
    void                lateralArrive(int direction);
    void                animateTo(double target);
    void                finish();
    void                damage();
    void                updateHover();
    PHLWINDOW           windowAt(const Vector2D& pos) const;
    CBox                boxFor(const PHLWINDOW& window, const SSlot& slot, double offsetX = 0) const;
    const SSlot*        slotFor(const PHLWINDOW& window) const;
};

inline UP<CMissionControl> g_pMissionControl;
