# Compila el plugin contra las cabeceras de Hyprland instaladas. Solo carga en
# esa misma versión: lo normal es no llamar a esto a mano, sino dejar que lo
# haga el cargador (hypr-mission-control) cuando la versión cambia.
#
#   make OUT=/ruta/missioncontrol.so

CXXFLAGS ?= -O2
CXXFLAGS += -shared -fPIC -std=c++2b -Wno-narrowing

ifeq ($(CXX),g++)
    CXXFLAGS += --no-gnu-unique
endif

SRC  = main.cpp MissionControl.cpp Config.cpp Gestures.cpp
HDR  = MissionControl.hpp Config.hpp Gestures.hpp globals.hpp
DEPS = pixman-1 libdrm hyprland pangocairo libinput libudev wayland-server xkbcommon
OUT ?= missioncontrol.so

# A un temporal y luego se renombra: Hyprland puede tener cargado el .so
# anterior y no se debe escribir encima de él
$(OUT): $(SRC) $(HDR)
	@mkdir -p $(dir $(OUT))
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SRC) -o $(OUT).tmp `pkg-config --cflags $(DEPS)`
	mv -f $(OUT).tmp $(OUT)

clean:
	rm -f $(OUT) $(OUT).tmp

.PHONY: clean
