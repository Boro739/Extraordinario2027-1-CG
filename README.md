# Examen Extraordinario de Laboratorio — Computación Gráfica e Interacción Humano Computadora

**Alumno:** Edgar Humberto Del Campo Valdés
**Número de cuenta:** 316532215
**Fecha de entrega:** 31 de octubre de 2026

Escena 3D en tiempo real (C++ / OpenGL 3.3) ambientada en un pueblo de la **Revolución Mexicana**, construida sobre el
proyecto base [joserapath23/Extraordinario2027-1](https://github.com/joserapath23/Extraordinario2027-1).

## Estructura

| Carpeta | Contenido |
|---|---|
| `Documentación/` | Documentación técnica (PDF), manual de usuario (PDF), video demostrativo (enlace), instalador y versión portable |
| `Código/` | Solución completa de Visual Studio 2022 con código, shaders, modelos, texturas y librerías (`External Libraries`) |

## Ejecutar

- **Instalador:** `Documentación/Instalador_ExtraordinarioCG.exe` (instala en `%LOCALAPPDATA%\ExtraordinarioCG` y crea un acceso directo en el Escritorio).
- **Portable:** descomprimir `Documentación/ExtraordinarioCG.zip` y abrir `ExtraOrdinario.exe`.

Requiere Windows 10 o superior y una tarjeta de video con OpenGL 3.3. Todas las DLL necesarias vienen incluidas.

## Compilar

Abrir `Código/ExtraOrdinario/ExtraOrdinario.sln` en Visual Studio 2022, seleccionar **Debug** o **Release** con plataforma **x86** y compilar (F5).
Las dependencias (GLFW, GLEW, GLM, Assimp, SOIL2) están dentro de `Código/ExtraOrdinario/External Libraries`.

## Controles

| Tecla | Acción |
|---|---|
| W A S D / flechas + mouse | Mover y girar la cámara |
| L | Luz direccional: blanco / naranja (intensidad 0.5) |
| Z | Zorro: caminar / detener |
| X | Zorro: saltar |
| C | Pausar / reanudar la campana (animación en bucle) |
| R / V / B | Encender o apagar las spotlights roja / verde / azul |
| Esc | Salir |

## Video demostrativo

[https://youtu.be/PMIuDqKpBuY](https://youtu.be/PMIuDqKpBuY)
