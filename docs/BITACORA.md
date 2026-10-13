# 📓 Bitácora de desarrollo

Registro de la sesión que fusionó los dos proyectos y construyó los sistemas
que hoy tiene `Shoter`. Deja constancia de las decisiones y, sobre todo, de los
errores: varios volverían a aparecer sin este registro.

---

## Punto de partida

Había dos proyectos incompatibles entre sí:

| | `Shootergame-main` | `Shoter` |
|---|---|---|
| Código C++ | 28 archivos: combate multijugador completo | Ninguno |
| Contenido | Ninguno | Plantilla First Person completa |
| Compila | No (sin binarios del módulo) | Sí |
| Motor | UE 5.3 | UE 5.8 (GUID local) |

El código bueno estaba en un proyecto vacío; el contenido bueno, en un proyecto
sin código. La decisión fue **portar el C++ a `Shoter`**, que ya tenía mallas,
animaciones, armas y mapas.

---

## Fase 1 — Port a UE 5.8

### Cuatro incompatibilidades encontradas

| Problema | Causa | Solución |
|---|---|---|
| No compilaba: *"no se encontró el archivo incluir"* | `Build.cs` sin `PublicIncludePaths` | Añadir `ModuleDirectory` |
| Input muerto | `BindAxis`/`BindAction` es del sistema legacy | Reescribir con Enhanced Input |
| `TArray::RemoveAt` roto | El `bool` final se sustituyó por `EAllowShrinking` en 5.5+ | Actualizar la llamada |
| Redirects inválidos | `DefaultEngine.ini` apuntaba a `/Script/Shoter` | Renombrar el módulo a `Shoter` |

### El módulo duplicado

Tras el primer push, el repositorio tenía **dos módulos C++**: el portado y el
viejo, que el rebase había conservado. Con ambos presentes, UnrealBuildTool
intenta compilar también el antiguo —no adaptado a 5.8— y falla.

Se detectó revisando el resultado del push. Sin esa revisión, el fallo habría
aparecido al clonar el repositorio en otra máquina.

### El GUID del motor

El `.uproject` guardaba el GUID de la instalación local:

```json
"EngineAssociation": "{882737CC-496B-DACD-C002-39A6278D221D}"
```

Ese identificador solo existe en el registro de Windows de una máquina. Cuando
la clave se borró, Unreal dejó de reconocer el proyecto y ofreció abrir una
copia — creando `Shoter 5.8`, un duplicado de 2.9 GB desconectado del
repositorio.

**Solución:** usar la versión (`"5.8"`), que el launcher resuelve en cualquier
equipo. Con el GUID, cualquiera que clonara el repositorio habría tenido el
mismo problema.

---

## Fase 2 — Zona de entrenamiento

Nivel de pruebas generado por script: suelo de 60×60 m, muros, coberturas,
cuatro puntos de aparición y doce blancos a tres distancias.

Detalle en [ZONA_ENTRENAMIENTO.md](ZONA_ENTRENAMIENTO.md).

### La API de Python cambia entre versiones

Aquí se perdió más tiempo del necesario. El script falló cuatro veces seguidas,
cada una por un nombre distinto:

| Intento | Error | Realidad en 5.8 |
|---|---|---|
| 1 | `module 'unreal' has no attribute 'InputCoreKeys'` | No existe; las teclas se construyen con `unreal.Key()` |
| 2 | `no attribute 'InputActionFactory'` | Se llama `InputAction_Factory`, con guion bajo |
| 3 | `Failed to find property 'generated_class'` | Ya no es propiedad del editor: hay que llamar al método |
| 4 | `Failed to convert 'list' to 'DefaultKeyMappings'` | Es una **estructura que contiene** la lista, no la lista |

**Lección:** sin el stub de Python generado no hay forma de verificar los
nombres desde fuera del editor, y adivinarlos lleva a fallar uno a uno. La
salida fue escribir `diagnostico.py`, que vuelca a un archivo qué expone
realmente la versión instalada. Debió ser el primer paso, no el quinto.

### Valores del CDO sin recompilar

Un fallo silencioso: asignar valores al objeto por defecto de un Blueprint
**no basta**. Hasta que el Blueprint no se recompila, el juego sigue usando los
valores anteriores.

Se manifestó como un cronómetro corriendo pese a `endless_match = true`, y como
un nivel que usaba `ShooterCharacter` en vez de `BP_Jugador`. Lo resuelve
`fix_blueprints.py`, que asigna **y** recompila.

### Geometría estática

Los bloques se creaban con `ComponentMobility.STATIC`, lo que obliga a
construir la iluminación antes de que el nivel se vea correctamente. No daba
error: simplemente el mapa salía oscuro. Se cambió a `MOVABLE`.

---

## Fase 3 — Sistemas de juego

Siete subsistemas en C++, documentados en [SISTEMAS.md](SISTEMAS.md):

| Sistema | Requerimientos |
|---|---|
| Progresión y guardado | RF-1.3, 9.1, 9.2, 9.3, 9.5, 9.6 |
| Ajustes | RF-11.1, 11.3–11.9 |
| Telemetría | RF-13.1, 13.4 |
| Killfeed y aviso de muerte | RF-7.7, 6.6 |
| Tablas de posiciones | RF-7.8, 8.6 |
| Menú (navegación) | RF-2.1–2.4, 3.4 |

### Decisiones de diseño

**La progresión vive en el GameInstance**, no en el PlayerState: así sobrevive
a los cambios de nivel. Cuando exista el backend (RNF-3.4), esta clase pasa a
ser su caché local sin que cambie el resto del código.

**La telemetría escribe JSON Lines** en el formato que consumiría un servicio
de análisis. Migrar a backend es cambiar el destino, no el contenido.

**El menú no pausa el juego.** Un FPS multijugador no puede detenerse porque un
jugador abra las opciones.

**El balance vive en DataAssets** (RNF-9.3): ajustar un arma es editar el
asset, sin recompilar.

---

## Problemas de la puesta en marcha

Los que aparecieron al probar el juego de verdad:

| Síntoma | Causa |
|---|---|
| Pantalla negra, "Sin título" | El script falló antes de crear nada |
| No responde ninguna tecla | Los mapeos se escribían en la propiedad obsoleta `mappings` |
| El arma no se ve | `DA_Rifle` se creó sin malla asignada |
| El arma ocupa media pantalla | Anclada a 30 cm sin escalar; las mallas son de tamaño real |
| Mirar hacia arriba baja la vista | `AddControllerPitchInput` mira abajo con valores positivos; el ratón entrega Y positivo hacia arriba |
| El HUD se veía borroso | Fuentes pequeñas del motor escaladas a 2.2× |
| Memoria crítica (0.37 GB libres) | Dos editores de Unreal abiertos a la vez |

### Live Coding bloquea la compilación

Con el editor abierto, compilar desde la terminal falla con *"Unable to build
while Live Coding is active"*. Cualquier cambio en C++ exige cerrar el editor
primero.

---

## Scripts de editor

En `Content/Python`. Todos son idempotentes: reejecutarlos reutiliza lo que ya
exista.

| Script | Para qué |
|---|---|
| `diagnostico.py` | Vuelca la API real del motor a `Saved/diagnostico_api.txt`. **Ejecutar esto primero ante cualquier fallo de script** |
| `build_training_zone.py` | Crea los assets del juego y el nivel de entrenamiento |
| `build_sci_fi_range.py` | Hangar futurista con paneles luminosos |
| `build_menu.py` | Crea los seis widgets del menú y `Lvl_Menu` |
| `fix_movimiento.py` | Corrige el tipo de `IA_Move` y los modificadores de WASD |
| `fix_gamepad.py` | Mapea el mando en `IMC_Combate` (RNF-4.5) |
| `fix_blueprints.py` | Reasigna valores a los Blueprints **y los recompila** |
| `fix_camara.py` | Mapea ratón y teclado en `IMC_Combate` |
| `fix_arma.py` | Asigna malla y nombre a `DA_Rifle` |
| `fix_personaje.py` | Asigna maniquí y animaciones a `BP_Jugador` |

---

---

## Fase 4 — Bots, granadas, deslizamiento y drones

Commits `5df6c9b` y `120facc`. Cinco requerimientos más cubiertos: RF-1.5,
RF-3.6, RF-4.6, RF-5.11 y la mitad de RF-8.4.

| Sistema | Clase | Requerimientos |
|---|---|---|
| IA de combate | `AShooterBotController` | RF-3.6, RF-1.5 |
| Granadas | `AShooterGrenade` | RF-5.11 |
| Deslizamiento | `ShooterCharacter::StartSlide` | RF-4.6 |
| Drones de entrenamiento | `ATrainingDrone`, `ATrainingRangeManager` | RNF-4.1 |

### Decisiones

**Los bots usan percepción por `UAIPerceptionComponent`, no árbol de
comportamiento.** Un FPS arcade necesita reacción rápida y previsible, no
planificación: un `Tick` con máquina de estados (patrulla / combate) es más
fácil de depurar y más barato con ocho bots en partida.

**La granada valida en el servidor.** El cliente pide el lanzamiento con
`Server_ThrowGrenade`; el daño radial se calcula solo en el servidor y la
explosión se reproduce por multicast (RNF-3.1, RNF-3.2).

**El deslizamiento reutiliza `MaxWalkSpeedCrouched`** en vez de un modo de
movimiento propio: menos código y replicación gratuita por el
`CharacterMovementComponent`.

---

## Fase 5 — Cerrar lo accesible por código

Objetivo: agotar todo lo implementable sin depender de Steam, backend ni
servidor dedicado. De 54 a 62 requerimientos funcionales.

### El bug que la documentación no registraba

El proyecto **no compilaba en un árbol limpio**. `TrainingDrone.cpp` y
`TargetActor.cpp` declaraban ambos `HitFlashDuration` en un namespace anónimo,
con valores distintos. El *unity build* de Unreal fusiona varios `.cpp` en una
unidad de traducción, donde los dos nombres colisionan.

Pasó desapercibido por una razón sutil: **el build adaptativo excluye del unity
file los archivos que `git status` marca como modificados**. Mientras se editaba
`TrainingDrone.cpp`, quedaba fuera y compilaba; al hacer commit, entraba y
rompía.

Se detectó al compilar tras un commit limpio. Sin eso, habría aparecido al
clonar el repositorio en otra máquina — el mismo patrón que el módulo duplicado
de la Fase 1.

**Lección:** verificar con `-DisableAdaptiveUnity` antes de dar por bueno un
cambio. Es la configuración con que compilaría una máquina nueva o un CI.

### Qué se añadió

| Sistema | Clase | Requerimientos |
|---|---|---|
| Dominación | `AControlZone` | RF-8.3 |
| Oleadas | `AWaveManager` | RF-8.4 |
| Accesorios | `UWeaponAttachmentDataAsset` | RF-5.10, RF-9.4 |
| Límite de RPCs | `FRPCRateLimiter` | RNF-3.5 |
| Detección de caída | `IsConnectionLost` | RNF-2.6 |
| Minimapa | `DrawMinimap` | RF-7.9 |
| Killcam | `Client_StartKillCam` | RF-6.7 |
| Localización | LOCTEXT + `SetLanguage` | RNF-6.1, 6.2, 6.3 |
| Reasignar teclas | `RemapKey` | RF-11.2 |

### Decisiones

**Las zonas de Dominación congelan el progreso cuando hay dos equipos dentro,
en vez de revertirlo.** Si retrocediera, dos jugadores enfrentados se anularían
sin que ninguno avanzara jamás.

**La dificultad de Oleadas crece de forma lineal, no exponencial.** Con
multiplicador, la oleada 10 sería injugable y la 3 trivial.

**Los accesorios son multiplicadores, no valores absolutos.** Un mismo
accesorio sirve para todas las armas sin recalibrarlo, y el balance sigue en
el DataAsset del arma (RNF-9.3). Las ranuras se combinan multiplicando: así
ninguna combinación puede anular una estadística por acumulación.

**El minimapa nunca muestra enemigos.** Sería un wallhack incorporado y
contradiría RNF-3.6, que exige no darle al cliente la posición de quien no ve.

**La killcam sigue al asesino en vivo en vez de reproducir una grabación.** Una
repetición real exigiría el sistema de Replay y guardar el estado de todos los
actores; seguir al asesino responde la misma pregunta por una fracción del
coste.

**La RPC que excede el límite se descarta en silencio.** Responder con un error
le diría al atacante dónde está el umbral.

**La reasignación se apoya en `UEnhancedInputUserSettings`** en vez de mantener
un mapa propio: el motor ya persiste los perfiles, y duplicarlo se
desincronizaría con la UI estándar.

### Localización: lo que no basta

Migrar a `LOCTEXT` no es envolver cadenas. Los textos con variables se
construían con `FString::Printf`, que **impide traducir**: concatenar en código
fija el orden de las palabras y la concordancia de plural, y ningún idioma
tiene por qué respetarlos. Se pasaron a `FText::Format` con argumentos
nombrados, que el traductor puede reordenar, y el recuento de bajas usa la
forma plural del motor en vez de añadir una "s".

### Un aviso de deprecación que habría roto el proyecto

`GetCurrentKeyProfile` está obsoleto en 5.8 y el compilador lo advierte:
*"your project will no longer compile"* en la próxima versión. Solo apareció al
compilar con `-DisableAdaptiveUnity`. Sustituido por `GetActiveKeyProfile`.

---

## Estado al cierre

**62 de 100 requerimientos funcionales, 41 de 58 Must.** Detalle en
[TRAZABILIDAD.md](TRAZABILIDAD.md).

No queda programación de cliente pendiente sin dependencias externas. Lo que
falta es trabajo de editor, contenido (7 armas y 3 mapas) y las tres
dependencias de siempre.

### Lo que queda bloqueado

Tres dependencias externas explican casi todo lo pendiente. Ninguna se resuelve
escribiendo código del cliente:

1. **Steamworks SDK** → RF-1 (cuenta), RF-12 (social)
2. **Backend de progresión** → RF-14 a RF-16, RNF-3.4
3. **Servidor dedicado** → RF-3 (matchmaking), RNF-7, RNF-8.
   El motor instalado desde el launcher no puede compilarlo:
   *"Server targets are not currently supported from this engine
   distribution"*. Requiere Unreal compilado desde el código fuente.

### Lo que queda por hacer, sin dependencias

Ya no es programación:

- **Widgets del menú**: la navegación está completa en C++; falta colocar los
  botones en UMG, que es trabajo de editor
- **Sound Classes**: los volúmenes se guardan pero no enrutan a nada hasta que
  existan los assets de audio
- **Colocar los actores nuevos**: Dominación necesita tres `AControlZone` en un
  mapa; Oleadas, un `AWaveManager` con sus `PlayerStart`
- **Crear los `DA_*` de accesorios**: el sistema lee lo que encuentre en el
  registro de assets
- **Contenido**: 7 armas y 3 mapas de los que pide el checklist
- **Ejecutar `fix_gamepad.py` y `fix_movimiento.py`** en el editor

### Bug abierto: movimiento WASD

El personaje se mueve siempre en la misma dirección con cualquier tecla.

**Causa probable, hallada al auditar el código:** `build_training_zone.py`
crea las acciones de `COMBAT_ACTIONS` con `value_type = BOOLEAN`, y asume que
`IA_Move` viene de la plantilla First Person —si no existe, solo deja un aviso
y continúa—. Si `IA_Move` se creó por esa vía quedó como booleano, y
`Get<FVector2D>()` convierte un `bool` en `(1, 0)`: las cuatro teclas producen
el mismo desplazamiento lateral.

`fix_movimiento.py` comprueba el tipo, lo corrige a `Axis2D`, reconstruye los
modificadores de las cuatro teclas y vuelve a guardar el contexto. **Pendiente
de ejecutar y verificar en el editor.**

---

## Fase 6 — Giro a shooter realista

El juego deja de ser arcade. Detalle completo en [REALISMO.md](REALISMO.md).

| Sistema | Clase | Qué cambia |
|---|---|---|
| Balística | `UShooterBallisticsSubsystem` | Balas simuladas: caída, tiempo de vuelo, penetración, supresión |
| Superficies | `UShooterPhysicalMaterial` | Resistencia balística por material |
| Zonas de impacto | `ULagCompensationComponent` | Cabeza como esfera; torso, abdomen, brazos, piernas |
| Heridas | `UHealthComponent` | Sin regeneración; hemorragia, cojera, vendas, caídas |
| Movimiento | `AShooterCharacter` | Inercia, resistencia, inclinación, balanceo, respiración |
| Armas | `AWeaponBase` | Cargadores reales, recámara, selector de fuego, puesta a cero |
| HUD | `AShooterHUD` | Modo realista sin mira ni munición exacta |
| Mapa | `build_realistic_map.py` | `Lvl_Distrito`: barrio de 160 × 120 m con tres carriles |

### Decisiones

**El retroceso y el balanceo escriben la rotación de control directamente**,
no con `AddControllerPitchInput`. Esa vía multiplica por las escalas de
entrada heredadas del motor (±2,5), así que los grados del DataAsset no eran
grados. Ojo: el retroceso de los DataAssets antiguos se percibirá ~2,5 veces
menor; `build_realistic_map.py` ya escribe valores nuevos.

**El desfase de rebobinado es constante durante el vuelo de la bala.** El
tirador vio a los demás con ese retraso; una bala que tarda 0,3 s debe
encontrarlos donde él los vería 0,3 s después, no donde estaban al disparar.

**La bala guarda una copia del modelo de daño, no el arma.** Al morir, el
arma se destruye; los disparos simultáneos deben contar igual.

**La cabeza es una esfera, no una franja de la cápsula.** La cápsula mide
84 cm de ancho: contar como disparo a la cabeza todo lo que rozaba su parte
alta premiaba fallar.

**La inclinación desplaza la cabeza también en el servidor.** Sin eso, quien
se asoma tras una esquina vería y dispararía con la cabeza fuera de su
cápsula, sin poder recibir impactos.

### Un error de enlazado

`UPhysicalMaterial` vive en el módulo `PhysicsCore`, no en `Engine`. Compiló
sin problemas y falló al enlazar con nueve símbolos sin resolver. Añadido a
`Shoter.Build.cs`.

### La causa real del bug de WASD

`IA_Move` sí era Axis2D. El fallo estaba en los **modificadores**:
`unreal.InputModifierSwizzleAxis()` crea el objeto en el paquete
transitorio, y al guardar `IMC_Combate` la referencia se descarta sin aviso.
El contexto quedaba con W, A, S y D sin modificadores: las cuatro daban
(1, 0). Encima, `BP_Jugador` cargaba también `IMC_Default` de la plantilla,
que mapea `IA_Move` a las mismas teclas, y ambos se pisaban.

Solución en `build_realistic_map.py`: los modificadores se crean con
`unreal.new_object(..., outer=context)`, el contexto se reconstruye entero y
el personaje usa solo `IMC_Combate`. El script relee lo guardado y lo deja en
el log (`W -> 1 modificadores en ['IMC_Combate']`).

**Lección:** en Python de Unreal, todo subobjeto que vaya dentro de un asset
necesita el asset como *outer*.

### Navegación de los bots

El volumen de navegación se colocaba, pero el editor guardaba el nivel antes
de terminar de construir la malla: los bots no podían moverse. Ahora la malla
se genera al cargar el nivel (`RuntimeGeneration=Dynamic`).

### Lo que el código no resuelve

El mapa usa materiales procedurales: coherentes, pero no fotorrealistas. El
salto visual exige importar materiales de Fab/Megascans (gratuitos); el
script los detecta y los usa si están. Personajes, animaciones y sonido de
armas siguen siendo los de la plantilla.

---

## Cómo retomar el proyecto

1. Abrir `Shoter.uproject` (compila el C++ al arrancar)
2. Abrir `Content/Maps/Lvl_Entrenamiento`
3. Play

Si algo falla, el registro de Unreal (`Saved/Logs/Shoter.log`) dice qué. Los
scripts escriben con el prefijo de su nombre: `[entrenamiento]`, `[camara]`,
`[arma]`, `[personaje]`.
