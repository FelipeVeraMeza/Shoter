# 🎯 Zona de entrenamiento

Nivel de pruebas para validar el combate antes de construir los mapas reales.
Se genera por script: no hay que colocar nada a mano.

---

## Cómo generarla

**Con Unreal cerrado**, compila el proyecto (o deja que el editor lo haga al abrir).
Después, dentro del editor:

1. Barra inferior → cambiar **Cmd** por **Python**
2. Pegar:

```python
exec(open(r"C:\Users\felip\OneDrive\Documentos\VS\Shoter\Content\Python\build_training_zone.py").read())
```

El script es idempotente: reejecutarlo reutiliza lo que ya exista en vez de duplicarlo.

---

## Qué crea

### Assets

| Asset | Ruta | Para qué |
|---|---|---|
| `IA_Fire`, `IA_Aim`, `IA_Reload`, `IA_Sprint`, `IA_Crouch`, `IA_SwitchWeapon` | `Content/Input/Actions` | Acciones de combate |
| `IMC_Combate` | `Content/Input` | Mapea esas acciones a teclas |
| `DA_Rifle` | `Content/Weapons` | Balance del arma (RNF-9.3) |
| `BP_Rifle` | `Content/Blueprints/Weapons` | Actor del arma |
| `BP_Jugador` | `Content/Blueprints/Core` | Personaje, ya con input y arma |
| `BP_ModoJuego` | `Content/Blueprints/Core` | Reglas de entrenamiento |
| `Lvl_Entrenamiento` | `Content/Maps` | El nivel |

### El nivel

```
        ┌─────────────── 60 m ───────────────┐
        │  ·  ·  ·  ·   blancos 28 m         │
        │  ·  ·  ·  ·   blancos 20 m         │
        │  ·  ·  ·  ·   blancos 12 m         │
        │      ▓   ▓     coberturas          │
        │           ▓                        │
        │                                    │
        │  ▲                          ▲      │
        │        apariciones                 │
        │  ▲                          ▲      │
        └────────────────────────────────────┘
```

- **Suelo** de 60 × 60 m con muro perimetral
- **12 blancos** en tres filas (12 m, 20 m, 28 m) para ver la caída de daño
- **3 coberturas** para practicar asomarse
- **4 puntos de aparición** en las esquinas, separados para que el sistema de
  reaparición segura tenga dónde elegir
- Luz direccional, luz ambiente y atmósfera

---

## Reglas del modo entrenamiento

El `BP_ModoJuego` se configura distinto al de partida competitiva:

| Ajuste | Entrenamiento | Partida normal |
|---|---|---|
| `bEndlessMatch` | ✅ activo | ❌ |
| Duración | sin límite | 8 min |
| Equipos | no | sí |
| Reaparición | 2 s | 4 s |

Con `bEndlessMatch` el HUD muestra `ENTRENAMIENTO` en vez del cronómetro, y la
sesión no termina sola.

---

## Los blancos

`ATargetActor` ([Source/Shoter/Combat/TargetActor.h](../Source/Shoter/Combat/TargetActor.h)):

- 100 de vida, ajustable por instancia
- Vira de azul a rojo según el daño recibido
- Al caer, reaparece en 3 s — la galería nunca se vacía
- El servidor decide el daño; los clientes solo ven el resultado (RNF-3.1)

Con `RespawnDelay = 0` el blanco desaparece definitivamente.

---

## Qué probar

| Qué | Cómo |
|---|---|
| Caída de daño (RF-5.8) | Dispara a las tres filas: los lejanos aguantan más |
| Dispersión (RF-5.7) | Mantén el disparo: la mira se abre |
| ADS (RF-5.3) | Clic derecho: la dispersión baja |
| Retroceso (RF-5.6) | Ráfaga larga: el arma sube |
| Hitmarker (RF-7.5) | Cada impacto confirmado |
| Recarga (RF-5.4) | `R`, o vacía el cargador |

### Multijugador

`Play → Net Mode: Play As Client`, *Number of Players*: 2.

Para validar la compensación de latencia (RNF-2.4), en la consola del cliente:

```
Net PktLag=150
```

Con 150 ms simulados los disparos deben seguir registrando donde apuntas. Si
tienes que adelantarte al enemigo para acertar, la compensación no funciona.
