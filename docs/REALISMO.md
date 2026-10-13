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

3. Abre `Content/Maps/Lvl_Distrito` y pulsa **Play**. Hay bots: el modo
   rellena hasta 8 jugadores.

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
- **Aumento por arma.** `AimFOV`: miras metálicas 55°, visor 30°.

| Arma | Daño | RPM | Cargador | V. boca | Penetración |
|---|---:|---:|---:|---:|---:|
| Fusil de asalto 5,56 | 40 | 750 | 30 | 920 m/s | 30 |
| Pistola 9 mm | 30 | 450 | 17 | 370 m/s | 12 |
| Fusil de tirador 7,62 | 70 | 300 | 20 | 830 m/s | 55 |

El fusil de tirador se crea (`BP_Tirador`) pero no va en el equipo inicial:
falta una pantalla de equipamiento que lo ofrezca.

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
| Norte | Callejón con contenedores de basura, cajas y vallas atravesables | Corta | A |
| Centro | Avenida de 16 m con coches, barreras y plaza con monumento | Media | B |
| Sur | Patio industrial: nave grande, contenedores apilados, muro con huecos | Media-larga | C (en la nave) |

- Edificios entre carriles con puertas a ambos lados y ventanas a la avenida.
  El bloque central tiene dos plantas con rampa interior.
- Fachadas altas alrededor: cierran el mapa sin muros invisibles.
- Iluminación de tarde: sol a 5200 K, cielo con atmósfera y nubes
  volumétricas, niebla volumétrica, post-proceso con grano y viñeta suaves.
- Navegación para bots y zonas de Dominación ya colocadas.

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
