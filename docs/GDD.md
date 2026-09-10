# 🎮 GAME DESIGN DOCUMENT — ShooterGame

> Documento vivo. Toda decisión de diseño se registra acá. Si algo no está en este
> documento, no está en el juego.

| Campo | Valor |
|-------|-------|
| **Título provisional** | ShooterGame |
| **Género** | FPS arcade de acción, multijugador |
| **Motor** | Unreal Engine 5.3+ |
| **Plataforma v1.0** | PC (Windows 10/11) vía Steam |
| **Jugadores** | 1–10 según modo (online) |
| **Modelo de negocio** | Premium (pago único) — ver §8 |
| **Clasificación objetivo** | ESRB Teen / PEGI 16 |
| **Duración de partida** | 5–8 minutos |

---

## 1️⃣ PILARES DE DISEÑO

Los cuatro pilares son el criterio para aceptar o rechazar cualquier feature.
Si una idea no refuerza un pilar, no entra.

1. **El disparo se siente contundente.** Respuesta inmediata, retroceso visible,
   audio con peso, feedback claro de impacto. El *game feel* del arma es la
   feature número uno del juego.
2. **Partidas cortas y rejugables.** Entrar y estar disparando en menos de 60
   segundos. Nada de partidas de 30 minutos.
3. **Fácil de aprender, difícil de dominar.** Controles estándar del género, sin
   tutorial obligatorio de 20 minutos. La profundidad viene del control del
   retroceso, el movimiento y el posicionamiento.
4. **Sin pay-to-win.** Todo lo que afecta el juego se desbloquea jugando. El
   dinero solo compra cosméticos.

---

## 2️⃣ CORE LOOP

```
   Menú principal
        ↓
   Matchmaking (< 60s)
        ↓
   ┌──────────────────────────┐
   │  PARTIDA (5–8 min)       │
   │  ┌────────────────────┐  │
   │  │ Aparecer → Moverse │  │
   │  │   ↓                │  │
   │  │ Combatir → Eliminar│  │
   │  │   ↓                │  │
   │  │ Morir → Reaparecer │  │
   │  └────────────────────┘  │
   └──────────────────────────┘
        ↓
   Pantalla de resultados + XP
        ↓
   Desbloqueos / progresión
        ↓
   (volver a jugar)
```

**Loop de segundo a segundo:** apuntar → disparar → controlar retroceso → recargar
→ reposicionarse.

**Loop de sesión:** jugar partidas → subir nivel → desbloquear arma o accesorio →
probar la nueva configuración.

---

## 3️⃣ EL JUGADOR

### 3.1 Movimiento

| Acción | Tecla | Valor objetivo |
|--------|-------|----------------|
| Caminar | `W A S D` | 400 cm/s |
| Correr (sprint) | `Shift` | 650 cm/s |
| Agacharse | `Ctrl` | 200 cm/s |
| Saltar | `Espacio` | 420 cm de impulso |
| Deslizarse (slide) | `Ctrl` corriendo | 900 cm/s inicial, 0.6 s |

- Sin aceleración/desaceleración pesada: el arcade exige respuesta inmediata.
- **No hay caída de daño** por altura (decisión arcade, evita frustración).
- El sprint desactiva el disparo; volver a disparar toma 0.2 s ("sprint-out time").

### 3.2 Salud

- **Salud base:** 100 HP. Sin escudos ni armadura en v1.0.
- **Regeneración:** empieza tras 5 s sin recibir daño, a 25 HP/s.
- Decisión: regeneración (no botiquines) porque mantiene el ritmo alto y elimina
  la necesidad de gestionar objetos.

---

## 4️⃣ SISTEMA DE ARMAS

### 4.1 Parámetros de cada arma

Toda arma se define como un `UDataAsset` con estos campos. **Ningún valor de
balance vive hardcodeado en C++.**

| Parámetro | Descripción |
|-----------|-------------|
| `Damage` | Daño base por impacto al torso |
| `HeadshotMultiplier` | Multiplicador a la cabeza (objetivo: 1.5–2.0) |
| `FireRate` | Disparos por minuto (RPM) |
| `MagazineSize` | Balas por cargador |
| `ReloadTime` | Segundos de recarga |
| `RecoilPattern` | Curva de retroceso (vertical + horizontal) |
| `SpreadBase` / `SpreadADS` | Dispersión de cadera / apuntando |
| `EffectiveRange` | Distancia sin caída de daño |
| `DamageFalloff` | Curva de caída de daño por distancia |
| `ADSTime` | Tiempo en levantar la mira |

### 4.2 Arsenal v1.0 (8 armas)

| # | Arma | Categoría | Rol | TTK objetivo |
|---|------|-----------|-----|--------------|
| 1 | Pistola | Secundaria | Respaldo, siempre disponible | ~600 ms |
| 2 | SMG ligera | Automática | Corta distancia, alta movilidad | ~350 ms |
| 3 | SMG pesada | Automática | Corta-media, más daño, más retroceso | ~380 ms |
| 4 | Rifle de asalto | Automática | Todoterreno, media distancia | ~400 ms |
| 5 | Carabina táctica | Ráfaga | Media, ráfagas de 3 | ~380 ms |
| 6 | Escopeta | Dispersión | Muy corta, letal a quemarropa | ~250 ms |
| 7 | Rifle de tirador | Semiauto | Larga distancia, 2 impactos | ~500 ms |
| 8 | Francotirador | Cerrojo | Larga, un disparo a la cabeza = muerte | ~1 disparo |

**TTK global objetivo: 300–500 ms.** Más rápido se siente injusto ("morí sin
reaccionar"); más lento rompe el pilar arcade.

### 4.3 Accesorios

Cada arma tiene 3 ranuras: **mira**, **cañón**, **cargador**. Se desbloquean por
uso de esa arma específica (no por nivel de cuenta). Regla dura: todo accesorio
tiene un contrapeso (mejor mira = ADS más lento). Nunca una mejora pura, para
proteger el pilar 4.

---

## 5️⃣ MODOS DE JUEGO

| Modo | Jugadores | Condición de victoria | Prioridad |
|------|-----------|----------------------|-----------|
| **Todos contra todos** | 8 (FFA) | Primero en 30 eliminaciones o mayor puntaje en 8 min | 🔴 Debe estar |
| **Equipos** | 4v4 | Primero en 50 eliminaciones o mayor puntaje en 8 min | 🔴 Debe estar |
| **Dominación** | 5v5 | Capturar y mantener 3 zonas hasta 200 puntos | 🟡 Debería estar |
| **Oleadas (coop)** | 1–4 PvE | Sobrevivir 20 oleadas de IA | 🟢 Podría estar |

**Decisión:** el modo Oleadas se implementa último porque requiere IA de combate,
que es un sistema completo aparte. Su ventaja es que da valor cuando la población
online es baja (crítico para un indie: un juego solo-PvP muerto es injugable).

---

## 6️⃣ MAPAS

**4 mapas en v1.0.** Reglas de diseño:

- **Estructura de 3 carriles** (three-lane): dos rutas exteriores + una central.
  Estándar del género porque genera enfrentamientos legibles y predecibles.
- **Tamaño:** ~100 × 100 m. Cruzar el mapa entero: 45–60 s.
- **Ciclo de combate:** un jugador debe encontrar acción cada 10–15 s.
- **Zonas de aparición** dinámicas: nunca aparecer a la vista de un enemigo ni a
  menos de 25 m de uno.
- Cada mapa debe favorecer a **al menos 2 categorías de arma** distintas: nada de
  mapas 100 % francotirador.

| Mapa | Ambientación | Distancia dominante |
|------|--------------|---------------------|
| Almacén | Industrial interior | Corta |
| Azotea | Urbano vertical | Media |
| Planta | Fábrica abandonada | Corta-media |
| Puerto | Exterior con contenedores | Media-larga |

---

## 7️⃣ PROGRESIÓN

- **Niveles de cuenta 1–50.** XP por eliminación, asistencia, objetivo y partida
  completada. Se otorga XP incluso al perder (evita el abandono de partidas).
- **Desbloqueos por nivel:** armas nuevas cada ~5 niveles.
- **Desbloqueos por uso:** accesorios de cada arma, usando esa arma.
- **Cosméticos:** por temporada y por tienda.

Regla: un jugador nuevo debe ser **competitivo desde el nivel 1**. El arma inicial
(Rifle de asalto) es viable en todo el juego; lo que se desbloquea son
*alternativas*, no *mejoras*.

---

## 8️⃣ MONETIZACIÓN

**Recomendación: premium (pago único), USD 9.99 – 14.99.**

Razón — el modelo free-to-play exige, para funcionar, una población enorme, un
backend de economía con transacciones seguras, un pase de batalla con contenido
continuo y un equipo de LiveOps. Para un lanzamiento indie es el camino más
probable al fracaso: un F2P con pocos jugadores no tiene ni ingresos ni partidas.

| Modelo | Ventaja | Riesgo |
|--------|---------|--------|
| **Premium** ✅ | Ingreso desde el día 1; sin backend de pagos propio (lo hace Steam); menos población necesaria | Techo de ingresos limitado; la barrera de precio reduce jugadores |
| F2P + pase | Máximo alcance potencial | Necesita ~10× la población; backend complejo; sin comunidad previa, muere |

**Post-lanzamiento:** DLC cosméticos y mapas gratuitos (los mapas nunca se cobran:
dividir la población de un juego pequeño lo mata).

---

## 9️⃣ DIRECCIÓN DE ARTE Y AUDIO

**Arte: estilizado, no realista.** Materiales limpios, paleta saturada, siluetas
legibles. Decisión práctica y deliberada:

- El realismo compite directamente con estudios de 200 personas — es una batalla
  perdida para un equipo pequeño.
- El estilizado envejece mucho mejor.
- La legibilidad (distinguir enemigo de fondo al instante) es un requisito
  *competitivo*, no estético.

**Contraste obligatorio:** los personajes deben destacar del entorno. Entornos
desaturados, personajes saturados, equipos con colores claramente opuestos.

**Audio:** es el 50 % de la sensación de disparo.
- Capas por arma: mecanismo + detonación + cola/reverberación.
- Pasos de enemigos audibles y direccionales (información táctica).
- Mezcla con prioridad: los disparos cercanos y pasos nunca son tapados por música.

---

## 🔟 FUERA DE ALCANCE EN v1.0

Registro explícito de lo que **no** se hace, para evitar el crecimiento
descontrolado del alcance:

- ❌ Campaña o modo historia
- ❌ Vehículos
- ❌ Destrucción del entorno
- ❌ Battle royale
- ❌ Soporte VR
- ❌ Consolas (requiere kits de desarrollo y certificación)
- ❌ Juego cruzado con consolas
- ❌ Editor de mapas para la comunidad
- ❌ Clases o habilidades tipo héroe
