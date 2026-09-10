# 📅 PLANIFICACIÓN DEL PROYECTO — ShooterGame

> Horizonte: **9 meses** (Sep 2026 → May 2027) · Sprints de 2 semanas · 18 sprints

---

## 🎯 OBJETIVO DEL PROYECTO

Lanzar en Steam un FPS arcade multijugador comercial, con 4 mapas, 8 armas y 3
modos de juego, que sostenga 60 FPS en hardware de gama media y soporte 500
jugadores concurrentes.

---

## 1️⃣ FASES

| Fase | Meses | Objetivo | Entregable que lo demuestra |
|------|-------|----------|-----------------------------|
| **F0 · Preproducción** | Mes 1 | Decisiones técnicas cerradas y pipeline funcionando | Build automática desde `main` |
| **F1 · Rebanada vertical** | Mes 2–3 | Una porción del juego final, completa y pulida | 8 jugadores en línea, 1 mapa, 2 armas, 1 modo |
| **F2 · Producción** | Mes 4–7 | Todo el contenido y los sistemas | Juego completo, sin pulir |
| **F3 · Beta** | Mes 8 | Validación con jugadores reales | Beta abierta estable con anti-trampas |
| **F4 · Lanzamiento** | Mes 9 | Publicación en Steam | Versión 1.0 publicada |
| **F5 · Post-lanzamiento** | Continuo | Retención y correcciones | Temporada 1 |

### 🔑 La regla de la rebanada vertical (F1)

La Fase 1 no entrega "el 20 % de cada sistema": entrega **una parte del juego final
terminada al 100 %**, con calidad de lanzamiento. Un mapa, dos armas, un modo — pero
con red, audio, efectos y HUD completos.

Sirve para responder la única pregunta que importa a esta altura: **¿es divertido?**
Si al final del mes 3 el juego no es divertido con un mapa y dos armas, no lo será
con cuatro mapas y ocho armas. Es el punto de decisión para continuar, corregir el
rumbo o cancelar — antes de gastar 6 meses más.

---

## 2️⃣ CRONOGRAMA POR SPRINTS

### F0 · PREPRODUCCIÓN — Mes 1 (Sep 2026)

| Sprint | Foco | Entregable |
|--------|------|------------|
| **S1** | Fundaciones técnicas | Git LFS operativo · proyecto UE5 con módulos C++ · runner de CI con UE5 instalado |
| **S2** | Prototipo de sensación de disparo | Disparo local con retroceso, audio y efectos. **Validar el pilar 1** antes de invertir en red |

**Hito M0 — fin de mes 1:**
- ✅ Compilación automática de cliente y servidor dedicado
- ✅ El disparo se siente bien (evaluado por 5 personas externas)
- ✅ GDD y requerimientos aprobados y congelados

### F1 · REBANADA VERTICAL — Mes 2–3 (Oct–Nov 2026)

| Sprint | Foco | Entregable |
|--------|------|------------|
| **S3** | Red base | Movimiento replicado con predicción · build de servidor dedicado corriendo |
| **S4** | Combate autoritativo | `Server_Fire` · `UHealthComponent` · muerte y reaparición |
| **S5** | Compensación de latencia | `ULagCompensationComponent` · validación con 150 ms simulados |
| **S6** | Partida completa | Modo Equipos 4v4 · HUD · mapa "Almacén" · 2 armas con DataAssets |

**Hito M1 — fin de mes 3: 🚦 PUNTO DE DECISIÓN**
- ✅ 8 jugadores reales en línea durante 8 minutos sin caídas (CA-1)
- ✅ Los disparos registran correctamente con 150 ms de ping (CA-2)
- ✅ **Veredicto de diversión:** 10 personas externas juegan 30 min. Si no piden
  jugar otra partida, se detiene la producción y se corrige el rumbo.

### F2 · PRODUCCIÓN — Mes 4–7 (Dic 2026 – Mar 2027)

| Sprint | Foco | Entregable |
|--------|------|------------|
| **S7** | Arsenal I | Armas 3–5 · sistema de accesorios |
| **S8** | Arsenal II | Armas 6–8 · balance inicial de TTK |
| **S9** | Mapa 2 · Matchmaking | "Azotea" · sesiones EOS · búsqueda de partida |
| **S10** | Modos | Todos contra todos · Dominación |
| **S11** | Progresión | XP, niveles, desbloqueos, persistencia en servidor |
| **S12** | Mapa 3 · Bots | "Planta" · IA de relleno (RF-3.6) |
| **S13** | Menús y configuración | Menú principal · opciones completas · reasignación de teclas |
| **S14** | Mapa 4 · Social | "Puerto" · grupos de amigos · reportes |

**Hito M2 — fin de mes 7:**
- ✅ Todo el contenido de v1.0 presente y jugable (*feature complete*)
- 🔒 **Congelamiento de alcance:** desde aquí no entra ninguna feature nueva

### F3 · BETA — Mes 8 (Abr 2027)

| Sprint | Foco | Entregable |
|--------|------|------------|
| **S15** | Blindaje | Easy Anti-Cheat · telemetría · moderación · localización |
| **S16** | Beta abierta | Optimización al presupuesto de rendimiento · corrección por prioridad · balance con datos reales |

**Hito M3 — fin de mes 8:**
- ✅ Los 7 criterios de aceptación (CA-1 a CA-7) aprobados
- ✅ Cero fallos críticos abiertos
- ✅ 60 FPS estables en hardware mínimo

### F4 · LANZAMIENTO — Mes 9 (May 2027)

| Sprint | Foco | Entregable |
|--------|------|------------|
| **S17** | Preparación de publicación | Revisión de Steam · EULA y privacidad · tráiler · capturas |
| **S18** | Lanzamiento | Publicación 1.0 · monitoreo en vivo · parche de día 1 |

**Hito M4 — LANZAMIENTO 🚀**

### F5 · POST-LANZAMIENTO — Continuo

- **Semana 1:** corrección de urgencias, monitoreo de servidores, respuesta a reseñas
- **Mes 1–2:** balance con datos reales, mapa gratuito 5
- **Mes 3:** Temporada 1 con cosméticos
- **Evaluación mes 6:** ¿continuar, ampliar o cerrar?

---

## 3️⃣ HITOS Y CRITERIOS DE SALIDA

| Hito | Fecha | Criterio de salida (todo debe cumplirse) |
|------|-------|------------------------------------------|
| **M0** | Fin Sep 2026 | Build automática · disparo validado · documentos congelados |
| **M1** 🚦 | Fin Nov 2026 | 8 jugadores online estables · lag compensation validada · **es divertido** |
| **M2** | Fin Mar 2027 | Contenido completo · alcance congelado |
| **M3** | Fin Abr 2027 | CA-1 a CA-7 aprobados · sin fallos críticos · rendimiento en objetivo |
| **M4** | May 2027 | Publicado en Steam |

---

## 4️⃣ RIESGOS

Ordenados por impacto esperado (probabilidad × severidad).

| ID | Riesgo | Prob. | Impacto | Mitigación |
|----|--------|:-----:|:-------:|------------|
| **R1** | **Crecimiento descontrolado del alcance** — "una feature más" | 🔴 Alta | 🔴 Alto | Priorización MoSCoW estricta · congelamiento en M2 · toda idea nueva va a la lista de v2, sin excepción |
| **R2** | **El netcode resulta más difícil de lo previsto** | 🟡 Media | 🔴 Alto | Se ataca en F1, no al final · si la compensación de latencia no funciona en S5, se detiene todo hasta resolverlo |
| **R3** | **Población insuficiente al lanzar** — nadie encuentra partida | 🔴 Alta | 🔴 Alto | Bots de relleno (RF-3.6) · modo coop PvE · lanzar solo 2 regiones para concentrar jugadores · ≥7.000 listas de deseos antes de lanzar |
| **R4** | **No es divertido** | 🟡 Media | 🔴 Alto | Se descubre en M1 (mes 3), no en el mes 8 · el prototipo de S2 valida la sensación antes de invertir |
| **R5** | **Tramposos arruinan la experiencia** | 🔴 Alta | 🟡 Medio | Servidor autoritativo desde D1 · EAC antes de la beta abierta · relevancia de red anti-wallhack |
| **R6** | **Optimización tardía** | 🟡 Media | 🟡 Medio | Presupuesto de rendimiento medido desde F1 · iluminación estática (D4) |
| **R7** | **Agotamiento del equipo** | 🟡 Media | 🔴 Alto | Sprints con carga realista · cada hito es jugable (la motivación viene de ver el juego funcionando) · sin trabajo extra sostenido |
| **R8** | **Costos de servidores superan lo previsto** | 🟢 Baja | 🟡 Medio | Empezar con servidores por demanda · apagar regiones sin uso · medir costo por partida desde la beta |
| **R9** | **Assets sin licencia comercial válida** | 🟢 Baja | 🔴 Alto | Registro de licencias desde el primer asset · nunca usar material de origen dudoso |
| **R10** | **Git con binarios se vuelve inmanejable** | 🟡 Media | 🟡 Medio | Git LFS configurado antes del primer asset · si el equipo crece a 5+, evaluar Perforce |

---

## 5️⃣ PRESUPUESTO ESTIMADO

### Costos fijos hasta el lanzamiento

| Concepto | Costo (USD) | Nota |
|----------|-------------|------|
| Steam Direct | 100 | Por producto, recuperable con ventas |
| Assets (Fab / Marketplace) | 300 – 1.500 | Personajes, entornos, animaciones |
| Audio y efectos de sonido | 100 – 500 | Bibliotecas con licencia comercial |
| Servidores (beta, 2 meses) | 200 – 600 | Solo desde F3 |
| Herramientas varias | 0 – 300 | Software de arte, utilidades |
| **Total pre-lanzamiento** | **700 – 3.000** | |

### Costos recurrentes post-lanzamiento

| Concepto | Costo mensual (USD) |
|----------|---------------------|
| Servidores dedicados (500 jugadores concurrentes) | 200 – 800 |
| Almacenamiento y telemetría | 20 – 50 |

### Gratis (importante)

- ✅ **Unreal Engine** — sin costo hasta USD 1.000.000 de ingresos brutos por
  producto. Sobre ese monto: 5 % de regalías.
- ✅ **Epic Online Services** — sesiones, matchmaking, voz: gratis, sin límite.
- ✅ **Easy Anti-Cheat** — gratis, incluido en EOS.

### Ingresos

- Steam retiene el **30 %** de cada venta.
- A USD 12,99: quedan ~USD 9,09 por unidad.
- **Punto de equilibrio** con USD 3.000 de costos: **≈330 unidades vendidas**.

---

## 6️⃣ EQUIPO Y ROLES

| Rol | Dedicación | Si el equipo es una sola persona |
|-----|------------|----------------------------------|
| Programación de gameplay y red | Alta, todo el proyecto | Es tu foco principal |
| Diseño de niveles | Media, F2 | 4 mapas ≈ 3 semanas cada uno |
| Arte 3D y efectos | Media | **Comprar assets**, no modelar |
| Audio | Baja | Bibliotecas con licencia |
| Pruebas y balance | Alta en F3 | Reclutar jugadores de prueba externos |
| Marketing y comunidad | Media desde F2 | No delegable — ver §7 |

> **Advertencia realista:** en solitario, este plan de 9 meses supone dedicación
> completa. A tiempo parcial (20 h/semana), el horizonte realista es de **16–18
> meses**. Ajustar el cronograma con honestidad ahora evita el fracaso después.

---

## 7️⃣ MARKETING (empieza en el mes 4, no en el mes 9)

El error más común y más caro de un lanzamiento indie es publicar la página de Steam
la semana del lanzamiento. El algoritmo de Steam premia las **listas de deseos**
acumuladas, y estas necesitan meses.

| Cuándo | Acción | Meta |
|--------|--------|------|
| Mes 4 | Publicar página de Steam ("próximamente") | Empezar a acumular listas de deseos |
| Mes 4–8 | Publicaciones semanales de avance (video corto del juego) | Comunidad inicial |
| Mes 6 | Participar en un festival de Steam (Next Fest) con demo | Pico de visibilidad |
| Mes 8 | Beta abierta con creadores de contenido | Prueba social |
| Mes 9 | Lanzamiento | **≥7.000 listas de deseos** |

**Regla de referencia:** ~7.000 listas de deseos al lanzar suelen traducirse en un
lanzamiento visible en Steam. Por debajo de 2.000, el juego pasa desapercibido sin
importar su calidad.

---

## 8️⃣ MÉTRICAS DE ÉXITO

| Métrica | Mínimo aceptable | Objetivo |
|---------|------------------|----------|
| Listas de deseos al lanzar | 2.000 | 7.000+ |
| Unidades vendidas (mes 1) | 330 (equilibrio) | 2.000 |
| Reseñas positivas | 70 % | 85 % |
| Jugadores concurrentes (semana 1) | 30 | 150 |
| Retención día 1 | 25 % | 40 % |
| Retención día 7 | 8 % | 15 % |
| Tiempo de búsqueda de partida | < 60 s | < 30 s |
| Duración media de sesión | 20 min | 40 min |

---

## 9️⃣ DEFINICIÓN DE TERMINADO

Una tarea no está terminada hasta que:

- [ ] Funciona en **cliente y servidor dedicado** (no solo en el editor)
- [ ] Se probó con **al menos 4 jugadores** en red real
- [ ] Se probó con **150 ms de latencia simulada**
- [ ] No introduce regresiones de rendimiento (dentro del presupuesto de 16.6 ms)
- [ ] Los valores de balance están en DataAssets, no en código
- [ ] Los textos visibles están en tablas de localización
- [ ] La build automática pasa
- [ ] Está fusionada en `main`

---

## 🔟 PRÓXIMOS 3 PASOS INMEDIATOS

Concretos, para empezar esta semana:

1. **Instalar Unreal Engine 5.3+ y compilar el prototipo actual.** Sin esto, nada
   más avanza. Confirma que el entorno funciona.
2. **Ejecutar `git lfs install` y verificar que `.gitattributes` toma efecto** antes
   de subir el primer asset binario. Es la decisión D2 y es irreversible.
3. **Validar el pilar 1 (sprint S2):** hacer que un solo disparo se sienta
   excelente — retroceso, audio, efectos, sacudida de cámara. Mostrárselo a 5
   personas. Ese es el corazón del juego; si eso no funciona, nada de lo demás
   importa.
