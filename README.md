# 🎯 ShooterGame

FPS arcade multijugador desarrollado en **Unreal Engine 5** (C++), para PC vía Steam.

> **Estado actual:** sistemas de combate multijugador implementados en C++ (red
> autoritativa, compensación de latencia, armas por datos, reglas de partida y HUD).
> Falta el contenido: mapas, personajes, armas y texturas.

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

### Configuración inicial (una sola vez, dentro del editor)

El código no trae assets: hay que crear el arma como dato. Son unos minutos y es
exactamente el flujo de trabajo previsto — el balance vive en assets, no en C++.

**1. Crear el arma (dato de balance)**
- `Content Browser → Add → Miscellaneous → Data Asset`
- Elegir la clase **WeaponDataAsset**, nombrarlo `DA_RifleAsalto`
- Abrirlo y ajustar los valores (los que trae por defecto ya son jugables:
  25 de daño, 600 RPM, cargador de 30)

**2. Crear el actor del arma**
- `Add → Blueprint Class →` buscar y elegir **WeaponBase**, nombrarlo `BP_RifleAsalto`
- Abrirlo y en *Weapon Data* asignar `DA_RifleAsalto`

**3. Crear el personaje**
- `Add → Blueprint Class →` buscar y elegir **ShooterCharacter**, nombrarlo `BP_Jugador`
- Seleccionar el componente **Inventory** → en *Default Weapons* agregar un elemento
  y asignar `BP_RifleAsalto`

**4. Crear el modo de juego**
- `Add → Blueprint Class →` buscar y elegir **ShooterGameMode**, nombrarlo `BP_ModoJuego`
- En *Default Pawn Class* asignar `BP_Jugador`
- `Project Settings → Maps & Modes → Default GameMode` → `BP_ModoJuego`

**5. Crear el nivel**
- `File → New Level → Basic` (trae piso e iluminación)
- Agregar dos o más **Player Start** separados entre sí
- Opcional: agregar algún **Target Actor** como blanco de prueba
- Guardar el nivel y presionar **Play**

### Probar el multijugador

En el editor: `Play → Net Mode → Play As Client`, y subir *Number of Players* a 2 o
más. Para validar de verdad hay que probar con **150 ms de latencia simulada**
(consola: `Net PktLag=150`), que es donde se nota si la compensación de latencia
funciona.

### Controles

| Acción | Tecla |
|--------|-------|
| Moverse | `W A S D` |
| Mirar | Mouse |
| Disparar | Click izquierdo |
| Apuntar | Click derecho |
| Recargar | `R` |
| Cambiar arma | `Q` / rueda del mouse |
| Correr | `Shift` |
| Agacharse | `Ctrl` |
| Saltar | `Espacio` |

Todo el esquema está también mapeado a mando.

---

## 🗂️ ESTRUCTURA

```
ShooterGame/
├── Config/                     Configuración del proyecto (.ini)
├── Source/
│   ├── ShooterGame.Target.cs         Build de cliente
│   ├── ShooterGameServer.Target.cs   Build de servidor dedicado
│   └── ShooterGame/
│       ├── Core/               GameMode, GameState, PlayerState
│       ├── Player/             Personaje y controlador
│       ├── Weapons/            Armas, datos de balance e inventario
│       ├── Combat/             Salud, compensación de latencia, blancos
│       └── UI/                 HUD
├── docs/                       Documentación del proyecto
└── .gitattributes              Configuración de Git LFS
```

---

## 🛣️ HOJA DE RUTA

| Fase | Estado |
|------|:------:|
| Documentación y diseño | ✅ |
| Sistemas de combate multijugador en C++ | ✅ |
| F0 · Preproducción y pipeline | 🔄 |
| F1 · Rebanada vertical (contenido y validación) | 🔄 |
| F2 · Producción de contenido | ⬜ |
| F3 · Beta | ⬜ |
| F4 · Lanzamiento en Steam | ⬜ |

Detalle completo en [PLANIFICACION.md](docs/PLANIFICACION.md).
