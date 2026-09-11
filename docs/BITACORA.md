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
| `build_menu.py` | Crea los seis widgets del menú y `Lvl_Menu` |
| `fix_blueprints.py` | Reasigna valores a los Blueprints **y los recompila** |
| `fix_camara.py` | Mapea ratón y teclado en `IMC_Combate` |
| `fix_arma.py` | Asigna malla y nombre a `DA_Rifle` |
| `fix_personaje.py` | Asigna maniquí y animaciones a `BP_Jugador` |

---

## Estado al cierre

**49 de 100 requerimientos funcionales, 40 de 58 Must.** Detalle en
[TRAZABILIDAD.md](TRAZABILIDAD.md).

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

- **Widgets del menú**: la navegación está completa en C++; falta colocar los
  botones en UMG, que es trabajo de editor
- **Sound Classes**: los volúmenes se guardan pero no enrutan a nada
- **Localización (RNF-6)**: los textos del HUD están en código, no en tablas
- **Bots (RF-3.6, RF-8.4)**: requiere árboles de comportamiento

---

## Cómo retomar el proyecto

1. Abrir `Shoter.uproject` (compila el C++ al arrancar)
2. Abrir `Content/Maps/Lvl_Entrenamiento`
3. Play

Si algo falla, el registro de Unreal (`Saved/Logs/Shoter.log`) dice qué. Los
scripts escriben con el prefijo de su nombre: `[entrenamiento]`, `[camara]`,
`[arma]`, `[personaje]`.
