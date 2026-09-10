# ShooterGame

Simulador de disparos en primera persona, hecho en Unreal Engine 5 (C++).

Versión inicial simple:
- Personaje en primera persona: moverse (WASD) y mirar (mouse).
- Disparo (click izquierdo) por raycast desde la cámara.
- Un blanco (`ATargetActor`) que recibe daño y se destruye al llegar a 0 de vida.

Sin texturas/materiales ni sonido todavía — eso viene en el siguiente paso.

## Requisitos

- Unreal Engine 5.3 o superior.
- Windows: Visual Studio 2022 con el workload "Game development with C++".
- Mac: Xcode.

## Cómo abrirlo

1. Clona el repo.
2. Click derecho sobre `ShooterGame.uproject` → "Generate Visual Studio project files" (Windows) o simplemente ábrelo con doble click y deja que el Editor lo compile.
3. Abre el proyecto en Unreal Editor.
4. Crea o abre un nivel (File → New Level → vacío está bien), agrega un `Player Start` y un `ATargetActor` (Place Actors → busca "Target Actor").
5. Verifica en Project Settings → Maps & Modes que el GameMode sea `ShooterGameMode` (ya viene configurado por defecto vía `Config/DefaultEngine.ini`).
6. Presiona Play.

## Controles

- `W A S D`: moverse
- Mouse: mirar
- Click izquierdo: disparar
- Espacio: saltar

## Estructura del código

- `Source/ShooterGame/ShooterCharacter.*`: el personaje jugable (movimiento, cámara, disparo).
- `Source/ShooterGame/TargetActor.*`: el blanco que recibe daño.
- `Source/ShooterGame/ShooterGameMode.*`: define el pawn por defecto.
