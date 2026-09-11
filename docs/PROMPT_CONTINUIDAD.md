# Prompt para continuar el proyecto en otra sesión

Copia todo lo que hay entre las líneas y pégalo como primer mensaje.

---

## PROMPT

Estoy desarrollando **Shoter**, un FPS arcade multijugador en Unreal Engine 5.8
con C++. El proyecto está en `C:\Users\felip\OneDrive\Documentos\VS\Shoter` y en
https://github.com/FelipeVeraMeza/Shoter

**Lee primero estos documentos del repositorio**, en este orden:

1. `docs/REQUERIMIENTOS.md` — los 152 requerimientos (100 RF + 52 RNF)
2. `docs/TRAZABILIDAD.md` — cuáles están hechos y cuáles no, contra el código
3. `docs/SISTEMAS.md` — qué hace cada subsistema y cómo usarlo
4. `docs/BITACORA.md` — decisiones tomadas y errores ya resueltos; **léelo antes
   de escribir scripts de Python para el editor**, te ahorra repetir fallos

### Estado actual

**49 de 100 requerimientos funcionales, 40 de 58 Must.** Todo compila en UE 5.8.

Implementado en C++ (`Source/Shoter`, 22 clases):

- **Combate**: servidor autoritativo, compensación de latencia con rebobinado,
  armas por DataAsset, dispersión, retroceso, caída de daño, headshots
- **Personaje**: WASD, cámara, salto, agacharse, correr, deslizamiento, granadas
- **Salud**: regeneración, muerte, reaparición en punto seguro, invencibilidad
- **HUD por Canvas**: mira dinámica, munición, salud, killfeed, tablas de
  posiciones, indicador de daño, pantalla de resultados
- **Progresión**: XP, niveles, desbloqueos, estadísticas, guardado local
- **Ajustes**: sensibilidad, calidad, volúmenes, VSync, resolución, daltonismo
- **Menú**: navegación entre 6 pantallas, pausa, abandonar partida
- **Bots**: percepción visual, patrulla, combate a distancia
- **Telemetría**: eventos en JSON Lines
- **Entrenamiento**: drones con chispas, gestor de reinicio

### Tres dependencias externas bloquean lo que falta

No se resuelven escribiendo código; no las propongas como si fueran tareas:

1. **Steamworks SDK** → RF-1 (cuenta), RF-12 (social)
2. **Backend de progresión** → RF-14 a RF-16, RNF-3.4
3. **Servidor dedicado** → RF-3 (matchmaking). El UE instalado desde el
   launcher **no puede compilarlo**: *"Server targets are not currently
   supported from this engine distribution"*. Requiere Unreal desde el código
   fuente de GitHub

### Cómo trabajar en este proyecto

**Compilar** (el editor debe estar CERRADO; Live Coding lo bloquea):

```powershell
& "D:\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
    ShoterEditor Win64 Development `
    -Project="C:\Users\felip\OneDrive\Documentos\VS\Shoter\Shoter.uproject" -WaitMutex
```

**Diagnosticar fallos**: el registro está en `Saved/Logs/Shoter.log`. Los
scripts escriben con prefijo: `[entrenamiento]`, `[camara]`, `[arma]`,
`[personaje]`, `[hangar]`.

**Scripts de editor** en `Content/Python`, ejecutables desde la consola Python
del editor:

```python
exec(open(r"C:\Users\felip\OneDrive\Documentos\VS\Shoter\Content\Python\<script>.py").read())
```

| Script | Para qué |
|---|---|
| `diagnostico.py` | Vuelca la API real del motor. **Ejecútalo primero ante cualquier fallo de script** |
| `build_training_zone.py` | Crea assets y el nivel de entrenamiento |
| `build_sci_fi_range.py` | Hangar futurista con paneles luminosos |
| `build_menu.py` | Los seis widgets del menú |
| `fix_camara.py`, `fix_movimiento.py`, `fix_arma.py`, `fix_personaje.py` | Correcciones puntuales |
| `fix_blueprints.py` | Reasigna valores a los Blueprints **y los recompila** |

### Cuatro trampas que ya me costaron tiempo

1. **La API de Python cambia entre versiones de UE.** No adivines nombres:
   ejecuta `diagnostico.py` y lee el informe. En 5.8: `InputAction_Factory`
   lleva guion bajo, `unreal.Key()` no acepta argumentos,
   `default_key_mappings` es una estructura que contiene la lista, y
   `generated_class` es un método, no una propiedad.

2. **Asignar valores al CDO de un Blueprint no basta.** Sin recompilarlo, el
   juego sigue usando los anteriores. Falla en silencio.

3. **La geometría `STATIC` exige construir la iluminación**, o el nivel sale
   oscuro sin dar error. Usa `MOVABLE`.

4. **Live Coding bloquea la compilación** con el editor abierto.

### Cómo quiero que trabajes

- **Verifica antes de afirmar.** Compila y lee el log; no digas que algo
  funciona sin comprobarlo
- **Di lo que no puedes hacer.** Si algo depende de assets de pago, de Steam o
  de un backend, dilo en lugar de rodearlo
- **Comentarios en el código**: explica el *porqué* de una decisión, no lo que
  el código ya dice
- **Commits a mi nombre** (Felipe), sin coautoría
- **Respóndeme en español**

### Qué falta, por orden de coste

Sin dependencias externas:

1. **Widgets del menú** — la lógica está completa en `ShooterMenuSubsystem`;
   falta colocar botones en UMG, que es trabajo de editor
2. **Sound Classes** — los volúmenes se guardan pero no enrutan a nada
3. **Arte** — drones, armas y escenario. El código ya expone las mallas y
   efectos como propiedades del Blueprint: sustituirlos no requiere tocar C++
4. **Modos Dominación y Oleadas** (RF-8.3, RF-8.4)
5. **Accesorios de arma** (RF-5.10, RF-9.4)

### Lo primero que quiero hacer

[ESCRIBE AQUÍ LO QUE QUIERAS ABORDAR]

---

## Fin del prompt

### Nota sobre el estado del juego al cerrar esta sesión

Quedó un bug sin confirmar: **al pulsar WASD el personaje se movía siempre en la
misma dirección**. La causa probable es que `IA_Move` no sea de tipo `Axis2D`,
con lo que `Get<FVector2D>()` no devuelve lo que el C++ espera.

`Content/Python/fix_movimiento.py` comprueba eso y los modificadores de cada
tecla, los corrige y deja en el log lo que encontró. **Ejecútalo y verifica el
resultado antes de seguir con otra cosa.**
