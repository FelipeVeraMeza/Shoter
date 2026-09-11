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
| 🔴 Must | 58 | 38 | 1 | 19 |
| 🟡 Should | 30 | 8 | 1 | 21 |
| 🟢 Could | 12 | 0 | 0 | 12 |
| **RF total** | **100** | **46** | **2** | **52** |
| **RNF total** | **52** | **13** | **6** | **33** |

El núcleo de combate está completo. Lo que falta se concentra en: cuenta y
sesión, menús, matchmaking, progresión, configuración y herramientas de
administración.

---

## 1. Jugador — Cuenta y sesión

| ID | Pri | Estado | Dónde / por qué |
|---|:--:|:--:|---|
| RF-1.1 Login con Steam | 🔴 | 🔒 | Requiere Steamworks SDK y `OnlineSubsystemSteam` |
| RF-1.2 Perfil persistente | 🔴 | 🔒 | Depende de RF-1.1 |
| RF-1.3 Nombre visible | 🔴 | ✅ | `ShooterProgressionSubsystem::SetPlayerName`, persistido |
| RF-1.4 Recuperar progreso | 🔴 | 🔒 | Depende del backend de progresión |
| RF-1.5 Jugar sin conexión | 🟡 | ❌ | Sin bots todavía |
| RF-1.6 Aviso de fallo online | 🔴 | ❌ | Sin capa online que pueda fallar |

## 2. Menú y navegación

| ID | Pri | Estado | Dónde / por qué |
|---|:--:|:--:|---|
| RF-2.1 Menú principal | 🔴 | ❌ | Sin UMG todavía; el HUD es Canvas |
| RF-2.2 Elegir modo | 🔴 | ❌ | Depende de RF-2.1 |
| RF-2.3 Ver nivel y XP | 🟡 | ❌ | Depende de progresión (RF-9) |
| RF-2.4 Salir del juego | 🔴 | ❌ | Depende de RF-2.1 |

## 3. Matchmaking y partidas

| ID | Pri | Estado | Dónde / por qué |
|---|:--:|:--:|---|
| RF-3.1 Buscar partida pública | 🔴 | 🔒 | Requiere EOS o Steam Sessions |
| RF-3.2 Emparejar por región | 🔴 | 🔒 | Idem |
| RF-3.3 Cancelar búsqueda | 🔴 | 🔒 | Idem |
| RF-3.4 Abandonar partida | 🔴 | ❌ | Depende del menú |
| RF-3.5 Reconexión | 🟡 | 🔒 | Requiere servidor dedicado |
| RF-3.6 Bots de relleno | 🟡 | ❌ | Sin IA todavía |
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
| RF-4.6 Deslizarse | 🟡 | ❌ | No implementado |
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
| RF-5.10 Accesorios | 🟡 | ❌ | No implementado |
| RF-5.11 Granadas | 🟢 | ❌ | No implementado |

## 6. Salud, muerte y reaparición ✅

| ID | Pri | Estado | Dónde |
|---|:--:|:--:|---|
| RF-6.1 100 de salud | 🔴 | ✅ | `HealthComponent::MaxHealth` |
| RF-6.2 Regeneración | 🔴 | ✅ | `HealthComponent::TickComponent` |
| RF-6.3 Muerte a 0 | 🔴 | ✅ | `HandleTakeAnyDamage` |
| RF-6.4 Reaparición | 🔴 | ✅ | `ShooterGameMode::RespawnPlayer` |
| RF-6.5 Aparición segura | 🔴 | ✅ | `ChoosePlayerStart_Implementation` elige el más lejano |
| RF-6.6 Ver quién te mató | 🟡 | ✅ | `Client_NotifyKilledBy`, visible al reaparecer |
| RF-6.7 Killcam | 🟢 | ❌ | No implementado |

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
| RF-7.9 Minimapa | 🟢 | ❌ | No implementado |

## 8. Modos de juego

| ID | Pri | Estado | Dónde |
|---|:--:|:--:|---|
| RF-8.1 Todos contra todos | 🔴 | ✅ | `bTeamBased = false`; el ganador sale de la tabla |
| RF-8.2 Equipos 4v4 | 🔴 | ✅ | `bTeamBased` con reparto automático |
| RF-8.3 Dominación | 🟡 | ❌ | No implementado |
| RF-8.4 Oleadas | 🟢 | ❌ | Requiere IA |
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
| RF-9.4 Desbloquear accesorios | 🟡 | ❌ | Sin sistema de accesorios |
| RF-9.5 Persistencia | 🔴 | ✅ | `UShooterSaveGame` en disco (local; RNF-3.4 pendiente) |
| RF-9.6 Estadísticas | 🟡 | ✅ | K/D, precisión y partidas acumuladas en el perfil |
| RF-9.7 Desafíos diarios | 🟢 | ❌ |

## 10–12. Arsenal, configuración y social

Bloques sin implementar. RF-11 (configuración) es el más accesible: no depende
de servicios externos, solo de UI y `UGameUserSettings`.

| Bloque | Total | Estado |
|---|---:|---|
| RF-10 Arsenal | 3 | ❌ Requiere UI de loadouts |
| RF-11 Configuración | 9 | ✅ 8 de 9 en `ShooterSettingsSubsystem`: sensibilidad, calidad, volúmenes, VSync, límite de FPS, modo ventana, daltonismo, color de mira. Persisten entre sesiones. Falta la UI que los exponga |
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
| RNF-2.6 Detectar desconexión | ❌ | No implementado |
| RNF-2.7 Sin teletransportes al 5 % de pérdida | 🔶 | Sin medir |

### Seguridad (RNF-3)

| ID | Estado | Nota |
|---|:--:|---|
| RNF-3.1 Servidor autoritativo | ✅ | Daño, munición y puntaje solo en servidor |
| RNF-3.2 El cliente no decide impactos | ✅ | `Server_Fire` recalcula el trazado |
| RNF-3.3 Easy Anti-Cheat | ❌ | Fase 3 |
| RNF-3.4 Progreso en servidor | 🔒 | Sin backend |
| RNF-3.5 Límite de RPCs | ❌ | No implementado |
| RNF-3.6 Relevancia de red | 🔶 | Por defecto del motor; sin ajustar |
| RNF-3.7 Sin datos personales | ✅ | El proyecto no recoge ninguno |

### Usabilidad (RNF-4)

| ID | Estado | Nota |
|---|:--:|---|
| RNF-4.1 Disparar en ≤60 s | ✅ | La zona de entrenamiento arranca en combate |
| RNF-4.2 Controles estándar | ✅ | WASD, ratón, R, Shift, Ctrl |
| RNF-4.3 HUD no solo por color | ✅ | Salud con número, munición con cifras |
| RNF-4.4 Legible a 1080p | ✅ | Paneles con fondo y contraste |
| RNF-4.5 Jugable con mando | ❌ | Sin mapeos de gamepad |
| RNF-4.6 Errores en lenguaje claro | 🔶 | Solo en scripts de Python |

### Compatibilidad, localización y resto

| Bloque | Estado |
|---|---|
| RNF-5 Compatibilidad | 🔶 Windows/DX12 por defecto del motor; sin probar el rango de resoluciones |
| RNF-6 Localización | ❌ Textos del HUD en código, no en tablas (incumple RNF-6.1) |
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

Por orden de coste y dependencias:

1. **RF-8.6 pantalla de resultados** y **RF-8.1 todos contra todos** — cierran
   el bloque de modos, sin dependencias
2. **RF-7.7 killfeed** y **RF-7.8 tabla de posiciones** — los datos ya existen
   en `ShooterPlayerState`
3. **RF-11 configuración** — `UGameUserSettings` cubre gráficos, y las teclas
   ya son reasignables; falta la UI
4. **RF-9 progresión local** — `USaveGame` permite cumplir RF-9.1 a RF-9.5 sin
   backend, aunque RNF-3.4 quede pendiente
