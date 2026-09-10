# 📋 REQUERIMIENTOS FUNCIONALES Y NO FUNCIONALES — ShooterGame

> FPS arcade multijugador — Unreal Engine 5 — PC (Steam)
> Base de diseño: [`GDD.md`](GDD.md) · Base técnica: [`ARQUITECTURA.md`](ARQUITECTURA.md)

## 🎯 ACTORES DEL SISTEMA

1. **Jugador** — usuario final que juega partidas online
2. **Anfitrión de partida privada** — jugador que crea y configura una sala cerrada
3. **Administrador / LiveOps** — operador del servicio (balance, baneos, temporadas)
4. **Sistemas externos** — Steam, Epic Online Services (EOS), servidores dedicados

## 🏷️ PRIORIZACIÓN (MoSCoW)

| Símbolo | Significado | Regla |
|---------|-------------|-------|
| 🔴 **M** | Must — el juego no existe sin esto | Bloquea el lanzamiento |
| 🟡 **S** | Should — importante, no bloqueante | Se corta si peligra la fecha |
| 🟢 **C** | Could — deseable | Solo si sobra tiempo |
| ⚪ **W** | Won't — fuera de v1.0 | Registrado para v2 |

---

## 1️⃣ JUGADOR

### 📌 REQUERIMIENTOS FUNCIONALES

#### Cuenta y sesión
- **RF-1.1** 🔴 El jugador inicia sesión automáticamente mediante su cuenta de Steam al abrir el juego
- **RF-1.2** 🔴 El sistema crea un perfil persistente vinculado al ID de Steam en el primer inicio
- **RF-1.3** 🔴 El jugador puede definir y modificar su nombre visible en partida
- **RF-1.4** 🔴 El sistema recupera el progreso del jugador (nivel, XP, desbloqueos) al iniciar sesión
- **RF-1.5** 🟡 El jugador puede jugar sin conexión contra bots si el servicio online no está disponible
- **RF-1.6** 🔴 El sistema informa con mensaje claro si no logra conectar con el servicio online

#### Menú y navegación
- **RF-2.1** 🔴 El jugador accede a un menú principal con: Jugar, Arsenal, Progresión, Opciones, Salir
- **RF-2.2** 🔴 El jugador puede seleccionar el modo de juego antes de buscar partida
- **RF-2.3** 🟡 El jugador ve su nivel, XP actual y progreso al siguiente nivel en el menú
- **RF-2.4** 🔴 El jugador puede salir del juego desde el menú sin cerrar el proceso por la fuerza

#### Matchmaking y partidas
- **RF-3.1** 🔴 El jugador puede buscar partida pública en el modo seleccionado
- **RF-3.2** 🔴 El sistema asigna al jugador a una partida con jugadores de su región
- **RF-3.3** 🔴 El jugador puede cancelar la búsqueda de partida en cualquier momento
- **RF-3.4** 🔴 El jugador puede abandonar una partida en curso y volver al menú
- **RF-3.5** 🟡 El jugador puede reconectarse a una partida de la que se desconectó, si sigue en curso
- **RF-3.6** 🟡 El sistema rellena con bots las partidas que no alcanzan el mínimo de jugadores
- **RF-3.7** 🟡 El sistema equilibra automáticamente los equipos al inicio y ante abandonos
- **RF-3.8** 🟢 El jugador puede crear una partida privada y compartir un código de acceso
- **RF-3.9** 🟢 El anfitrión de una partida privada puede configurar modo, mapa y límite de tiempo

#### Control del personaje
- **RF-4.1** 🔴 El jugador puede desplazarse en las cuatro direcciones (`W A S D`)
- **RF-4.2** 🔴 El jugador puede rotar la cámara con el mouse (mirar en primera persona)
- **RF-4.3** 🔴 El jugador puede saltar
- **RF-4.4** 🔴 El jugador puede agacharse
- **RF-4.5** 🔴 El jugador puede correr (sprint), con disparo deshabilitado mientras corre
- **RF-4.6** 🟡 El jugador puede deslizarse (slide) al agacharse mientras corre
- **RF-4.7** 🔴 El personaje colisiona con el entorno y no puede atravesar geometría sólida

#### Armas y combate
- **RF-5.1** 🔴 El jugador porta un arma principal y una secundaria, e intercambia entre ellas
- **RF-5.2** 🔴 El jugador puede disparar el arma equipada
- **RF-5.3** 🔴 El jugador puede apuntar con la mira (ADS), reduciendo la dispersión
- **RF-5.4** 🔴 El jugador puede recargar manualmente; la recarga es automática al vaciar el cargador
- **RF-5.5** 🔴 El sistema consume munición por disparo e impide disparar sin munición
- **RF-5.6** 🔴 El arma aplica un patrón de retroceso definido por su configuración
- **RF-5.7** 🔴 El arma aplica dispersión, mayor disparando desde la cadera que apuntando
- **RF-5.8** 🔴 El daño disminuye con la distancia según la curva de caída del arma
- **RF-5.9** 🔴 Los impactos en la cabeza aplican un multiplicador de daño
- **RF-5.10** 🟡 El jugador puede equipar accesorios (mira, cañón, cargador) en cada arma
- **RF-5.11** 🟢 El jugador puede lanzar granadas

#### Salud, muerte y reaparición
- **RF-6.1** 🔴 El jugador tiene 100 puntos de salud y recibe daño por impactos enemigos
- **RF-6.2** 🔴 La salud se regenera tras un período sin recibir daño
- **RF-6.3** 🔴 El jugador muere al llegar a 0 de salud
- **RF-6.4** 🔴 El jugador reaparece automáticamente tras un tiempo de espera
- **RF-6.5** 🔴 El sistema elige un punto de aparición seguro (sin enemigos a la vista ni cercanos)
- **RF-6.6** 🟡 El jugador ve quién lo eliminó y con qué arma
- **RF-6.7** 🟢 El jugador puede ver una repetición del momento de su muerte (killcam)

#### Interfaz en partida (HUD)
- **RF-7.1** 🔴 El jugador ve una mira (crosshair) que refleja la dispersión actual del arma
- **RF-7.2** 🔴 El jugador ve su munición actual y de reserva
- **RF-7.3** 🔴 El jugador ve su salud actual
- **RF-7.4** 🔴 El jugador ve el marcador del modo (puntaje y tiempo restante)
- **RF-7.5** 🔴 El jugador recibe confirmación visual y sonora al impactar a un enemigo (hitmarker)
- **RF-7.6** 🔴 El jugador ve un indicador de la dirección desde la que recibe daño
- **RF-7.7** 🟡 El jugador ve el registro de eliminaciones de la partida (killfeed)
- **RF-7.8** 🟡 El jugador puede consultar la tabla de posiciones durante la partida
- **RF-7.9** 🟢 El jugador ve un minimapa con aliados y objetivos

#### Modos de juego
- **RF-8.1** 🔴 El sistema ofrece el modo *Todos contra todos* (8 jugadores)
- **RF-8.2** 🔴 El sistema ofrece el modo *Equipos* (4v4)
- **RF-8.3** 🟡 El sistema ofrece el modo *Dominación* con captura de 3 zonas (5v5)
- **RF-8.4** 🟢 El sistema ofrece el modo cooperativo *Oleadas* contra IA (1–4 jugadores)
- **RF-8.5** 🔴 El sistema finaliza la partida al cumplirse la condición de victoria o el tiempo
- **RF-8.6** 🔴 El sistema muestra una pantalla de resultados con estadísticas al terminar

#### Progresión
- **RF-9.1** 🔴 El jugador gana XP por eliminaciones, asistencias, objetivos y completar la partida
- **RF-9.2** 🔴 El jugador sube de nivel al acumular XP suficiente
- **RF-9.3** 🔴 El sistema desbloquea armas al alcanzar niveles determinados
- **RF-9.4** 🟡 El sistema desbloquea accesorios según el uso de cada arma
- **RF-9.5** 🔴 El progreso persiste entre sesiones
- **RF-9.6** 🟡 El jugador puede consultar sus estadísticas acumuladas (K/D, precisión, partidas)
- **RF-9.7** 🟢 El jugador puede completar desafíos diarios por XP adicional

#### Personalización de arsenal
- **RF-10.1** 🟡 El jugador puede configurar sus equipamientos (loadouts) con arma principal y secundaria
- **RF-10.2** 🟡 El jugador puede guardar múltiples equipamientos y elegir uno antes de reaparecer
- **RF-10.3** 🟢 El jugador puede aplicar cosméticos a sus armas

#### Configuración
- **RF-11.1** 🔴 El jugador puede ajustar la sensibilidad del mouse
- **RF-11.2** 🔴 El jugador puede reasignar todas las teclas y controles
- **RF-11.3** 🔴 El jugador puede ajustar la calidad gráfica (presets y opciones individuales)
- **RF-11.4** 🔴 El jugador puede ajustar volúmenes por canal (general, efectos, música, voz)
- **RF-11.5** 🔴 El jugador puede activar/desactivar la sincronización vertical y limitar los FPS
- **RF-11.6** 🟡 El jugador puede elegir modo ventana, ventana sin bordes o pantalla completa
- **RF-11.7** 🟡 El jugador puede activar modos de daltonismo
- **RF-11.8** 🟡 El jugador puede personalizar el color y forma de la mira
- **RF-11.9** 🔴 La configuración persiste entre sesiones

#### Social
- **RF-12.1** 🟡 El jugador puede invitar amigos de Steam a su grupo
- **RF-12.2** 🟡 El grupo entra junto a la misma partida
- **RF-12.3** 🟢 El jugador puede usar chat de texto en partida
- **RF-12.4** 🟢 El jugador puede usar chat de voz con su equipo
- **RF-12.5** 🟡 El jugador puede silenciar a otro jugador
- **RF-12.6** 🔴 El jugador puede reportar a otro jugador por trampas o conducta inapropiada

### 🔒 REQUERIMIENTOS NO FUNCIONALES — JUGADOR

#### Rendimiento
- **RNF-1.1** El juego mantiene **60 FPS mínimo** a 1080p con calidad media en hardware objetivo (GTX 1060 / RX 580, 16 GB RAM)
- **RNF-1.2** El juego alcanza **144+ FPS** a 1080p con calidad baja en hardware de gama alta (competitivo)
- **RNF-1.3** El tiempo entre el clic y el disparo en pantalla no supera **1 frame** de retardo añadido
- **RNF-1.4** La carga de un mapa no supera **20 segundos** en SSD
- **RNF-1.5** El arranque hasta el menú principal no supera **30 segundos**
- **RNF-1.6** El tamaño de instalación no supera **15 GB**
- **RNF-1.7** El uso de memoria de video no supera **4 GB** en calidad media

#### Red
- **RNF-2.1** El servidor opera a una frecuencia de **60 Hz** (tickrate)
- **RNF-2.2** El juego es jugable con hasta **120 ms** de latencia
- **RNF-2.3** El consumo de ancho de banda no supera **128 kbps** por jugador
- **RNF-2.4** El sistema aplica **compensación de latencia** (lag compensation) para validar impactos
- **RNF-2.5** El tiempo de búsqueda de partida no supera **60 segundos** en horario activo
- **RNF-2.6** El cliente detecta la desconexión del servidor en menos de **10 segundos** e informa al jugador
- **RNF-2.7** Una pérdida de paquetes del 5 % no produce teletransportes visibles de personajes

#### Seguridad y anti-trampas
- **RNF-3.1** El servidor es **autoritativo**: valida movimiento, disparos, daño y puntaje
- **RNF-3.2** El cliente nunca decide si un impacto ocurrió ni cuánto daño aplica
- **RNF-3.3** El juego integra **Easy Anti-Cheat** antes de la beta abierta
- **RNF-3.4** El progreso del jugador se almacena en el servidor, no en el cliente
- **RNF-3.5** El sistema limita la frecuencia de RPCs por cliente para evitar saturación
- **RNF-3.6** El cliente no recibe la posición de enemigos fuera de su relevancia de red (anti-wallhack)
- **RNF-3.7** No se almacenan datos personales fuera del identificador de Steam

#### Usabilidad y accesibilidad
- **RNF-4.1** El jugador está disparando en menos de **60 segundos** desde que abre el juego
- **RNF-4.2** Los controles por defecto siguen el estándar del género (no requieren aprendizaje)
- **RNF-4.3** Toda información crítica del HUD es legible sin depender solo del color
- **RNF-4.4** Los textos son legibles a 1080p sin escalado
- **RNF-4.5** El juego es jugable íntegramente con mando (gamepad)
- **RNF-4.6** Los mensajes de error se expresan en lenguaje claro, sin códigos técnicos

#### Compatibilidad
- **RNF-5.1** Compatible con Windows 10 y 11 (64 bits)
- **RNF-5.2** Compatible con DirectX 12; retrocompatible con DirectX 11
- **RNF-5.3** Soporta resoluciones de 1280×720 hasta 3840×2160 y formatos ultrapanorámicos
- **RNF-5.4** Compatible con mando de Xbox y PlayStation vía Steam Input

#### Localización
- **RNF-6.1** Todo texto visible proviene de tablas de localización (ningún texto en código)
- **RNF-6.2** El juego se distribuye en **español e inglés** en v1.0
- **RNF-6.3** El idioma se detecta desde la configuración de Steam y es modificable

---

## 2️⃣ ADMINISTRADOR / LIVEOPS

### 📌 REQUERIMIENTOS FUNCIONALES

#### Telemetría y análisis
- **RF-13.1** 🔴 El sistema registra eventos de partida (eliminaciones, armas usadas, duración, mapa)
- **RF-13.2** 🔴 El administrador puede consultar métricas de uso y balance por arma
- **RF-13.3** 🟡 El administrador puede consultar tasa de retención y jugadores concurrentes
- **RF-13.4** 🟡 El sistema registra errores y caídas del cliente para diagnóstico
- **RF-13.5** 🟡 El administrador puede identificar mapas o modos con abandono anormal

#### Balance y configuración remota
- **RF-14.1** 🟡 El administrador puede modificar valores de balance de armas sin publicar una actualización del cliente
- **RF-14.2** 🟡 El administrador puede activar o desactivar modos de juego remotamente
- **RF-14.3** 🟢 El administrador puede rotar el conjunto de mapas disponibles

#### Moderación
- **RF-15.1** 🔴 El administrador puede revisar reportes de jugadores
- **RF-15.2** 🔴 El administrador puede banear una cuenta de forma temporal o permanente
- **RF-15.3** 🟡 El sistema banea automáticamente las cuentas detectadas por el anti-trampas
- **RF-15.4** 🟡 El administrador puede consultar el historial de sanciones de una cuenta

#### Operación del servicio
- **RF-16.1** 🔴 El administrador puede consultar el estado de los servidores por región
- **RF-16.2** 🟡 El administrador puede escalar la cantidad de servidores disponibles
- **RF-16.3** 🟡 El administrador puede publicar un mensaje de mantenimiento visible en el menú
- **RF-16.4** 🟢 El administrador puede gestionar temporadas y recompensas

### 🔒 REQUERIMIENTOS NO FUNCIONALES — ADMINISTRADOR

#### Escalabilidad
- **RNF-7.1** La infraestructura soporta **500 jugadores concurrentes** sin degradación
- **RNF-7.2** La infraestructura escala automáticamente ante picos de demanda
- **RNF-7.3** Una instancia de servidor dedicado soporta una partida de 10 jugadores usando menos de **1 GB** de RAM

#### Disponibilidad
- **RNF-8.1** Disponibilidad del servicio online del **99 %** mensual
- **RNF-8.2** Si el backend de progresión cae, el jugador puede seguir jugando (degradación elegante)
- **RNF-8.3** El progreso no se pierde ante caída del servidor: se sincroniza al reconectar
- **RNF-8.4** Los despliegues no interrumpen partidas en curso (las instancias terminan su partida)

#### Mantenibilidad
- **RNF-9.1** El código sigue la convención de nomenclatura de Unreal Engine
- **RNF-9.2** La lógica de juego reside en C++; los Blueprints solo componen y configuran
- **RNF-9.3** Ningún valor de balance está hardcodeado: todo en DataAssets o DataTables
- **RNF-9.4** Los binarios se versionan con Git LFS
- **RNF-9.5** Cada cambio en `main` genera una build automática verificable
- **RNF-9.6** El proyecto compila desde cero en un entorno limpio siguiendo solo el README

#### Legal y cumplimiento
- **RNF-10.1** El juego cumple los requisitos de publicación de Steam (Steamworks)
- **RNF-10.2** Todo asset de terceros cuenta con licencia comercial documentada
- **RNF-10.3** El juego incluye EULA y política de privacidad accesibles
- **RNF-10.4** El tratamiento de datos cumple con GDPR para jugadores europeos
- **RNF-10.5** Se declara el uso de Unreal Engine según los términos de licencia de Epic

---

## 📊 MATRIZ DE ACCESO

| Funcionalidad | Jugador | Anfitrión privado | Administrador |
|---------------|:-------:|:-----------------:|:-------------:|
| Jugar partidas públicas | ✅ | ✅ | ✅ |
| Configurar partida privada | ❌ | ✅ | ✅ |
| Expulsar de partida privada | ❌ | ✅ | ✅ |
| Ver telemetría global | ❌ | ❌ | ✅ |
| Modificar balance | ❌ | ❌ | ✅ |
| Banear cuentas | ❌ | ❌ | ✅ |
| Gestionar servidores | ❌ | ❌ | ✅ |
| Reportar jugadores | ✅ | ✅ | ✅ |

---

## 🧪 CRITERIOS DE ACEPTACIÓN CRÍTICOS

Requerimientos que se validan con una prueba explícita antes de considerar el
juego lanzable:

| ID | Prueba | Criterio de aprobación |
|----|--------|------------------------|
| **CA-1** | 10 jugadores en una partida durante 8 min | Sin caídas, sin desincronización, 60 Hz estable |
| **CA-2** | Cliente con 150 ms de latencia simulada | Los disparos registran donde el jugador apuntó |
| **CA-3** | Cliente modificado intentando enviar daño falso | El servidor rechaza y registra el intento |
| **CA-4** | Partida completa en hardware mínimo | Nunca baja de 60 FPS |
| **CA-5** | Desconexión forzada a mitad de partida | Reconexión exitosa conservando puntaje |
| **CA-6** | Jugador nuevo sin instrucciones | Está disparando en menos de 60 s |
| **CA-7** | 20 partidas simultáneas | Sin degradación de rendimiento del servicio |

---

## ✅ CHECKLIST DE IMPLEMENTACIÓN

### Núcleo jugable
- [ ] Movimiento en primera persona con replicación de red
- [ ] Sistema de armas con datos configurables
- [ ] Disparo autoritativo con compensación de latencia
- [ ] Sistema de daño, muerte y reaparición
- [ ] HUD completo en partida

### Multijugador
- [ ] Build de servidor dedicado funcionando
- [ ] Sesiones y matchmaking vía EOS
- [ ] Reconexión a partida en curso
- [ ] Bots de relleno
- [ ] Anti-trampas integrado

### Contenido
- [ ] 8 armas balanceadas
- [ ] 4 mapas terminados
- [ ] 2 modos obligatorios + 1 opcional
- [ ] Sistema de progresión y desbloqueos

### Producto
- [ ] Menús y configuración completos
- [ ] Localización español/inglés
- [ ] Telemetría y moderación
- [ ] Página de Steam y materiales de lanzamiento
- [ ] EULA, privacidad y licencias verificadas
