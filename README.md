# Hypr Mission Control

Una herramienta de gestión de gestos y control de misión diseñada para entornos basados en Hyprland.

![C++](https://img.shields.io/badge/Language-C++-blue)
![License](https://img.shields.io/badge/License-MIT-green)

## Características Principales

*   **Gestión de Gestos:** Procesamiento eficiente de entradas para mejorar la navegación en entornos de escritorio Linux.
*   **Mission Control:** Implementación de funcionalidades de control de ventanas similares a entornos de escritorio modernos.
*   **Altamente Configurable:** Arquitectura modular para ajustar el comportamiento del sistema según las preferencias del usuario.
*   **Optimizado:** Escrito en C++ para garantizar un bajo consumo de recursos y alta capacidad de respuesta.

## Guía de Inicio Rápido

### Requisitos previos
*   Un compilador compatible con C++17 o superior.
*   `make` para la construcción del proyecto.
*   Librerías de desarrollo de Hyprland instaladas en el sistema.

### Instalación
Para compilar y ejecutar el proyecto:

```bash
# Clonar el repositorio
git clone <url-del-repositorio>
cd hypr-mission-control

# Compilar el proyecto
make

# Ejecutar el binario generado
./hypr-mission-control
```

Para usuarios de Arch Linux, el repositorio incluye soporte para AUR:
```bash
cd aur
makepkg -si
```

## Estructura del Proyecto

```text
.
├── src/                # Código fuente principal
│   ├── Gestures.cpp/hpp # Lógica de procesamiento de gestos
│   ├── MissionControl.cpp/hpp # Lógica de control de misión
│   ├── Config.cpp/hpp   # Gestión de configuraciones
│   └── main.cpp         # Punto de entrada
├── aur/                # Archivos para empaquetado en AUR
├── Makefile            # Automatización de compilación
├── LICENSE             # Términos de licencia
└── README.md           # Documentación del proyecto
```

## Soporte y Documentación

Si encuentras algún error o deseas solicitar una nueva funcionalidad, por favor abre un *Issue* en el repositorio oficial. Para discusiones generales, consulta la documentación interna del código fuente.

## Mantenimiento y Contribución

Este proyecto es de código abierto. Si deseas contribuir, asegúrate de seguir las buenas prácticas reflejadas en el código existente y enviar un *Pull Request*. Consulta el archivo `LICENSE` para conocer los términos de uso y distribución.