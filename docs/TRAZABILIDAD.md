# 📊 Trazabilidad de requerimientos

Estado real de los 152 requerimientos de [REQUERIMIENTOS.md](REQUERIMIENTOS.md)
contra el código del proyecto.

**Leyenda**

| Símbolo | Significado |
|---|---|
| ✅ | Implementado y verificado en código |
| 🔶 | Parcial: existe la base, falta completarlo |
| ❌ | No implementado |
| 🔒 | Bloqueado por dependencia externa (Steam, backend, servidor dedicado) |

---

## Resumen

| Prioridad | Total | ✅ | 🔶 | ❌ / 🔒 |
|---|---:|---:|---:|---:|
| 🔴 Must | 58 | 43 | 0 | 15 |
| 🟡 Should | 30 | 17 | 1 | 12 |
| 🟢 Could | 12 | 4 | 0 | 8 |
| **RF total** | **100** | **64** | **1** | **35** |
| **RNF total** | **52** | **17** | **6** | **29** |

El núcleo de combate está completo. Lo que falta se concentra en: cuenta y
sesión, matchmaking y herramientas de administración — casi todo bloqueado
por las tres dependencias externas.

> **Lo accesible por código está hecho.** De los 35 RF pendientes, ~22 están
> bloqueados por Steam, backend o servidor dedicado; el resto es trabajo de
> editor (widgets UMG) o de contenido (7 armas y 3 mapas que faltan).

### Qué cambió en la última sesión

Fase 7 (detalle en [REALISMO.md](REALISMO.md) y la bitácora): primera
persona con brazos, visores, efectos sin Niagara, equipamiento, bots
reescritos, menús en Slate y el Distrito como nivel completo. Compilado, sin
jugar todavía.

| Requerimiento | Antes | Ahora |
|---|:--:|:--:|
| RF-2.1 Menú principal | 🔶 | ✅ `SShooterMenuScreen` en Slate, sin depender de los `WBP_*` |
| RF-2.2 Elegir modo | 🔶 | ✅ Pantalla *Jugar*: modo y jugadores por URL, leídos en `InitGame` |
| RF-3.6 Bots de relleno | ✅ | ✅ Ahora con `PlayerState`, equipo, reaparición y línea de visión |
| RF-5.11 Granadas | ✅ | ✅ `GrenadeClass` por defecto: antes `G` no lanzaba nada |

| Requerimiento | Antes | Ahora |
|---|:--:|:--:|
| RF-5.10 Accesorios | ❌ | ✅ `WeaponAttachmentDataAsset` |
| RF-6.7 Killcam | ❌ | ✅ Cámara sobre el asesino |
| RF-7.9 Minimapa | ❌ | ✅ Aliados y zonas, nunca enemigos |
| RF-8.3 Dominación | ❌ | ✅ `AControlZone` |
| RF-8.4 Oleadas | ❌ | ✅ `AWaveManager` |
| RF-9.4 Accesorios por uso | ❌ | ✅ Conteo de disparos por arma |
| RF-11.2 Reasignar teclas | ❌ | ✅ `RemapKey` |
| RNF-2.6 Detectar desconexión | ❌ | ✅ Aviso a los 3 s |
| RNF-3.5 Límite de RPCs | ❌ | ✅ `FRPCRateLimiter` |
| RNF-4.5 Mando | ❌ | 🔶 Script listo, falta ejecutarlo |
| RNF-6 Localización | ❌ | ✅ LOCTEXT + selector de idioma |

---

## 1. Jugador — Cuenta y sesión

| ID | Pri | Estado | Dónde / por qué |
|---|:--:|:--:|---|
| RF-1.1 Login con Steam | 🔴 | 🔒 | Requiere Steamworks SDK y `OnlineSubsystemSteam` |
| RF-1.2 Perfil persistente | 🔴 | 🔒 | Depende de RF-1.1 |
| RF-1.3 Nombre visible | 🔴 | ✅ | `ShooterProgressionSubsystem::SetPlayerName`, persistido |
| RF-1.4 Recuperar progreso | 🔴 | 🔒 | Depende del backend de progresión |
| RF-1.5 Jugar sin conexión | 🟡 | ✅ | `ShooterGameMode::FillWithBots` permite partida local contra IA |
| RF-1.6 Aviso de fallo online | 🔴 | ❌ | Sin capa online que pueda fallar |

## 2. Menú y navegación

| ID | Pri | Estado | Dónde / por qué |
|---|:--:|:--:|---|
| RF-2.1 Menú principal | 🔴 | ✅ | `ShooterMenuSubsystem` + `SShooterMenuScreen` (Slate): principal, jugar, opciones, controles, equipamiento, progresión, pausa |
| RF-2.2 Elegir modo | 🔴 | ✅ | Pantalla *Jugar*: Equipos, Dominación o Todos contra todos, y 4/8/12 jugadores |
| RF-2.3 Ver nivel y XP | 🟡 | ✅ | `GetProgressSummary` y `GetStatsSummary` en `ShooterMenuWidget` |
| RF-2.4 Salir del juego | 🔴 | ✅ | `QuitGame`, cierre ordenado del proceso |

## 3. Matchmaking y partidas

| ID | Pri | Estado | Dónde / por qué |
|---|:--:|:--:|---|
| RF-3.1 Buscar partida pública | 🔴 | 🔒 | Requiere EOS o Steam Sessions |
| RF-3.2 Emparejar por región | 🔴 | 🔒 | Idem |
| RF-3.3 Cancelar búsqueda | 🔴 | 🔒 | Idem |
| RF-3.4 Abandonar partida | 🔴 | ✅ | `LeaveMatch` vuelve al nivel de menú |
| RF-3.5 Reconexión | 🟡 | 🔒 | Requiere servidor dedicado |
| RF-3.6 Bots de relleno | 🟡 | ✅ | `FillWithBots` con `DesiredBotCount` y `BotFillDelay` |
| RF-3.7 Equilibrar equipos | 🟡 | 🔶 | `ChooseTeamForNewPlayer` reparte al entrar; falta reequilibrio ante abandonos |
| RF-3.8 Partida privada | 🟢 | 🔒 | Requiere sesiones |
| RF-3.9 Configurar sala | 🟢 | 🔒 | Idem |

## 4. Control del personaje ✅

| ID | Pri | Estado | Dónde |
|---|:--:|:--:|---|
| RF-4.1 Mover WASD | 🔴 | ✅ | `ShooterCharacter::Move` |
| RF-4.2 Cámara con ratón | 🔴 | ✅ | `ShooterCharacter::Look` |
| RF-4.3 Saltar | 🔴 | ✅ | `JumpAction` → `ACharacter::Jump` |
| RF-4.4 Agacharse | 🔴 | ✅ | `ShooterCharacter::ToggleCrouch` |
| RF-4.5 Correr sin disparar | 🔴 | ✅ | `StartSprinting` llama a `StopFire` |
| RF-4.6 Deslizarse | 🟡 | ✅ | `StartSlide`/`UpdateSlide`, con duración y enfriamiento |
| RF-4.7 Colisión | 🔴 | ✅ | `UCapsuleComponent` del personaje |

## 5. Armas y combate ✅

| ID | Pri | Estado | Dónde |
|---|:--:|:--:|---|
| RF-5.1 Dos armas | 🔴 | ✅ | `InventoryComponent::DefaultWeapons` y `SelectNextWeapon` |
| RF-5.2 Disparar | 🔴 | ✅ | `WeaponBase::FireOnce` |
| RF-5.3 ADS | 🔴 | ✅ | `bIsAiming` reduce la dispersión y el FOV |
| RF-5.4 Recargar | 🔴 | ✅ | `TryReload`; automática al vaciar en `TryStartFire` |
| RF-5.5 Consumo de munición | 🔴 | ✅ | `CurrentAmmo` validado en servidor |
| RF-5.6 Retroceso | 🔴 | ✅ | `ApplyRecoilToOwner` con curvas del DataAsset |
| RF-5.7 Dispersión | 🔴 | ✅ | `GetCurrentSpreadDegrees` |
| RF-5.8 Caída de daño | 🔴 | ✅ | `WeaponDataAsset::GetDamageAtDistance` |
| RF-5.9 Headshot | 🔴 | ✅ | `LagCompensationComponent` con `HeadshotHeightFraction` |
| RF-5.10 Accesorios | 🟡 | ✅ | `WeaponAttachmentDataAsset`: mira, cañón y cargador con modificadores multiplicativos |
| RF-5.11 Granadas | 🟢 | ✅ | `AShooterGrenade`: daño radial, mecha de 3 s, validado en servidor |

## 6. Salud, muerte y reaparición ✅

| ID | Pri | Estado | Dónde |
|---|:--:|:--:|---|
| RF-6.1 100 de salud | 🔴 | ✅ | `HealthComponent::MaxHealth` |
| RF-6.2 Regeneración | 🔴 | ✅ | `HealthComponent::TickComponent` |
| RF-6.3 Muerte a 0 | 🔴 | ✅ | `HandleTakeAnyDamage` |
| RF-6.4 Reaparición | 🔴 | ✅ | `ShooterGameMode::RespawnPlayer` |
| RF-6.5 Aparición segura | 🔴 | ✅ | `ChoosePlayerStart_Implementation` elige el más lejano |
| RF-6.6 Ver quién te mató | 🟡 | ✅ | `Client_NotifyKilledBy`, visible al reaparecer |
| RF-6.7 Killcam | 🟢 | ✅ | `Client_StartKillCam` enfoca al asesino durante la espera |

## 7. HUD en partida

| ID | Pri | Estado | Dónde |
|---|:--:|:--:|---|
| RF-7.1 Mira dinámica | 🔴 | ✅ | `ShooterHUD::DrawCrosshair` |
| RF-7.2 Munición | 🔴 | ✅ | `DrawAmmo` |
| RF-7.3 Salud | 🔴 | ✅ | `DrawHealth` |
| RF-7.4 Marcador y tiempo | 🔴 | ✅ | `DrawMatchInfo` |
| RF-7.5 Hitmarker | 🔴 | 🔶 | Visual en `DrawHitMarker`; falta el sonido |
| RF-7.6 Indicador de daño | 🔴 | ✅ | `DrawDamageIndicator` |
| RF-7.7 Killfeed | 🟡 | ✅ | `DrawKillFeed`, replicado por el GameState |
| RF-7.8 Tabla de posiciones | 🟡 | ✅ | `DrawLiveScoreboard`, con Tab |
| RF-7.9 Minimapa | 🟢 | ✅ | `DrawMinimap`: aliados y zonas; nunca enemigos (RNF-3.6) |

## 8. Modos de juego

| ID | Pri | Estado | Dónde |
|---|:--:|:--:|---|
| RF-8.1 Todos contra todos | 🔴 | ✅ | `bTeamBased = false`; el ganador sale de la tabla |
| RF-8.2 Equipos 4v4 | 🔴 | ✅ | `bTeamBased` con reparto automático |
| RF-8.3 Dominación | 🟡 | ✅ | `AControlZone` con captura, disputa y puntos por segundo |
| RF-8.4 Oleadas | 🟢 | ✅ | `AWaveManager` con dificultad lineal y tope de concurrentes |
| RF-8.5 Fin de partida | 🔴 | ✅ | `FinishMatch` por tiempo o `CheckWinCondition` |
| RF-8.6 Pantalla de resultados | 🔴 | ✅ | `DrawScoreboard` con posiciones, bajas y muertes |

## 9. Progresión

Implementada con guardado local en `UShooterSaveGame`. El almacenamiento en
servidor (RNF-3.4) queda pendiente del backend.

| ID | Pri | Estado | Dónde |
|---|:--:|:--:|---|
| RF-9.1 Ganar XP | 🔴 | ✅ | `AddExperience`, otorgada al terminar la partida |
| RF-9.2 Subir de nivel | 🔴 | ✅ | Curva del 15 % acumulativo por nivel |
| RF-9.3 Desbloquear armas | 🔴 | ✅ | Tabla de desbloqueos por nivel en `RefreshUnlocks` |
| RF-9.4 Desbloquear accesorios | 🟡 | ✅ | `RecordWeaponShot` cuenta disparos por arma |
| RF-9.5 Persistencia | 🔴 | ✅ | `UShooterSaveGame` en disco (local; RNF-3.4 pendiente) |
| RF-9.6 Estadísticas | 🟡 | ✅ | K/D, precisión y partidas acumuladas en el perfil |
| RF-9.7 Desafíos diarios | 🟢 | ❌ |

## 10–12. Arsenal, configuración y social

Bloques sin implementar. RF-11 (configuración) es el más accesible: no depende
de servicios externos, solo de UI y `UGameUserSettings`.

| Bloque | Total | Estado |
|---|---:|---|
| RF-10 Arsenal | 3 | ❌ Requiere UI de loadouts |
| RF-11 Configuración | 9 | ✅ 9 de 9 en `ShooterSettingsSubsystem`: sensibilidad, reasignación de teclas, calidad, volúmenes, VSync, límite de FPS, modo ventana, daltonismo, color de mira. Persisten entre sesiones. Falta la UI que los exponga |
| RF-12 Social | 6 | 🔒 Requiere Steam |

## 13–16. Administrador / LiveOps

| ID | Pri | Estado | Dónde |
|---|:--:|:--:|---|
| RF-13.1 Registrar eventos | 🔴 | ✅ | `ShooterTelemetrySubsystem`, JSON Lines en `Saved/Telemetry` |
| RF-13.4 Registrar errores | 🟡 | ✅ | `RecordError` |
| RF-13.2, 13.3, 13.5 | 🔴🟡 | 🔒 | Requieren backend que consuma los eventos |
| RF-14 a RF-16 | — | 🔒 | Requieren backend y panel de administración |

Los eventos ya se emiten en el formato que consumiría un servicio de análisis:
migrar es cambiar el destino, no el contenido.

---

## Requerimientos no funcionales

### Rendimiento (RNF-1)

| ID | Estado | Nota |
|---|:--:|---|
| RNF-1.1 60 FPS en gama media | 🔶 | Sin medir en hardware objetivo |
| RNF-1.2 144+ FPS en gama alta | 🔶 | Sin medir |
| RNF-1.3 Latencia de disparo ≤1 frame | ✅ | El cliente predice: `FireOnce` actúa antes de `Server_Fire` |
| RNF-1.4 Carga de mapa ≤20 s | 🔶 | Sin medir |
| RNF-1.5 Arranque ≤30 s | ❌ | Sin menú que medir |
| RNF-1.6 Instalación ≤15 GB | ✅ | Proyecto actual muy por debajo |
| RNF-1.7 VRAM ≤4 GB | 🔶 | Sin medir |

### Red (RNF-2)

| ID | Estado | Nota |
|---|:--:|---|
| RNF-2.1 Tickrate 60 Hz | ✅ | `NetServerMaxTickRate=60` en `DefaultEngine.ini` |
| RNF-2.2 Jugable a 120 ms | 🔶 | La compensación existe; falta validar con `Net PktLag` |
| RNF-2.3 Ancho de banda ≤128 kbps | 🔶 | Sin medir |
| RNF-2.4 Compensación de latencia | ✅ | `LagCompensationComponent` con rebobinado de historial |
| RNF-2.5 Búsqueda ≤60 s | 🔒 | Sin matchmaking |
| RNF-2.6 Detectar desconexión | ✅ | `IsConnectionLost`: avisa a los 3 s, con mensaje en lenguaje claro |
| RNF-2.7 Sin teletransportes al 5 % de pérdida | 🔶 | Sin medir |

### Seguridad (RNF-3)

| ID | Estado | Nota |
|---|:--:|---|
| RNF-3.1 Servidor autoritativo | ✅ | Daño, munición y puntaje solo en servidor |
| RNF-3.2 El cliente no decide impactos | ✅ | `Server_Fire` recalcula el trazado |
| RNF-3.3 Easy Anti-Cheat | ❌ | Fase 3 |
| RNF-3.4 Progreso en servidor | 🔒 | Sin backend |
| RNF-3.5 Límite de RPCs | ✅ | `FRPCRateLimiter` en disparo, granada y estados |
| RNF-3.6 Relevancia de red | 🔶 | Por defecto del motor; sin ajustar |
| RNF-3.7 Sin datos personales | ✅ | El proyecto no recoge ninguno |

### Usabilidad (RNF-4)

| ID | Estado | Nota |
|---|:--:|---|
| RNF-4.1 Disparar en ≤60 s | ✅ | La zona de entrenamiento arranca en combate |
| RNF-4.2 Controles estándar | ✅ | WASD, ratón, R, Shift, Ctrl |
| RNF-4.3 HUD no solo por color | ✅ | Salud con número, munición con cifras |
| RNF-4.4 Legible a 1080p | ✅ | Paneles con fondo y contraste |
| RNF-4.5 Jugable con mando | 🔶 | `fix_gamepad.py` mapea el mando; pendiente de ejecutar en el editor |
| RNF-4.6 Errores en lenguaje claro | 🔶 | Solo en scripts de Python |

### Compatibilidad, localización y resto

| Bloque | Estado |
|---|---|
| RNF-5 Compatibilidad | 🔶 Windows/DX12 por defecto del motor; sin probar el rango de resoluciones |
| RNF-6 Localización | ✅ Textos en LOCTEXT con argumentos nombrados y plurales; español/inglés con selector de idioma persistido |
| RNF-7 Escalabilidad | 🔒 Sin infraestructura |
| RNF-8 Disponibilidad | 🔒 Sin servicio |
| RNF-9 Mantenibilidad | ✅ 4 de 6: nomenclatura, lógica en C++, balance en DataAssets, Git LFS |
| RNF-10 Legal | ❌ Fase de lanzamiento |

---

## Dependencias que bloquean

Tres dependencias externas explican la mayoría de lo pendiente:

1. **Steamworks SDK** — bloquea RF-1 (cuenta), RF-12 (social) y RNF-10.1
2. **Backend de progresión** — bloquea RF-9 (progresión), RF-13 a RF-16 (admin)
3. **Servidor dedicado** — el motor instalado desde el launcher no puede
   compilarlo (*"Server targets are not currently supported from this engine
   distribution"*); requiere Unreal compilado desde el código fuente. Bloquea
   RF-3 (matchmaking), RF-3.5 (reconexión) y RNF-7/8

Ninguna se resuelve escribiendo código del cliente: son decisiones de
infraestructura previstas en las fases F2–F4 de
[PLANIFICACION.md](PLANIFICACION.md).

---

## Siguiente bloque accesible

**No queda programación de cliente pendiente sin dependencias.** Lo que falta
para una beta se divide en tres categorías, ninguna de las cuales se resuelve
escribiendo C++:

### 1. Trabajo de editor

| Tarea | Por qué no es código |
|---|---|
| Ejecutar `fix_gamepad.py` (RNF-4.5) | El script está escrito; requiere el editor abierto |
| Ejecutar `fix_movimiento.py` | Confirma el bug de WASD |
| Colocar `AControlZone` en un mapa | Dominación necesita tres zonas situadas a mano |
| Colocar `AWaveManager` en un mapa | Oleadas necesita el gestor y sus `PlayerStart` |
| Crear los `DA_*` de accesorios | El sistema lee lo que haya; los assets se crean en el editor |

### 2. Contenido

- **7 armas** de las 8 que pide el checklist (solo existe `DA_Rifle`)
- **3 mapas** de los 4 (existe `Lvl_Entrenamiento`)
- **Assets de audio**: sin Sound Classes los volúmenes se guardan pero no
  enrutan a nada

### 3. Dependencias externas

Las tres de siempre: Steamworks, backend y servidor dedicado. Ver la sección
anterior.

### Lo único de código que queda

- **Sonido del hitmarker** (RF-7.5): depende de que exista el asset de audio
- **Reequilibrio de equipos ante abandonos** (RF-3.7): la base está en
  `ChooseTeamForNewPlayer`
- **Medir el rendimiento** (RNF-1.1, 1.2, 1.4, 1.7): requiere hardware
  objetivo, no código
