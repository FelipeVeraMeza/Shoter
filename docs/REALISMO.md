# 🎖️ Shooter realista

El juego pasó de FPS arcade a shooter táctico realista. Este documento explica
qué cambió, dónde vive cada sistema y cómo ajustarlo. El balance sigue en
DataAssets: nada de lo que aquí aparece obliga a recompilar para afinarlo.

---

## Puesta en marcha

1. Compila (el editor cerrado) y abre el proyecto.
2. Ejecuta en la consola Python del editor:

   ```python
   exec(open(r"<proyecto>/Content/Python/build_realistic_map.py").read())
   ```

3. El editor abre `Lvl_Distrito` (es el mapa de inicio del editor): pulsa
   **Play**. Hay bots: el modo rellena hasta 8 jugadores.

**El script hay que ejecutarlo después de compilar**: crea materiales y
coloca actores de clases C++ nuevas (`AShooterFireSource`,
`AShooterMenuGameMode`). Si se ejecuta con un binario viejo, avisa en el log
y deja fuera los fuegos.

El juego empaquetado (o *Standalone*) arranca en `Lvl_Menu`: **Jugar →
modo y jugadores → Desplegar en Distrito**. La URL lleva las opciones
(`Lvl_Distrito?modo=dominacion?jugadores=8`) y las lee
`AShooterGameMode::InitGame`.

El script es idempotente: reejecutarlo actualiza armas, materiales y controles,
y reconstruye solo los actores que generó él mismo (etiqueta `ShoterGenerado`).
Lo que añadas a mano al nivel se conserva.

---

## Controles

| Acción | Teclado | Mando |
|---|---|---|
| Asomarse izquierda / derecha (mantener) | `Q` / `E` | Cruceta ← / → |
| Selector de fuego | `B` | Cruceta ↑ |
| Vendarse | `H` | Cruceta ↓ |
| Granada | `G` | RB |
| Contener la respiración | `Shift` mientras apuntas | — |
| Cambiar de arma | Rueda o `1` | Y |
| Comprobar cargador | `R` con el cargador lleno | X |
| Pausa | `P` / `Esc` | Start |
| Equipamiento (arma principal) | `L` | — |
| Lista de controles en pantalla | `F1` | — |

`F1` y `L` no pasan por Enhanced Input: son teclas de interfaz y las lee
`AShooterPlayerController::PollMenuKeys`, así que funcionan también muerto.

---

## Primera persona — `UShooterFirstPersonComponent`

El arma ya no flota: la sostienen unos brazos.

- **Brazos.** `AShooterCharacter::FirstPersonArms` es el mismo maniquí que
  el cuerpo (Manny o Quinn, con el tinte del equipo), con la cabeza y las
  piernas ocultas y una pose de apuntar de la plantilla (`MF_Rifle_Idle_ADS`,
  `MF_Pistol_Idle_ADS`) en modo *Single Node*.
- **El arma manda, los brazos siguen.** El componente coloca el arma
  respecto a la cámara y luego mueve los brazos para que el socket
  `HandGrip_R` caiga en la empuñadura (brazos = mano⁻¹ · arma). Así la mira
  queda siempre en el eje de la vista al apuntar, con cualquier malla.
- **Geometría deducida de la malla** (`AWeaponBase::BuildViewFrame`): el eje
  largo de la caja es el cañón (orientado al socket `Muzzle`), el borde
  superior es la línea de mira. Si una malla no encaja, se fija a mano con
  `bOverrideSightLocation` / `SightLocation` en el DataAsset.
- **Peso del arma:** balanceo al andar, retraso al girar, golpe de retroceso
  hacia el hombro, posición baja a la carrera, giro en la recarga (con la
  animación `MM_*_Reload` estirada a `ReloadTime`), bajada al cambiar de arma
  o vendarse.
- **Sin atravesar paredes:** brazos y arma son primitivas *First Person*: la
  cámara los dibuja con su propio campo de visión (74°, 52° apuntando) y a
  escala 0,6 hacia el ojo.
- **Lo que ven los demás:** el cuerpo usa `ABP_TP_Rifle` / `ABP_TP_Pistol`
  de la plantilla (solo dependen de `Character`, con *aim offset* por
  cabeceo) y el arma va en su mano. Cada arma tiene dos mallas: la del
  mundo (`WeaponMesh`) y la de primera persona (`FirstPersonMesh`).

Ajustes por arma en el DataAsset: `HipSightOffset` (dónde queda la mira a la
cadera), `AimEyeDistance` (distancia ojo-mira), `VisualKickBack/Pitch`.

---

## Visores — `EWeaponSightType`

| Tipo | Arma | Cómo se ve |
|---|---|---|
| Miras metálicas | Pistola | Se apunta por encima de la corredera |
| Holográfico | Fusil de asalto | Caja con ventana y cristal con punto y anillo rojos (`M_Reticula`), montada sobre el arma |
| Aumentos | Fusil de tirador | Tubo con anillas; al apuntar, el HUD dibuja la imagen del visor a pantalla completa (retícula dúplex con marcas de caída) y el arma se oculta |

Las piezas del visor se construyen con mallas básicas en
`AWeaponBase::LayoutSight`; no hace falta ningún modelo.

---

## Efectos — `ShooterEffects` (sin Niagara)

El proyecto no tiene sistemas de partículas, así que los efectos se
construyen con mallas básicas del motor y los materiales que crea el script
en `Content/Realista/Efectos`. Si un DataAsset o un material físico tiene un
sistema Niagara asignado, se usa ese en su lugar.

| Efecto | Clase | Detalle |
|---|---|---|
| Fogonazo | `AWeaponBase::ShowMuzzleFlash` | Tres planos aditivos (`M_Fogonazo`) en la boca, girados al azar, y una luz de 50 ms que ilumina el entorno |
| Trazadora | `AShooterTracer` | Una de cada `TracerEveryNShots` balas; trazo incandescente (`M_Trazadora`) que vuela al 55 % de la velocidad real para que el ojo lo siga |
| Casquillo | `AShooterShellCasing` | Cilindro de latón con física, expulsado a la derecha; solo a menos de 25 m de la cámara y con tope de 48 |
| Impacto | `AShooterImpactEffect` | Nube de polvo del color de la superficie, esquirlas con física (de cerca) y marca (`M_Impacto`); sangre en los cuerpos |
| Explosión | `AShooterGrenade` | Impacto a escala 5, destello de luz y el disparo de la plantilla muy grave |
| Fuego | `AShooterFireSource` | Llamas en planos cruzados (`M_Fuego`), luz que parpadea con ruido y una columna de humo de esferas translúcidas que el viento inclina |

**Sonido:** el proyecto solo trae `FirstPersonTemplateWeaponFire02`. Se usa
para las tres armas con distinto tono (`FireSoundPitch`) y con atenuación
real: se oye a 150 m y pierde agudos con la distancia. Los sonidos propios
de cada arma necesitan assets de Fab.

---

## Balística — `UShooterBallisticsSubsystem`

Las balas ya no son trazados instantáneos: el servidor simula cada una con
velocidad finita, gravedad y pérdida de velocidad, por tramos de 1/60 s.

- **Caída y tiempo de vuelo.** Un 5,56 a 300 m tarda ~0,4 s y cae lo bastante
  como para tener que compensar.
- **Puesta a cero.** El disparo sale ligeramente elevado para que la bala
  cruce la línea de mira a `ZeroingDistance` (100 m en el fusil).
- **Penetración.** Cada superficie tiene una resistencia por centímetro
  (`UShooterPhysicalMaterial`). La bala atraviesa si le queda
  `PenetrationPower` y sale con menos energía y algo desviada.
- **Supresión.** Una bala que pasa a menos de 3 m de la cabeza de alguien le
  oscurece la visión, le mete aberración cromática y le hace temblar el pulso.
- **Compensación de latencia.** Cada tramo se resuelve contra los personajes
  rebobinados. El desfase es constante durante el vuelo: la bala encuentra a
  los demás donde el tirador los vería en ese momento.

Si el tirador muere con la bala en el aire, la bala sigue contando: guarda una
copia del modelo de daño (`FShooterDamageModel`), no el arma.

### Resistencias de referencia

| Material | Resistencia | Fusil 5,56 (30) atraviesa… |
|---|---:|---|
| Valla de madera (4 cm) | 1 | Sí, de sobra |
| Chapa ondulada (3 cm) | 2 | Sí |
| Pared de ladrillo (25 cm) | 3 | No (75) |
| Hormigón (sin material asignado) | 4 | No |
| Sacos terreros | 8 | No |

Una superficie sin `UShooterPhysicalMaterial` se trata como hormigón: es
preferible que una pared sin configurar detenga balas a que las deje pasar.

**Ojo:** las trazas usan la colisión simple de la malla, que no lee el material
físico del material visual. El script lo asigna con
`SetPhysMaterialOverride` en cada componente; si colocas mallas a mano, haz lo
mismo o asígnalo en su *Body Setup*.

---

## Zonas de impacto

La cabeza es una esfera aparte (13 cm de radio), no una franja de la cápsula:
la cápsula es mucho más ancha que un cráneo. El cuerpo se divide por altura y
distancia al eje.

| Zona | Multiplicador (fusil) | Torso = 40 |
|---|---:|---:|
| Cabeza | ×3 | 120 → muerte |
| Torso | ×1 | 40 → 3 impactos |
| Abdomen | ×0,9 | 36 |
| Brazo | ×0,6 | 24 |
| Pierna | ×0,6 | 24 + cojera |

La zona viaja como nombre de hueso en el daño puntual (`zone_head`,
`zone_leg`...), así que `UHealthComponent` la recibe por la vía estándar.

---

## Salud y heridas — `UHealthComponent`

- **Sin regeneración** (`RegenRate = 0`). Queda como opción para modos arcade.
- **Hemorragia.** Un impacto de 15 o más abre una herida; cada una resta
  1,2 HP/s. Hasta 3 a la vez. Quien abrió la herida se lleva la baja si la
  víctima se desangra.
- **Pierna herida.** Velocidad al 70 % hasta vendarse.
- **Vendas.** 2 por vida, 3,5 s sin poder disparar: detienen la hemorragia,
  curan la pierna y devuelven 15 HP.
- **Caídas.** Desde ~6 m hacen daño; desde ~20 m matan. Las fuertes lesionan
  la pierna.

---

## Movimiento y puntería — `AShooterCharacter`

| Parámetro | Antes (arcade) | Ahora |
|---|---:|---:|
| Andar | 400 cm/s | 360 |
| Correr | 650 | 590, solo hacia delante |
| Apuntando | igual | ×0,55 |
| Aceleración | instantánea | 1400 cm/s² |
| Salto | 420 | 360, cuesta resistencia |
| Control en el aire | 0,35 | 0,08 |
| Deslizamiento | sí | desactivado (`bAllowSlide`) |

- **Resistencia.** Correr gasta 12/s (unos 8 s de sprint); saltar, 18. Se
  recupera tras 1,2 s de descanso, más rápido agachado.
- **Salida del sprint.** 0,25 s antes de poder disparar.
- **Inclinación.** Desplaza la cabeza 38 cm y gira la vista 12°. Se recorta
  contra las paredes. El servidor desplaza también la esfera de la cabeza en
  la compensación de latencia: quien se asoma se expone.
- **Balanceo de la mira.** Ruido Perlin sobre la vista real (la bala va a
  donde se apunta). Crece con el cansancio, el movimiento y la supresión;
  baja agachado. Contener la respiración lo reduce al 15 % durante 5 s; si se
  agota, el pulso se dispara un rato.
- **Retroceso con recuperación.** Cada disparo sube el arma en unos frames y,
  al dejar de disparar, se recupera `RecoilRecoveryFraction` (70 % en el
  fusil). Una ráfaga larga sube la mira de verdad.
- **Dispersión por postura.** Desde la cadera y corriendo es casi a ciegas;
  apuntando y quieto, precisa; en el aire, ×2,5.

---

## Armas — `AWeaponBase`

- **Cargadores reales.** La reserva son cargadores, no un fondo común. Al
  recargar se cambia por el más lleno y el viejo vuelve a la bolsa con lo que
  le quedaba.
- **Bala en recámara.** Con cerrojo cerrado, la recarga táctica deja
  cargador + 1. En vacío, la recarga es más lenta (`EmptyReloadTime`).
- **Selector de fuego.** `AvailableFireModes` en el DataAsset; el servidor
  solo acepta posiciones que el arma tenga.
- **Aumento por arma.** `AimFOV`: miras metálicas 55°, holográfico 55°,
  visor de aumentos 20° (unos 4×).

| Arma | Daño | RPM | Cargador | V. boca | Penetración |
|---|---:|---:|---:|---:|---:|
| Fusil de asalto 5,56 | 40 | 750 | 30 | 920 m/s | 30 |
| Pistola 9 mm | 30 | 450 | 17 | 370 m/s | 12 |
| Fusil de tirador 7,62 | 70 | 300 | 20 | 830 m/s | 55 |

### Equipamiento

La pantalla de equipamiento (`L`, o *Pausa → Equipamiento*) ofrece las armas
de `AShooterGameMode::PrimaryWeaponOptions` (fusil de asalto y fusil de
tirador). La elección viaja al servidor
(`AShooterPlayerController::Server_SelectPrimaryWeapon`), que solo acepta
armas de esa lista, y se aplica al reaparecer. La pistola, dos granadas y dos
vendas van siempre. Un 25 % de los bots lleva el fusil de tirador
(`MarksmanBotChance`).

---

## Bots — `AShooterBotController`

Antes de esta revisión los bots no podían jugar una partida de verdad: no
tenían `PlayerState` (sin equipo, todos disparaban a todos), al morir no
reaparecían ni daban puntos, no podían apuntar arriba ni abajo y veían a
través de las paredes.

Ahora pelean como tiradores disciplinados:

- **Necesitan verlos:** cono de 70° y línea de visión al pecho o la cabeza
  (90 m de alcance). Las vallas de madera tapan la vista aunque las balas
  las atraviesen.
- **Tiempo de reacción** de 0,35 a 0,75 s ante un enemigo nuevo.
- **Puntería con error** que crece con la distancia y con el movimiento, y
  baja mientras siguen al mismo blanco. A veces buscan la cabeza de cerca.
- **Disciplina de tiro:** ráfagas de 3 a 6 en corto, pares a media
  distancia, tiro a tiro de lejos. Apuntan con la mira a más de 10 m y se
  agachan cuando están quietos y lejos.
- **Movimiento:** se acercan hasta 22 m y desde ahí dan pasos laterales.
  Patrullan entre las zonas A, B y C y el lado enemigo.
- **Reaccionan al daño:** se giran hacia quien les dispara aunque no lo
  vean, y van a buscarlo a su última posición conocida.
- **Mantenimiento:** recargan en las pausas y se vendan cuando sangran y no
  hay nadie a la vista.
- Tienen indicativo de radio (Halcón, Lobo, Cuervo...), equipo asignado
  antes de aparecer y aparecen en el lado de su equipo.

El fuego amigo existe, pero una baja propia no puntúa.

---

## HUD realista

Activo por defecto (`UShooterSettingsSubsystem::SetRealisticHUD`):

- Sin mira, sin minimapa, sin marcador de impacto, sin barra de vida.
- La salud se intuye por la visión: por debajo del 50 % la imagen pierde color
  y se cierra la viñeta.
- La munición solo aparece al pulsar recargar o al recargar, como estimación
  ("Casi lleno", "Medio"...), con el número de cargadores.
- Siempre visibles: resistencia y aliento cuando faltan, y los avisos
  `SANGRANDO`, `PIERNA HERIDA`, `VENDANDO…`.

El HUD completo sigue disponible desactivando el ajuste.

---

## El mapa — `Lvl_Distrito`

Barrio urbano de 160 × 120 m. Alfa aparece al oeste y Bravo al este.

| Carril | Ambiente | Distancia | Zona |
|---|---|---|---|
| Norte | Callejón con contenedores de basura, bolsas, bidones ardiendo, tendido eléctrico y vallas atravesables | Corta | A |
| Centro | Avenida de 16 m con coches (dos ardiendo), autobús calcinado y camión, barreras, cráteres y plaza con monumento partido | Media | B |
| Sur | Patio industrial: nave con pasarela elevada, contenedores apilados o abiertos, gaviones, muro con huecos | Media-larga | C (en la nave) |

- Edificios entre carriles con puertas a ambos lados y ventanas a la avenida,
  con alféizares, dinteles, jambas, impostas, balcones y azoteas con
  máquinas, depósitos y antenas. Dentro hay mobiliario (tienda saqueada,
  oficina, vivienda) que sirve de cobertura.
- El bloque central tiene dos plantas con rampa interior; el del este está
  en ruinas (muros desiguales, losa caída, escombros, vigas, fuego).
- Andamio con lona en la fachada del bloque NE.
- Calzada con línea central, bordes, pasos de peatones, bordillos y tapas de
  alcantarilla; farolas con brazo y luminaria, alguna doblada.
- Sacos terreros apilados a matajunta, barreras con perfil escalonado,
  palés, neumáticos, árboles secos, bancos y montones de cascotes.
- Fachadas altas alrededor con ventanas (algunas tapiadas): cierran el mapa
  sin muros invisibles.
- **Iluminación de atardecer dorado:** sol a 13° y 3800 K, dispersión
  volumétrica (los rayos de luz atraviesan el humo), *light shafts* al mirar
  al sol, atmósfera con más polvo en suspensión, niebla volumétrica cálida,
  bolsas de bruma local y gradación de color sucia (algo desaturada, luces
  cálidas, sombras frías, grano y aberración cromática).
- Navegación para bots, zonas de Dominación y puntos de aparición con la
  etiqueta de su equipo (`PlayerStartTag` = `Alfa` / `Bravo`).

Del orden de 1500 piezas, todas con movilidad estática; el número exacto lo escribe el script en el log (`piezas de geometría`).

### Menú — `Lvl_Menu`

`AShooterMenuGameMode` pone la vista en la cámara del decorado (un rincón del
Distrito con un coche ardiendo) y abre el menú principal. Los menús son
Slate puro (`SShooterMenuScreen`): principal, jugar (modo y número de
jugadores), opciones (sensibilidad, invertir Y, HUD realista, volumen,
calidad, ventana, VSync), controles, equipamiento, progresión y pausa. Si
alguna vez se maquetan los `WBP_*` y se asignan en *Project Settings →
Shooter Menu Subsystem*, tienen prioridad sobre el menú integrado.

### Materiales

Sin assets propios, el script crea un material maestro procedural
(`M_Superficie`): mezcla dos colores y dos rugosidades con ruido en
coordenadas de mundo, sin texturas, así que los cubos escalados no estiran
nada. Es jugable y coherente, pero **no es fotorrealista**.

Para el salto visual de verdad: descarga materiales de **Fab** (los Megascans
son gratuitos con Unreal) a `Content/Fab` o `Content/Megascans` y reejecuta el
script. Busca por palabras clave (`concrete`, `brick`, `asphalt`, `rust`,
`wood`...) y los usa en lugar de los procedurales.

---

## Rendimiento

- **TSR** como antialiasing: reconstruye a resolución completa desde una
  interna menor, que es lo que paga Lumen y las sombras virtuales.
- **Geometría STATIC** cuando la iluminación estática está desactivada (es el
  caso): Lumen y las sombras virtuales cachean lo que no se mueve.
- **Sin desenfoque de movimiento:** emborrona al enemigo justo al girar.
- Las armas replican a 20 Hz en vez de a la frecuencia del personaje.
- El subsistema de balística solo hace *tick* cuando hay balas en vuelo.
- Los efectos no existen en un servidor dedicado, los impactos lejanos (más
  de 120 m) no se crean y las piezas con física (casquillos, esquirlas) solo
  aparecen cerca de la cámara.
- Los bots comprueban la línea de visión cada 0,15 s, no en cada fotograma.
