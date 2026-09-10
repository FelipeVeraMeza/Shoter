# 🏗️ ARQUITECTURA TÉCNICA — ShooterGame

> Unreal Engine 5.3+ · C++ · Multijugador con servidor autoritativo

---

## ⚠️ DECISIONES IRREVERSIBLES

Estas cuatro decisiones deben tomarse **antes** de escribir más código. Cambiarlas
después implica reescribir el juego completo.

| # | Decisión | Elección | Por qué no se puede postergar |
|---|----------|----------|-------------------------------|
| **D1** | Modelo de red | **Servidor dedicado autoritativo** desde el día 1 | Convertir un juego single-player a multijugador es una reescritura total. Todo el código nace pensando en autoridad y replicación. |
| **D2** | Control de versiones de binarios | **Git LFS** (configurado en `.gitattributes`) | Si se suben `.uasset` sin LFS, el historial se corrompe con archivos enormes y limpiarlo obliga a reescribir el historial. |
| **D3** | Lógica en C++, no en Blueprint | **C++ para sistemas, Blueprint solo para composición** | Un sistema de armas en red hecho en Blueprint es imposible de depurar y de optimizar. Migrarlo después cuesta más que hacerlo bien. |
| **D4** | Iluminación | **Iluminación estática precalculada** (no Lumen) | Lumen cuesta ~30–40 % de FPS. Un FPS competitivo necesita 144+ FPS. Rehacer la iluminación de 4 mapas al final del proyecto es semanas perdidas. |

---

## 1️⃣ MODELO DE RED

### 1.1 Topología

```
          ┌──────────────────────────────┐
          │   SERVIDOR DEDICADO (UE5)    │
          │   Autoridad total            │
          │   60 Hz · sin renderizado    │
          └──────────────────────────────┘
             ▲        ▲        ▲       ▲
             │        │        │       │
         ┌───┴──┐ ┌───┴──┐ ┌───┴──┐ ┌──┴───┐
         │Cliente│ │Cliente│ │Cliente│ │Cliente│
         │  1    │ │  2   │ │  3   │ │  N   │
         └───────┘ └──────┘ └──────┘ └──────┘
```

**Regla fundamental:** el cliente **propone**, el servidor **dispone**. El cliente
nunca envía "maté a X" — envía "disparé desde aquí hacia allá" y el servidor decide
qué pasó.

### 1.2 Reparto de autoridad

| Sistema | Quién decide | Qué hace el cliente |
|---------|--------------|---------------------|
| Movimiento | Servidor valida | Predice localmente (`CharacterMovementComponent` ya lo hace) |
| Disparo | Servidor calcula el impacto | Predice efectos visuales (fogonazo, sonido, casquillo) |
| Daño y salud | **Solo servidor** | Recibe el resultado replicado |
| Munición | **Solo servidor** | Predice para que el HUD responda al instante |
| Puntaje | **Solo servidor** | Solo muestra |
| Reglas de partida | **Solo servidor** | Solo muestra |

### 1.3 Compensación de latencia (lag compensation)

Sin esto, un jugador con 100 ms de ping tiene que apuntar *adelante* del enemigo, y
el juego se siente roto.

**Mecanismo:** el servidor guarda un historial de ~1 segundo con las posiciones de
las cajas de colisión de cada jugador. Al recibir un disparo:

1. Calcula el tiempo del cliente: `latencia / 2 + interpolación`
2. **Rebobina** las cajas de colisión de todos los jugadores a ese instante
3. Ejecuta el trazado (line trace) contra esas posiciones pasadas
4. Restaura las posiciones actuales
5. Aplica el daño si hubo impacto

Este componente (`ULagCompensationComponent`) es el más delicado del proyecto. Se
implementa en la Fase 1, nunca después.

### 1.4 RPCs del sistema de disparo

```cpp
// Cliente → Servidor: petición de disparo. Con validación.
UFUNCTION(Server, Reliable, WithValidation)
void Server_Fire(const FVector_NetQuantize& TraceStart,
                 const FVector_NetQuantizeNormal& TraceDirection,
                 float ClientFireTime);

// Servidor → Todos: efectos cosméticos (no afectan reglas).
UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayFireEffects();

// Replicación de estado
UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo)
int32 CurrentAmmo;
```

**Criterio:** lo que afecta las reglas viaja `Reliable`. Lo cosmético viaja
`Unreliable` (si se pierde un fogonazo, no pasa nada; si se pierde una muerte, sí).

---

## 2️⃣ ESTRUCTURA DE CLASES

```
AShooterGameMode          (solo servidor) reglas, victoria, reaparición
   └── AShooterGameState   (replicado a todos) puntaje, tiempo, estado
        └── AShooterPlayerState  (por jugador) nombre, kills, muertes, XP

AShooterPlayerController  (uno por jugador)
   └── AShooterCharacter   (el pawn replicado)
        ├── UCameraComponent            cámara en primera persona
        ├── UShooterMovementComponent   movimiento con predicción
        ├── UHealthComponent            salud y muerte (autoridad servidor)
        └── UInventoryComponent         armas equipadas
             └── AWeaponBase            arma como actor replicado
                  └── UWeaponDataAsset  configuración (daño, RPM, retroceso)

ULagCompensationComponent (en el GameMode) historial de colisiones
AShooterHUD / UMG          interfaz
```

### 2.1 Responsabilidad de cada clase

| Clase | Responsabilidad | Existe en |
|-------|-----------------|-----------|
| `AShooterGameMode` | Reglas, condición de victoria, decidir dónde reaparecer | Solo servidor |
| `AShooterGameState` | Estado compartido de la partida | Servidor + todos los clientes |
| `AShooterPlayerState` | Datos persistentes del jugador en la partida | Servidor + todos |
| `AShooterPlayerController` | Entrada del jugador, HUD, comunicación con servidor | Servidor + su dueño |
| `AShooterCharacter` | Representación física y visual del jugador | Todos (con relevancia) |
| `AWeaponBase` | Comportamiento del arma | Todos |
| `UWeaponDataAsset` | **Datos**, sin lógica | Contenido, no replicado |

> **Regla de oro:** si preguntas "¿dónde pongo esta lógica?", la respuesta es la
> clase con menos alcance de red que pueda resolverlo.

---

## 3️⃣ DATOS Y BALANCE

Ningún número de balance vive en C++. Todo en `UPrimaryDataAsset`:

```cpp
UCLASS(BlueprintType)
class UWeaponDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Category="Balance") float Damage = 25.f;
    UPROPERTY(EditDefaultsOnly, Category="Balance") float HeadshotMultiplier = 1.75f;
    UPROPERTY(EditDefaultsOnly, Category="Balance") float RoundsPerMinute = 600.f;
    UPROPERTY(EditDefaultsOnly, Category="Balance") int32 MagazineSize = 30;
    UPROPERTY(EditDefaultsOnly, Category="Balance") float ReloadTime = 2.1f;
    UPROPERTY(EditDefaultsOnly, Category="Balance") UCurveFloat* RecoilVertical;
    UPROPERTY(EditDefaultsOnly, Category="Balance") UCurveFloat* DamageFalloff;
    UPROPERTY(EditDefaultsOnly, Category="Visual")  USkeletalMesh* WeaponMesh;
};
```

**Ventaja concreta:** balancear un arma es editar un asset, no recompilar. Y
habilita el requerimiento RF-14.1 (balance remoto sin actualizar el cliente).

---

## 4️⃣ SERVICIOS ONLINE

| Necesidad | Solución | Costo |
|-----------|----------|-------|
| Identidad y amigos | **Steam** (Online Subsystem Steam) | Gratis |
| Sesiones y matchmaking | **Epic Online Services (EOS)** | **Gratis, sin límite** |
| Anti-trampas | **Easy Anti-Cheat** (incluido en EOS) | Gratis |
| Voz | EOS Voice | Gratis |
| Hosting de servidores | Edgegap / Hathora (contenedores por región) | ~USD 200–800/mes |
| Distribución | Steam | USD 100 + 30 % de ingresos |

**Por qué EOS y no una solución propia:** es gratuito, incluye anti-trampas
certificado, y no exige mantener infraestructura de sesiones. Construir esto a mano
son meses de trabajo y un riesgo de seguridad enorme.

---

## 5️⃣ ESTRUCTURA DEL PROYECTO

```
ShooterGame/
├── Config/                  configuración .ini
├── Content/                 assets (Git LFS)
│   ├── Characters/
│   ├── Weapons/
│   │   └── Data/            UWeaponDataAsset por arma
│   ├── Maps/
│   ├── UI/
│   └── Audio/
├── Source/ShooterGame/
│   ├── Core/                GameMode, GameState, PlayerState
│   ├── Player/              Character, Controller, Movement
│   ├── Weapons/             WeaponBase, Inventory, DataAssets
│   ├── Combat/              Health, Damage, LagCompensation
│   ├── UI/                  HUD y widgets
│   └── Online/              sesiones, matchmaking
└── docs/                    esta documentación
```

**Organización por *feature*, no por tipo.** Todo lo de armas junto, en lugar de
"todos los .h por un lado". Con 40+ clases, la diferencia es enorme.

---

## 6️⃣ PRESUPUESTO DE RENDIMIENTO

Objetivo: **16.6 ms por frame** (60 FPS) en hardware mínimo. Se mide desde la Fase 1,
no al final.

| Sistema | Presupuesto |
|---------|-------------|
| Renderizado | 8.0 ms |
| Lógica de juego | 3.0 ms |
| Física | 1.5 ms |
| Animación | 2.0 ms |
| Red | 0.5 ms |
| Audio + UI | 1.6 ms |

**Decisiones derivadas:**
- ❌ **Lumen desactivado** — iluminación precalculada (ver D4)
- ⚠️ **Nanite con criterio** — sirve para entornos, no para personajes animados
- ✅ **LODs obligatorios** en todo mesh
- ✅ **Significance Manager** para reducir el costo de personajes lejanos
- ✅ **Relevancia de red** — no replicar lo que el cliente no puede ver (también es
  una medida anti-trampas: RNF-3.6)

---

## 7️⃣ PIPELINE DE BUILD

```
Commit en main
   ↓
GitHub Actions (runner propio con Windows + UE5)
   ↓
Compilar: Cliente (Win64) + Servidor Dedicado (Win64/Linux)
   ↓
Pruebas automatizadas (Gauntlet)
   ↓
Cliente → Steam (rama beta)   Servidor → imagen de contenedor
```

> ⚠️ Los runners de GitHub en la nube **no sirven** para Unreal: el motor no cabe en
> ellos. Se necesita un runner propio (una PC con UE5 instalado). Presupuestar esto
> desde la Fase 0.

---

## 8️⃣ ESTADO ACTUAL vs. ARQUITECTURA OBJETIVO

Lo que existe hoy en el repositorio es un **prototipo single-player**, útil solo
como punto de partida:

| Componente | Hoy | Objetivo | Acción |
|------------|-----|----------|--------|
| `AShooterCharacter` | Movimiento + disparo local | Replicado con predicción | Reescribir con red |
| Disparo | Trazado local directo | RPC + compensación de latencia | Reescribir |
| `ATargetActor` | Cubo que se destruye | Reemplazado por jugadores reales | Conservar para pruebas |
| `AShooterGameMode` | Solo define el pawn | Reglas completas de partida | Ampliar |
| Salud | Inexistente | `UHealthComponent` autoritativo | Crear |
| Datos de armas | Valores en C++ | DataAssets | Migrar |

**Importante:** el prototipo actual sirve para validar la *sensación* del disparo
(pilar 1 del GDD). Esa validación es real y valiosa. Pero el código de red se
construye desde cero en la Fase 1 — no se "parcha" el prototipo.
