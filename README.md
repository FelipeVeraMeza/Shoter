# 🎯 Shoter

FPS táctico realista multijugador en **Unreal Engine 5.8** (C++), para PC vía Steam.

> **Estado actual:** sistemas de combate multijugador implementados en C++ (red
> autoritativa, compensación de latencia, armas por datos, reglas de partida y HUD),
> sobre el contenido de la plantilla First Person: personajes, animaciones, armas y
> mapas de prototipado.

---

## 📚 DOCUMENTACIÓN

| Documento | Contenido |
|-----------|-----------|
| [**GDD.md**](docs/GDD.md) | Diseño del juego: pilares, armas, modos, mapas, progresión, monetización |
| [**REQUERIMIENTOS.md**](docs/REQUERIMIENTOS.md) | Requerimientos funcionales y no funcionales (RF/RNF) priorizados |
| [**ARQUITECTURA.md**](docs/ARQUITECTURA.md) | Modelo de red, clases, decisiones técnicas irreversibles |
| [**PLANIFICACION.md**](docs/PLANIFICACION.md) | Fases, sprints, hitos, riesgos, presupuesto y marketing |
| [**ZONA_ENTRENAMIENTO.md**](docs/ZONA_ENTRENAMIENTO.md) | Nivel de pruebas: cómo generarlo y qué validar en él |
| [**SISTEMAS.md**](docs/SISTEMAS.md) | Los subsistemas de C++: qué hace cada uno y cómo usarlo |
| [**REALISMO.md**](docs/REALISMO.md) | Balística, heridas, puntería, HUD realista y el mapa Distrito |
| [**TRAZABILIDAD.md**](docs/TRAZABILIDAD.md) | Los 152 requerimientos contra el código, con su estado real |
| [**BITACORA.md**](docs/BITACORA.md) | Historia del proyecto: decisiones, errores y cómo se resolvieron |

**Si es tu primera vez en el proyecto, lee en este orden:** GDD → Requerimientos →
Arquitectura → Planificación.

---

## 🚀 CÓMO EMPEZAR

### Requisitos

- **Unreal Engine 5.8**
- **Visual Studio 2022** con el workload *Game development with C++*
- **Git LFS** — obligatorio (ver abajo)

### Instalación

```bash
# 1. Instalar Git LFS (una sola vez por máquina)
git lfs install

# 2. Clonar
git clone https://github.com/FelipeVeraMeza/Shoter.git
cd Shoter
```

> ⚠️ **Git LFS es obligatorio.** Los archivos `.uasset`, `.umap`, texturas y modelos
> se versionan con LFS. Si clonas sin tenerlo instalado, los assets llegan como
> archivos de texto rotos.

### Abrir y compilar

1. Click derecho en `Shoter.uproject` → **Generate Visual Studio project files**
2. Doble click en `Shoter.uproject` (la primera compilación tarda varios minutos)

Para compilar desde la línea de comandos sin abrir el editor:

```powershell
& "D:\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
    ShoterEditor Win64 Development -Project="<ruta>\Shoter.uproject" -WaitMutex
```

### Configuración inicial (una sola vez, dentro del editor)

El C++ no trae assets propios: define las clases, y el balance vive en DataAssets
(RNF-9.3). Hay dos caminos para crearlos.

#### Camino rápido: generar la zona de entrenamiento

Un script crea todos los assets, los conecta y monta un nivel jugable con galería
de tiro. En el editor, cambia **Cmd** por **Python** en la barra inferior y pega:

```python
exec(open(r"<ruta del proyecto>/Content/Python/build_training_zone.py").read())
```

Detalle completo en [ZONA_ENTRENAMIENTO.md](docs/ZONA_ENTRENAMIENTO.md).

#### Camino manual

Si prefieres entender dónde vive cada cosa, los pasos son estos.

**1. Crear el arma (dato de balance)**
- `Content Browser → Add → Miscellaneous → Data Asset`
- Elegir la clase **WeaponDataAsset**, nombrarlo `DA_RifleAsalto`
- Abrirlo y ajustar los valores (los que trae por defecto ya son jugables:
  25 de daño, 600 RPM, cargador de 30)
- En *Weapon Mesh* puedes asignar la malla de `Content/Weapons/Rifle`

**2. Crear el actor del arma**
- `Add → Blueprint Class →` buscar y elegir **WeaponBase**, nombrarlo `BP_RifleAsalto`
- Abrirlo y en *Weapon Data* asignar `DA_RifleAsalto`

**3. Crear el personaje**
- `Add → Blueprint Class →` buscar y elegir **ShooterCharacter**, nombrarlo `BP_Jugador`
- Seleccionar el componente **Inventory** → en *Default Weapons* agregar un elemento
  y asignar `BP_RifleAsalto`
- En la categoría **Input** del Blueprint, asignar:
  - *Default Mapping Contexts* → `Content/Input/IMC_Default`
  - *Move Action* → `IA_Move` · *Look Action* → `IA_Look` · *Jump Action* → `IA_Jump`
  - *Fire / Aim / Reload / Sprint / Crouch / Switch Weapon* → crear los `IA_` que falten
    y mapearlos en un Input Mapping Context propio

**4. Crear el modo de juego**
- `Add → Blueprint Class →` buscar y elegir **ShooterGameMode**, nombrarlo `BP_ModoJuego`
- En *Default Pawn Class* asignar `BP_Jugador`
- `Project Settings → Maps & Modes → Default GameMode` → `BP_ModoJuego`

**5. Crear el nivel**
- `File → New Level → Basic`, o partir de `Content/LevelPrototyping`
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
| Recargar / comprobar cargador | `R` |
| Cambiar arma | `1` / rueda del mouse |
| Correr (apuntando: contener la respiración) | `Shift` |
| Asomarse | `Q` / `E` |
| Selector de fuego | `B` |
| Vendarse | `H` |
| Granada | `G` |
| Agacharse | `Ctrl` |
| Saltar | `Espacio` |

Las teclas concretas viven en los Input Mapping Contexts, no en C++: se reasignan
sin recompilar (RF-11.2).

---

## 🗂️ ESTRUCTURA

```
Shoter/
├── Config/                     Configuración del proyecto (.ini)
├── Content/                    Assets: personajes, armas, mapas, animaciones
│   ├── Characters/             Maniquíes y animaciones
│   ├── FirstPerson/            Plantilla base en Blueprints
│   ├── Input/                  Input Actions y Mapping Contexts
│   ├── LevelPrototyping/       Geometría y materiales de bloqueo
│   ├── Variant_Shooter/        Shooter en Blueprints (referencia, intacto)
│   └── Weapons/                Mallas de pistola, rifle y lanzagranadas
├── Source/
│   ├── Shoter.Target.cs              Build de cliente
│   ├── ShoterEditor.Target.cs        Build de editor
│   ├── ShoterServer.Target.cs        Build de servidor dedicado
│   └── Shoter/
│       ├── Core/               GameMode, GameState, PlayerState
│       ├── Player/             Personaje y controlador
│       ├── Weapons/            Armas, datos de balance e inventario
│       ├── Combat/             Salud, compensación de latencia, blancos
│       ├── Progression/        XP, niveles, desbloqueos y guardado
│       ├── Settings/           Ajustes de control, vídeo y audio
│       ├── Telemetry/          Registro de eventos de partida
│       └── UI/                 HUD y navegación de menús
├── Content/Python/             Scripts de generación en el editor
├── docs/                       Documentación del proyecto
└── .gitattributes              Configuración de Git LFS
```

El contenido de `Variant_Shooter` es el shooter en Blueprints de la plantilla. Se
conserva intacto como referencia y como fuente de assets; la lógica de juego real
vive en `Source/Shoter` (RNF-9.2).

---

## 🛣️ HOJA DE RUTA

| Fase | Estado |
|------|:------:|
| Documentación y diseño | ✅ |
| Sistemas de combate multijugador en C++ | ✅ |
| Port a UE 5.8 con Enhanced Input | ✅ |
| Progresión, ajustes, telemetría y menús | ✅ |
| F0 · Preproducción y pipeline | 🔄 |
| F1 · Rebanada vertical (contenido y validación) | 🔄 |
| F2 · Producción de contenido | ⬜ |
| F3 · Beta | ⬜ |
| F4 · Lanzamiento en Steam | ⬜ |

Detalle completo en [PLANIFICACION.md](docs/PLANIFICACION.md).
