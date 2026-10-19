# Prompt para continuar el proyecto en otra sesión

Copia todo lo que hay entre las líneas y pégalo como primer mensaje.

---

## PROMPT

Estoy desarrollando **Shoter**, un **FPS táctico realista multijugador** en
Unreal Engine 5.8 con C++. El proyecto está en
`C:\Users\felip\OneDrive\Documentos\VS\Shoter` y en
https://github.com/FelipeVeraMeza/Shoter

Empezó como FPS arcade y en octubre de 2026 lo convertimos en shooter
realista. La meta visual es un shooter urbano tipo AAA: calles de piedra y
ladrillo, ruinas, escombros, brazos con guantes sosteniendo un fusil con mira
holográfica, fuego, humo, luz dorada con bruma. La jugabilidad realista, la
primera persona con brazos, los efectos, los bots, los menús y el nivel
`Lvl_Distrito` están hechos con lo que trae el proyecto; lo que falta es
**probarlo jugando** y **arte de Fab** (texturas, kits, sonidos, Niagara).

### Lee primero, en este orden

1. `docs/REALISMO.md` — **el documento principal**: balística, heridas,
   movimiento, armas, HUD realista, el mapa Distrito y cómo regenerarlo
2. `docs/BITACORA.md` — decisiones y errores ya resueltos. **Léelo antes de
   escribir scripts de Python para el editor**: ahí están las trampas de la API
3. `docs/SISTEMAS.md` — los subsistemas anteriores (progresión, ajustes,
   menú, Dominación, Oleadas, accesorios...)
4. `docs/GDD.md` — diseño; las secciones de movimiento, salud, TTK y arte
   están marcadas como revisadas para el giro realista

### Estado actual

Todo compila en UE 5.8 sin errores ni avisos. **En la última sesión (fase 7)
no se abrió el editor**: `build_realistic_map.py` se reescribió y no se ha
ejecutado, y **nadie ha jugado una partida** con los cambios. Lo primero es
ejecutar el script, leer el log (`[distrito] AVISO`) y jugar.

**Sistemas realistas (C++, `Source/Shoter`):**

| Sistema | Clase |
|---|---|
| Balas simuladas: caída, tiempo de vuelo, penetración, supresión | `UShooterBallisticsSubsystem` |
| Resistencia balística por superficie | `UShooterPhysicalMaterial` |
| Zonas de impacto (cabeza como esfera, torso, abdomen, brazos, piernas) | `ULagCompensationComponent` |
| Sin regeneración; hemorragia, cojera, vendas, daño por caída | `UHealthComponent` |
| Inercia, resistencia, inclinación Q/E, balanceo de mira, respiración, retroceso con recuperación | `AShooterCharacter` |
| Cargadores reales, bala en recámara, selector de fuego, puesta a cero | `AWeaponBase` |
| HUD realista (sin mira ni munición exacta), activo por defecto | `AShooterHUD` + `UShooterSettingsSubsystem::SetRealisticHUD` |
| Viñeta y desaturación por supresión y heridas | `AShooterCharacter::UpdateCameraEffects` |
| Brazos en primera persona: el arma se fija a la cámara y los brazos la siguen | `UShooterFirstPersonComponent` |
| Malla de arma doble (mundo / primera persona), visor holográfico y de aumentos deducidos de la malla | `AWeaponBase::BuildViewFrame`, `LayoutSight` |
| Fogonazo, trazadoras, casquillos, impactos, explosión, fuego con humo (sin Niagara) | `Source/Shoter/Effects/` |
| Bots con PlayerState, línea de visión, reacción, ráfagas, reaparición | `AShooterBotController`, `AShooterGameMode::OnBotKilled` |
| Equipamiento (arma principal), validado en servidor | `PrimaryWeaponOptions`, `Server_SelectPrimaryWeapon` |
| Menús completos en Slate, sin Widget Blueprints | `SShooterMenuScreen`, `AShooterMenuGameMode` |

**Contenido generado por `Content/Python/build_realistic_map.py`:**

- `Lvl_Distrito`: barrio de 160 × 120 m, tres carriles (callejón norte,
  avenida con plaza, patio industrial con nave y pasarela), edificios con
  interiores y detalles, ruina, autobús calcinado, fuegos con humo
  (`AShooterFireSource`), cráteres, sacos terreros, tendido eléctrico, zonas
  de Dominación A/B/C, 12 apariciones etiquetadas por equipo, navegación,
  atardecer dorado con rayos de luz y bruma
- `Lvl_Menu`: decorado con cámara; su GameMode es `AShooterMenuGameMode`.
  Es el mapa de arranque del juego; el editor arranca en `Lvl_Distrito`
- `BP_ModoRealista`: equipos, 15 min, rellena con bots hasta 8 jugadores,
  reinicia la partida 25 s después de terminar. El menú cambia modo y
  jugadores por la URL (`?modo=dominacion?jugadores=12`)
- Armas: `DA_Rifle` (5,56, holográfico), `DA_Pistola` (9 mm) y `DA_Tirador`
  (7,62, visor de aumentos) y sus `BP_`, con animaciones de brazos y cuerpo
  de la plantilla y el único sonido de disparo del proyecto con distinto tono.
  El jugador lleva fusil + pistola y puede cambiar a tirador con `L`
- Materiales de efectos en `Content/Realista/Efectos` (`M_Fogonazo`,
  `M_Trazadora`, `M_Reticula`, `M_Impacto`, `M_Polvo`, `M_Fuego`): el C++
  los carga por ruta
- Materiales: `M_Superficie` (procedural, ruido en coordenadas de mundo, sin
  texturas) y 14 instancias en `Content/Realista/Materiales`; 8 materiales
  físicos en `Content/Realista/Fisica`. **Si existen materiales en
  `Content/Fab`, `Content/Megascans` o `Content/Quixel`, el script los usa
  en su lugar** buscando por palabra clave
- `IMC_Combate` reconstruido entero, y es el **único** contexto de
  `BP_Jugador`

El script es idempotente: borra y recrea solo los actores con la etiqueta
`ShoterGenerado`.

### Controles

WASD mover · ratón mirar · clic izq. disparar · clic der. apuntar ·
`Shift` correr (apuntando: contener la respiración) · `Ctrl`/`C` agacharse ·
`Espacio` saltar · `R` recargar/comprobar cargador · `Q`/`E` asomarse ·
`B` selector de fuego · `H` vendarse · `G` granada · rueda/`1` cambiar arma ·
`P`/`Esc` pausa · `Tab` tabla de posiciones · `L` equipamiento ·
`F1` lista de controles. Mando también mapeado.

### Lo que sigue, por prioridad

**Puede hacerse ahora mismo:**

1. **Ejecutar `build_realistic_map.py`** (con el binario recién compilado) y
   corregir cada `[distrito] AVISO` del log
2. **Jugar una partida** con bots en `Lvl_Distrito` y ajustar. Lo más
   probable que haya que retocar (ver "Sin probar" en la bitácora, fase 7):
   - Posición del arma a la cadera (`HipSightOffset`) y del visor
     holográfico sobre el fusil de la plantilla
   - Que `AWeaponBase::BuildViewFrame` deduzca bien los ejes de `SKM_Rifle`
     y `SKM_Pistol` (si el arma sale girada, fijar `SightLocation`)
   - Intensidades de fogonazo, fuego y trazadoras frente a la exposición
   - Dificultad de los bots (`AimSpeed`, `AimErrorPer10m`, `ReactionTime`)
3. Probar el menú arrancando en *Standalone* desde `Lvl_Menu`

**Necesita que yo descargue assets de Fab con mi cuenta de Epic:**

- Materiales Megascans (gratis con Unreal): adoquines, piedra, ladrillo,
  escombros, metal oxidado
- Un kit modular de edificios o ruinas
- Un pack de armas y otro de efectos Niagara (fuego, humo, fogonazos). El
  código usa un Niagara si se asigna en el DataAsset o en el material
  físico; si no, los efectos propios
- Sonidos de armas (hoy las tres usan el disparo de la plantilla con
  distinto tono), de impactos y de recarga
- Personajes con equipo militar y animaciones de primera persona propias

Cuando estén importados, hay que integrarlos: el script ya recoge los
materiales; las mallas y efectos hay que colocarlos o conectarlos. Ojo con
el tamaño: cada material 4K suma 50–150 MB; a 2K, la cuarta parte.

### Tres dependencias externas siguen bloqueando lo de siempre

No se resuelven escribiendo código; no las propongas como tareas:

1. **Steamworks SDK** → cuentas y social
2. **Backend de progresión**
3. **Servidor dedicado**: el UE del launcher no compila targets de servidor
   (*"Server targets are not currently supported from this engine
   distribution"*); requiere Unreal desde el código fuente

### Cómo trabajar en este proyecto

**Compilar** (el editor debe estar CERRADO; Live Coding lo bloquea):

```powershell
& "D:\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
    ShoterEditor Win64 Development `
    -Project="C:\Users\felip\OneDrive\Documentos\VS\Shoter\Shoter.uproject" -WaitMutex
```

**Ejecutar un script de Python sin abrir el editor a mano** (se cierra solo
al terminar):

```powershell
& "D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" `
    "C:\Users\felip\OneDrive\Documentos\VS\Shoter\Shoter.uproject" `
    -ExecutePythonScript="<ruta del script>" -nosplash -unattended
```

Para que el editor **siga abierto** después (por ejemplo, para sacar
capturas con `unreal.AutomationLibrary.take_high_res_screenshot` desde un
`register_slate_post_tick_callback`), usa `"-ExecCmds=py <ruta>"` en su
lugar: `-ExecutePythonScript` cierra el editor al terminar.

**Avísame antes de abrir el editor por script.** La última vez lo abriste
para sacar capturas, pulsé Play sin saberlo y el script me movió la cámara y
me cerró el juego.

**Registro:** `Saved/Logs/Shoter.log`. Los scripts escriben con prefijo:
`[distrito]`, `[entrenamiento]`, `[camara]`, `[gamepad]`... Los "Error" de
`LogAutomationTest` y `UnifiedErrorTest` al arrancar son autopruebas del
motor, no fallos nuestros.

### Trampas que ya costaron tiempo

1. **La API de Python cambia entre versiones.** No adivines: ejecuta
   `Content/Python/diagnostico.py` (vuelca la API a
   `Saved/diagnostico_api.txt`). En 5.8: `InputAction_Factory` lleva guion
   bajo, `unreal.Key()` no acepta argumentos (se fija `key_name`),
   `default_key_mappings` es una estructura que contiene la lista,
   `generated_class` es un método, y la niebla volumétrica es
   `enable_volumetric_fog`
2. **Los subobjetos de un asset necesitan el asset como *outer*.**
   `unreal.InputModifierNegate()` vive en el paquete transitorio y se pierde
   al guardar: usa `unreal.new_object(Clase, outer=asset)`. Era la causa del
   bug de WASD
3. **Asignar al CDO de un Blueprint no basta:** hay que recompilarlo, o el
   juego usa los valores viejos sin avisar
4. **Las trazas de bala usan la colisión simple**, que no lee el material
   físico del material visual: hay que fijarlo con
   `SetPhysMaterialOverride` en el componente
5. **`UPhysicalMaterial` está en el módulo `PhysicsCore`**: sin él en
   `Shoter.Build.cs`, compila pero no enlaza
6. **El retroceso y el balanceo escriben la rotación de control directamente**
   (`AShooterCharacter::AddViewRotation`), no con `AddControllerPitchInput`,
   que multiplica por escalas heredadas (±2,5). Los valores de retroceso son
   grados reales
7. **`LeftShift` dispara un comando de depuración del motor**
   (`DebugManager.CycleToPreviousColumn`, de `BaseInput.ini`). Es inofensivo
   y no existe en Shipping
8. **La exposición automática quema las fachadas claras**: el post-proceso
   lleva un `auto_exposure_bias` negativo
9. **`AAIController` sin `bWantsPlayerState` no tiene PlayerState**: sin él
   no hay equipo, nombre ni marcador. Y con
   `bSetControlRotationFromPawnOrientation` a true (el valor por defecto)
   reescribe el cabeceo cada fotograma: el bot no puede apuntar arriba
10. **La cámara de un pawn no gira con su controlador** salvo para el
    jugador que mira a través de ella (`bUsePawnControlRotation` se aplica
    en `GetCameraView`). Para disparar, usa `GetAimRotation()`, no la cámara
11. **Cambiar la malla de un componente en modo *Single Node* reinicia la
    animación**: hay que volver a reproducirla (lo hace
    `UShooterFirstPersonComponent` al detectar el cambio)
12. **Un parámetro llamado `bHidden` en un método de un actor** oculta el
    miembro de `AActor` y el compilador lo trata como error (C4458)

### Cómo quiero que trabajes

- **Verifica antes de afirmar.** Compila y lee el log; no digas que algo
  funciona sin comprobarlo. Si no lo has probado jugando, dilo
- **Di lo que no puedes hacer.** Si algo depende de assets de Fab, de Steam
  o de un backend, dilo en lugar de rodearlo
- **Comentarios en el código:** explica el *porqué* de una decisión, no lo que
  el código ya dice. Sigue el estilo existente (español, `LOCTEXT` para todo
  texto visible, balance en DataAssets y nunca en C++)
- **Commits a mi nombre** (Felipe), **sin coautoría**, y solo cuando te lo pida
- **Respóndeme en español**

### Lo primero que quiero hacer

[ESCRIBE AQUÍ LO QUE QUIERAS ABORDAR]

---

## Fin del prompt

### Nota al cerrar esta sesión

Los cambios de la fase 7 **no están confirmados en git**: C++ nuevo y
modificado, el script del mapa reescrito, configuración y documentación. Los
assets que genera el script aparecerán cuando se ejecute. Revísalos con
`git status` y confírmalos cuando lo pidas.
