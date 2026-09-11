"""
Vuelca a un archivo qué expone la API de Python de este motor.

Sin un stub generado no hay forma de comprobar los nombres desde fuera del
editor, y adivinarlos lleva a fallar uno a uno. Esto los recoge de una vez.

Ejecutar en la consola Python del editor:
    exec(open(r"<proyecto>/Content/Python/diagnostico.py").read())

Escribe el informe en Saved/diagnostico_api.txt
"""

import unreal
import os

OUT_PATH = os.path.join(
    unreal.Paths.project_saved_dir().replace("/", os.sep), "diagnostico_api.txt"
)

lines = []


def add(text=""):
    lines.append(str(text))


add("=== VERSION ===")
add(unreal.SystemLibrary.get_engine_version())
add()

add("=== FACTORIES DISPONIBLES ===")
for name in sorted(n for n in dir(unreal) if "Factory" in n):
    add("  " + name)
add()

add("=== CLASES DE INPUT ===")
for name in sorted(n for n in dir(unreal) if "Input" in n or n == "Key"):
    add("  " + name)
add()

add("=== SUBSISTEMAS DE EDITOR ===")
for name in sorted(n for n in dir(unreal) if "Subsystem" in n and "Editor" in n):
    add("  " + name)
add()

add("=== CLASES DEL PROYECTO (modulo Shoter) ===")
for name in (
    "ShooterCharacter", "ShooterGameMode", "ShooterGameState",
    "ShooterPlayerController", "ShooterPlayerState", "ShooterHUD",
    "TargetActor", "WeaponBase", "WeaponDataAsset",
    "HealthComponent", "InventoryComponent", "LagCompensationComponent",
):
    add("  {:28} {}".format(name, "OK" if hasattr(unreal, name) else "FALTA"))
add()

add("=== FIRMA DE create_asset ===")
try:
    add(unreal.AssetTools.create_asset.__doc__)
except Exception as error:
    add("error: {}".format(error))
add()

add("=== unreal.Key: como se construye ===")
try:
    key = unreal.Key("R")
    add("  unreal.Key('R') -> OK, key_name = {}".format(key.key_name))
except Exception as error:
    add("  unreal.Key('R') -> FALLA: {}".format(error))

try:
    key = unreal.Key()
    key.set_editor_property("key_name", "R")
    add("  unreal.Key() + set_editor_property -> OK")
except Exception as error:
    add("  unreal.Key() + set_editor_property -> FALLA: {}".format(error))
add()

add("=== EnhancedActionKeyMapping: propiedades ===")
try:
    mapping = unreal.EnhancedActionKeyMapping()
    add("  instancia creada OK")
except Exception as error:
    add("  FALLA al instanciar: {}".format(error))
add()

add("=== ShooterCharacter: propiedades de input ===")
try:
    cdo = unreal.get_default_object(unreal.ShooterCharacter)
    for prop in (
        "move_action", "look_action", "jump_action", "fire_action",
        "aim_action", "reload_action", "sprint_action", "crouch_action",
        "switch_weapon_action", "default_mapping_contexts",
    ):
        try:
            cdo.get_editor_property(prop)
            add("  {:28} OK".format(prop))
        except Exception:
            add("  {:28} NO EXISTE".format(prop))
except Exception as error:
    add("  error: {}".format(error))
add()

add("=== ShooterGameMode: propiedades ===")
try:
    cdo = unreal.get_default_object(unreal.ShooterGameMode)
    for prop in ("endless_match", "team_based", "respawn_delay", "default_pawn_class"):
        try:
            cdo.get_editor_property(prop)
            add("  {:28} OK".format(prop))
        except Exception:
            add("  {:28} NO EXISTE".format(prop))
except Exception as error:
    add("  error: {}".format(error))

report = "\n".join(lines)

with open(OUT_PATH, "w", encoding="utf-8") as handle:
    handle.write(report)

unreal.log("[diagnostico] informe escrito en: {}".format(OUT_PATH))
unreal.log("[diagnostico] " + report.replace("\n", " | "))
