# 🔧 Sistemas implementados

Guía de los subsistemas de `Source/Shoter`, qué requerimiento cubre cada uno y
cómo usarlos desde Blueprint o C++.

---

## Progresión — `UShooterProgressionSubsystem`

Vive en el GameInstance, así que sobrevive a los cambios de nivel. Guarda en
`Saved/SaveGames/ShooterProfile.sav`.

**Cubre:** RF-1.3, RF-9.1, RF-9.2, RF-9.3, RF-9.5, RF-9.6

```cpp
UShooterProgressionSubsystem* Progression =
    GetGameInstance()->GetSubsystem<UShooterProgressionSubsystem>();

Progression->AddExperience(100);
Progression->RecordMatchResult(Kills, Deaths, Assists);

int32 Current, Required;
Progression->GetLevelProgress(Current, Required);   // barra de XP del menú
```

### Curva de niveles

Cada nivel cuesta un 15 % más que el anterior, partiendo de 1000 XP:

| Nivel | XP acumulada |
|---:|---:|
| 2 | 1 000 |
| 5 | 4 993 |
| 10 | 13 337 |
| 20 | 44 275 |

### Desbloqueos por nivel (RF-9.3)

| Nivel | Arma |
|---:|---|
| 1 | Rifle |
| 3 | Pistola |
| 6 | Escopeta |
| 10 | Subfusil |
| 15 | Francotirador |
| 20 | Lanzagranadas |

La tabla está en `GetUnlockTable()`, dentro del `.cpp`. Para producción conviene
moverla a un DataTable (RNF-9.3).

---

## Ajustes — `UShooterSettingsSubsystem`

Los de vídeo se delegan en `UGameUserSettings`, que es quien sabe aplicarlos.
Los de juego viven en el perfil, para que persistan (RF-11.9).

**Cubre:** RF-11.1, 11.3, 11.4, 11.5, 11.6, 11.7, 11.8, 11.9

```cpp
UShooterSettingsSubsystem* Settings =
    GetGameInstance()->GetSubsystem<UShooterSettingsSubsystem>();

Settings->SetMouseSensitivity(1.8f);      // ya afecta a la cámara
Settings->SetQualityPreset(2);            // 0 baja … 4 cinemática
Settings->SetCrosshairColor(FLinearColor::Yellow);
Settings->ApplyAndSave();
```

Todos los métodos son `BlueprintCallable`: el menú de opciones se puede montar
en UMG sin escribir C++.

### Qué está conectado

| Ajuste | Efecto inmediato |
|---|---|
| Sensibilidad, inversión Y | `ShooterCharacter::Look` |
| Color de mira | `ShooterHUD::DrawCrosshair` |
| Calidad, VSync, FPS, ventana | `UGameUserSettings` |
| Volúmenes | Guardados; falta enrutarlos a Sound Classes |
| Daltonismo | Guardado; falta aplicar la paleta |

---

## Telemetría — `UShooterTelemetrySubsystem`

Escribe JSON Lines en `Saved/Telemetry/events_AAAAMMDD.jsonl`. Un objeto por
línea, apto para ingestión por lotes.

**Cubre:** RF-13.1, RF-13.4

```json
{"ts":"2026-09-11T14:22:01Z","session":"A1B2-...","event":"match_start","map":"Lvl_Entrenamiento","mode":"Equipos"}
{"ts":"2026-09-11T14:30:44Z","session":"A1B2-...","event":"match_end","duration":"523.0","kills":"14","deaths":"9"}
```

El formato ya es el que consumiría un servicio de análisis: migrar a backend es
cambiar el destino, no el contenido. `bEnabled = false` corta el registro sin
tocar las llamadas.

---

## Registro de eliminaciones

El `AShooterGameState` replica las últimas 8 eliminaciones; el HUD dibuja las 5
más recientes y las desvanece a los 6 segundos.

**Cubre:** RF-7.7 (killfeed), RF-6.6 (quién te mató)

```cpp
ShooterGameState->AddKillEvent(KillerName, VictimName, WeaponName, bHeadshot);
```

Las líneas que involucran al jugador local se destacan en ámbar. Al morir, el
panel de reaparición muestra quién te eliminó y con qué arma.

---

## Tablas de posiciones

| Cuándo | Función | Requerimiento |
|---|---|---|
| Durante la partida, con `Tab` | `DrawLiveScoreboard` | RF-7.8 |
| Al terminar | `DrawScoreboard` | RF-8.6 |

Ambas ordenan por bajas y, a igualdad, por menos muertes. La fila propia se
resalta. En modo por equipos muestra el marcador; en todos contra todos, el
jugador que encabeza.

---

## Blancos de entrenamiento — `ATargetActor`

Esferas con retroalimentación inmediata:

- Destello blanco de 0.12 s al impactar
- Tinte de azul a rojo según la salud restante
- Reaparición configurable (3 s por defecto)
- Vaivén opcional: `SwingAmplitude` y `SwingSpeed`

El servidor decide el daño; los clientes solo reproducen el efecto (RNF-3.1).

---

## Qué falta conectar

| Pendiente | Por qué |
|---|---|
| UI de ajustes | La lógica está completa; falta el menú UMG (RF-2.1) |
| Volúmenes → Sound Classes | Requiere crear los assets de audio |
| Paleta de daltonismo | Requiere post-proceso o materiales de HUD |
| Telemetría → backend | Dependencia externa |
| Progreso en servidor | RNF-3.4, dependencia externa |

---

## Menú — `UShooterMenuSubsystem` y `UShooterMenuWidget`

La navegación vive en C++; los widgets solo componen (RNF-9.2).

**Cubre:** RF-2.1, RF-2.2, RF-2.3, RF-2.4, RF-3.4

### Pantallas

`EShooterMenuScreen` define seis: `Main`, `Play`, `Arsenal`, `Progression`,
`Options`, `Pause`. El subsistema mantiene una pila de navegación, así que
`GoBack` vuelve a la anterior sin que el widget lleve la cuenta.

```cpp
UShooterMenuSubsystem* Menu =
    GetGameInstance()->GetSubsystem<UShooterMenuSubsystem>();

Menu->OpenScreen(EShooterMenuScreen::Options);
Menu->GoBack();
Menu->TogglePause();                       // Escape en partida
Menu->StartMatch(TEXT("Lvl_Entrenamiento"));
```

### Para montar un widget

1. Ejecutar `Content/Python/build_menu.py`: crea los seis `WBP_*` derivados de
   `ShooterMenuWidget`, y el nivel `Lvl_Menu`
2. Abrir cada widget, colocar botones y enlazar su *OnClicked* a las funciones
   heredadas: `OpenScreen`, `GoBack`, `StartMatch`, `QuitGame`, `LeaveMatch`
3. `Project Settings → Game → Shooter Menu Subsystem → Screen Widgets`:
   asignar cada widget a su pantalla

El paso 2 es diseño visual: los widgets de UMG son assets binarios que se
construyen arrastrando elementos, no por código.

### Datos listos para mostrar

```cpp
int32 Level, CurrentXP, RequiredXP;
GetProgressSummary(Level, CurrentXP, RequiredXP);   // barra de XP (RF-2.3)

GetStatsSummary();   // "K/D 1.42 · Precisión 34 % · 27 partidas"
GetPlayerName();
SetPlayerName(FText::FromString(TEXT("Felipe")));   // RF-1.3
```

### Modo de entrada

Con el menú abierto se usa `FInputModeGameAndUI` y cursor visible. El juego
**no se pausa**: un FPS multijugador no puede detenerse porque un jugador abra
las opciones.
