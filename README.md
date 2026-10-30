# Hypr Mission Control

Una herramienta de gestión de gestos y control de misión diseñada específicamente para entornos Wayland/Hyprland.

![C++](https://img.shields.io/badge/language-C++-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

## Características Principales

* **Gestión de Gestos:** Soporte integrado para interpretar y procesar gestos de entrada.
* **Control de Misión:** Gestión eficiente del flujo de trabajo y ventanas mediante `MissionControl`.
* **Altamente Configurable:** Estructura modular basada en `Config.cpp` para personalizar el comportamiento del sistema.
* **Optimizado para Hyprland:** Desarrollado nativamente para integrarse con el compositor Hyprland.

## Guía de Inicio Rápido

### Requisitos previos
* Compilador compatible con C++17 o superior.
* `make` y herramientas de compilación de GCC/Clang.
* Librerías de desarrollo para Hyprland.

### Compilación e Instalación
Para compilar el proyecto, utiliza el Makefile incluido:

```bash
make
./hypr-mission-control
```

Si prefieres realizar una instalación desde el paquete AUR (Arch User Repository), puedes utilizar los archivos proporcionados en el directorio `/aur`:

```bash
cd aur
makepkg -si
```

## Estructura del Proyecto

```text
.
├── src/                # Código fuente principal
│   ├── Config.cpp      # Lógica de configuración
│   ├── Gestures.cpp    # Procesamiento de gestos
│   ├── MissionControl.cpp # Lógica de control de ventanas
│   └── main.cpp        # Punto de entrada
├── aur/                # Archivos para empaquetado en Arch Linux
├── Makefile            # Script de construcción
└── LICENSE             # Términos de la licencia
```

## Soporte y Documentación

Para reportar errores o solicitar nuevas funcionalidades, por favor utiliza el sistema de Issues del repositorio. Asegúrate de incluir logs detallados y la versión de tu compositor.

## Mantenimiento y Contribución

Este proyecto es de código abierto. Si deseas contribuir:

1. Realiza un fork del repositorio.
2. Crea una rama para tu funcionalidad (`git checkout -b feature/nueva-funcionalidad`).
3. Realiza tus cambios y envía un Pull Request.

Consulta el archivo [LICENSE](./LICENSE) para conocer los detalles sobre los términos de uso y distribución de este software.