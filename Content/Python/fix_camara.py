"""
Mapea ratón y teclado en IMC_Combate.

La plantilla reparte los mapeos entre IMC_Default e IMC_MouseLook, y no hay
garantía de que la cámara esté en el contexto que carga BP_Jugador. Mapear
aquí las teclas hace que el personaje no dependa de cómo esté organizada
la plantilla.

En UE 5.8 la propiedad 'mappings' está obsoleta: el motor lee de
'default_key_mappings'. Escribir en la antigua guarda los datos pero el
juego no los aplica.

Ejecutar en la consola Python del editor:
    exec(open(r"<proyecto>/Content/Python/fix_camara.py").read())
"""

import unreal

INPUT_DIR = "/Game/Input"
ACTIONS_DIR = "/Game/Input/Actions"
BP_JUGADOR = "/Game/Blueprints/Core/BP_Jugador"

# Nombre de la propiedad que el motor lee, de más nueva a más antigua.
MAPPING_PROPERTIES = ("default_key_mappings", "mappings")


def log(message):
    unreal.log("[camara] {}".format(message))


def make_key(key_name):
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    return key


def negate(x=False, y=False, z=False):
    modifier = unreal.InputModifierNegate()
    modifier.set_editor_property("x", x)
    modifier.set_editor_property("y", y)
    modifier.set_editor_property("z", z)
    return modifier


def swizzle_yxz():
    """Mueve el valor de X a Y: convierte un eje 1D en el componente Y de un Vector2D."""
    modifier = unreal.InputModifierSwizzleAxis()
    modifier.set_editor_property("order", unreal.InputAxisSwizzle.YXZ)
    return modifier


def get_mapping_property(context):
    """Nombre de la propiedad de mapeos que esta versión del motor expone."""
    for name in MAPPING_PROPERTIES:
        try:
            context.get_editor_property(name)
            return name
        except Exception:
            continue
    return None


def make_mapping(action, key_name, modifiers=None):
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    mapping.set_editor_property("key", make_key(key_name))

    if modifiers:
        try:
            mapping.set_editor_property("modifiers", modifiers)
        except Exception as error:
            log("AVISO: modificadores no aplicados a {}: {}".format(key_name, error))

    return mapping


def run():
    context = unreal.EditorAssetLibrary.load_asset(INPUT_DIR + "/IMC_Combate")
    if not context:
        log("ERROR: falta IMC_Combate. Ejecuta antes build_training_zone.py")
        return

    prop = get_mapping_property(context)
    if not prop:
        log("ERROR: el contexto no expone ninguna propiedad de mapeos conocida")
        return
    log("usando la propiedad '{}'".format(prop))

    actions = {}
    for name in ("IA_Look", "IA_Move", "IA_Jump", "IA_Fire", "IA_Aim",
                 "IA_Reload", "IA_Sprint", "IA_Crouch", "IA_SwitchWeapon"):
        asset = unreal.EditorAssetLibrary.load_asset(
            "{}/{}".format(ACTIONS_DIR, name))
        if asset:
            actions[name] = asset
        else:
            log("AVISO: falta {}".format(name))

    if "IA_Look" not in actions or "IA_Move" not in actions:
        log("ERROR: sin IA_Look o IA_Move no hay nada que mapear")
        return

    # Se reconstruye la lista entera: así el script es idempotente y no
    # acumula teclas duplicadas al reejecutarlo.
    mappings = []

    # Cámara: el ratón entrega un Vector2D listo para IA_Look.
    mappings.append(make_mapping(actions["IA_Look"], "Mouse2D"))

    # Movimiento. IA_Move entrega un Vector2D que ShooterCharacter::Move lee
    # como (X = lateral, Y = adelante). Una tecla produce 1.0 en X, así que
    # adelante/atrás necesitan el swizzle antes de negar.
    move = actions["IA_Move"]
    mappings.append(make_mapping(move, "W", [swizzle_yxz()]))
    mappings.append(make_mapping(move, "S", [swizzle_yxz(), negate(y=True)]))
    mappings.append(make_mapping(move, "D"))
    mappings.append(make_mapping(move, "A", [negate(x=True)]))

    # Resto de acciones.
    for action_name, key_name in (
        ("IA_Jump", "SpaceBar"),
        ("IA_Fire", "LeftMouseButton"),
        ("IA_Aim", "RightMouseButton"),
        ("IA_Reload", "R"),
        ("IA_Sprint", "LeftShift"),
        ("IA_Crouch", "LeftControl"),
        ("IA_SwitchWeapon", "Q"),
    ):
        if action_name in actions:
            mappings.append(make_mapping(actions[action_name], key_name))

    # 'default_key_mappings' es una estructura que contiene la lista, no la
    # lista en sí: hay que meter los mapeos dentro de ella.
    written_count = 0
    try:
        current = context.get_editor_property(prop)

        if isinstance(current, list):
            context.set_editor_property(prop, mappings)
            written_count = len(mappings)
        else:
            # Buscar dentro de la estructura el campo que guarda la lista.
            inner_name = None
            for candidate in ("mappings", "key_mappings", "default_key_mappings"):
                try:
                    current.get_editor_property(candidate)
                    inner_name = candidate
                    break
                except Exception:
                    continue

            if not inner_name:
                log("ERROR: la estructura no expone un campo de mapeos conocido")
                return

            current.set_editor_property(inner_name, mappings)
            context.set_editor_property(prop, current)
            written_count = len(mappings)
            log("escrito en {}.{}".format(prop, inner_name))
    except Exception as error:
        log("ERROR al escribir los mapeos: {}".format(error))
        return

    unreal.EditorAssetLibrary.save_loaded_asset(context)
    log("IMC_Combate: {} mapeos guardados".format(written_count))

    # Recompilar el personaje para que recoja el contexto actualizado.
    player_bp = unreal.EditorAssetLibrary.load_asset(BP_JUGADOR)
    if player_bp:
        library = getattr(unreal, "BlueprintEditorLibrary", None)
        if library and hasattr(library, "compile_blueprint"):
            try:
                library.compile_blueprint(player_bp)
            except Exception as error:
                log("AVISO: no se pudo recompilar BP_Jugador: {}".format(error))
        unreal.EditorAssetLibrary.save_loaded_asset(player_bp)
        log("BP_Jugador recompilado")

    log("")
    log("Listo. Abre Lvl_Entrenamiento y pulsa Play.")


try:
    run()
except Exception as error:
    import traceback
    unreal.log_error("[camara] FALLÓ: {}".format(error))
    unreal.log_error(traceback.format_exc())
