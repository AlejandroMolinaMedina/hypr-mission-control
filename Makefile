# Instalación: el código fuente y el cargador. El plugin no se instala
# compilado; lo compila el cargador para la versión de Hyprland que corre.
#
#   make install                        en /usr/local
#   make install PREFIX=~/.local        solo para tu usuario
#   make install PREFIX=/usr DESTDIR=…  (paquetes)

PREFIX  ?= /usr/local
DESTDIR ?=

SHARE = $(DESTDIR)$(PREFIX)/share/hypr-mission-control
BIN   = $(DESTDIR)$(PREFIX)/bin

SOURCES = $(wildcard src/*.cpp src/*.hpp) src/Makefile

# Comprobar que compila (no instala nada)
build:
	$(MAKE) -C src OUT=$(CURDIR)/build/missioncontrol.so

install:
	install -Dm755 hypr-mission-control $(BIN)/hypr-mission-control
	install -dm755 $(SHARE)/src
	install -m644 $(SOURCES) $(SHARE)/src/
	install -Dm644 README.md $(DESTDIR)$(PREFIX)/share/doc/hypr-mission-control/README.md
	install -Dm644 LICENSE $(DESTDIR)$(PREFIX)/share/licenses/hypr-mission-control/LICENSE

uninstall:
	rm -f $(BIN)/hypr-mission-control
	rm -rf $(SHARE) $(DESTDIR)$(PREFIX)/share/doc/hypr-mission-control $(DESTDIR)$(PREFIX)/share/licenses/hypr-mission-control

clean:
	rm -rf build

.PHONY: build install uninstall clean
