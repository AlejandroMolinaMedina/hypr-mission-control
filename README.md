# Hypr Mission Control

Hypr Mission Control es una herramienta diseñada para gestionar y controlar espacios de trabajo, ventanas y gestos en entornos de escritorio basados en Hyprland, proporcionando una interfaz eficiente para la manipulación de eventos y estados del sistema.

## Características Principales

- **Gestión de Gestos:** Implementación robusta para el reconocimiento y ejecución de acciones basadas en gestos.
- **Control de Misión:** Gestión centralizada del estado del sistema y la configuración de ventanas.
- **Configuración Dinámica:** Soporte para la carga y gestión de parámetros de configuración mediante una arquitectura modular.
- **Ligero y Eficiente:** Escrito en C++ para garantizar un rendimiento óptimo y bajo consumo de recursos.

## Guía de Inicio Rápido

### Requisitos Previos

Asegúrate de tener instaladas las herramientas de compilación necesarias y las librerías de desarrollo de Hyprland.
- `g++` (o `clang`)
- `make`
- Librerías de desarrollo de Hyprland

### Compilación e Instalación

Para compilar el proyecto, utiliza el Makefile proporcionado en la raíz o en el directorio `src`:

```bash
# Navegar al directorio fuente
cd src

# Compilar el proyecto
make

# Ejecutar el binario generado
./hypr-mission-control
```

## Estructura del Proyecto

```text
.
├── src/                # Código fuente principal
│   ├── Config.cpp      # Lógica de carga de configuración
│   ├── Gestures.cpp    # Procesamiento de gestos
│   ├── MissionControl.cpp # Lógica central del sistema
│   ├── main.cpp        # Punto de entrada
│   └── Makefile        # Script de compilación
├── aur/                # Archivos para empaquetado en AUR
├── LICENSE             # Términos de licencia
└── README.md           # Documentación del proyecto
```

## Soporte y Documentación

Si encuentras un error o tienes una sugerencia, por favor abre un *Issue* en el repositorio. Para consultas adicionales, revisa los archivos de cabecera (`.hpp`) que documentan las interfaces principales del sistema.

## Mantenimiento y Contribución

Este proyecto acepta contribuciones externas. Por favor, asegúrate de seguir las convenciones de estilo de código existentes antes de enviar un *Pull Request*. 

Este software se distribuye bajo los términos especificados en el archivo [LICENSE](./LICENSE).