# 🎯 ShooterGame

FPS arcade multijugador desarrollado en **Unreal Engine 5** (C++), para PC vía Steam.

> **Estado actual:** prototipo single-player jugable + documentación de proyecto completa.
> El desarrollo del multijugador comienza en la Fase 1 (ver planificación).

---

## 📚 DOCUMENTACIÓN

| Documento | Contenido |
|-----------|-----------|
| [**GDD.md**](docs/GDD.md) | Diseño del juego: pilares, armas, modos, mapas, progresión, monetización |
| [**REQUERIMIENTOS.md**](docs/REQUERIMIENTOS.md) | Requerimientos funcionales y no funcionales (RF/RNF) priorizados |
| [**ARQUITECTURA.md**](docs/ARQUITECTURA.md) | Modelo de red, clases, decisiones técnicas irreversibles |
| [**PLANIFICACION.md**](docs/PLANIFICACION.md) | Fases, sprints, hitos, riesgos, presupuesto y marketing |

**Si es tu primera vez en el proyecto, lee en este orden:** GDD → Requerimientos →
Arquitectura → Planificación.

---

## 🚀 CÓMO EMPEZAR

### Requisitos

- **Unreal Engine 5.3+** (vía Epic Games Launcher)
- **Windows:** Visual Studio 2022 con el workload *Game development with C++*
- **Mac:** Xcode
- **Git LFS** — obligatorio (ver abajo)

### Instalación

```bash
# 1. Instalar Git LFS (una sola vez por máquina)
git lfs install

# 2. Clonar
git clone https://github.com/FelipeVeraMeza/Shootergame.git
cd Shootergame
```

> ⚠️ **Git LFS es obligatorio.** Los archivos `.uasset`, `.umap`, texturas y modelos
> se versionan con LFS. Si clonas sin tenerlo instalado, los assets llegan como
> archivos de texto rotos.

### Abrir y compilar

1. Click derecho en `ShooterGame.uproject` → **Generate Visual Studio project files**
2. Doble click en `ShooterGame.uproject` (la primera compilación tarda varios minutos)
3. En el editor: `File → New Level → Empty Level`
4. Agregar un **Player Start** y uno o más **Target Actor** desde el panel *Place Actors*
5. Guardar el nivel y presionar **Play**

### Controles

| Acción | Tecla |
|--------|-------|
| Moverse | `W A S D` |
| Mirar | Mouse |
| Disparar | Click izquierdo |
| Saltar | `Espacio` |

---

## 🗂️ ESTRUCTURA

```
ShooterGame/
├── Config/                  Configuración del proyecto (.ini)
├── Source/ShooterGame/      Código C++
│   ├── ShooterCharacter.*   Personaje en primera persona
│   ├── TargetActor.*        Blanco destructible
│   └── ShooterGameMode.*    Reglas de partida
├── docs/                    Documentación del proyecto
└── .gitattributes           Configuración de Git LFS
```

---

## 🛣️ HOJA DE RUTA

| Fase | Estado |
|------|:------:|
| Documentación y diseño | ✅ |
| Prototipo de disparo (single-player) | ✅ |
| F0 · Preproducción y pipeline | ⬜ |
| F1 · Rebanada vertical multijugador | ⬜ |
| F2 · Producción de contenido | ⬜ |
| F3 · Beta | ⬜ |
| F4 · Lanzamiento en Steam | ⬜ |

Detalle completo en [PLANIFICACION.md](docs/PLANIFICACION.md).
