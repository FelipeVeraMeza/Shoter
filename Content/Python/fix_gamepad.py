"""
Anade los mapeos de mando a IMC_Combate (RNF-4.5).

El juego debe ser jugable integramente con gamepad. Los ejes ya estan
configurados en DefaultInput.ini, pero sin mapeos en el contexto el mando
no hace nada.

Usa el mismo patron que fix_camara.py: en UE 5.8 la propiedad que el motor
lee es 'default_key_mappings', que es una estructura conteniendo la lista,
no la lista en si.

Ejecutar en la consola Python del editor:
    exec(open(r"<proyecto>/Content/Python/fix_gamepad.py").read())
"""

import unreal

INPUT_DIR = "/Game/Input"
ACTIONS_DIR = "/Game/Input/Actions"
BP_JUGADOR = "/Game/Blueprints/Core/BP_Jugador"

MAPPING_PROPERTIES = ("default_key_mappings", "mappings")

# Accion -> tecla de mando.
#
# Disposicion estandar de shooter en consola: gatillos para disparar y
# apuntar, stick izquierdo para moverse, derecho para mirar. Un jugador de
# consola no deberia tener que aprender nada (RNF-4.2).
GAMEPAD_BUTTONS = (
    ("IA_Jump", "Gamepad_FaceButton_Bottom"),
    ("IA_Fire", "Gamepad_RightTrigger"),
    ("IA_Aim", "Gamepad_LeftTrigger"),
    ("IA_Reload", "Gamepad_FaceButton_Left"),
    ("IA_Sprint", "Gamepad_LeftThumbstick"),
    ("IA_Crouch", "Gamepad_FaceButton_Right"),
    ("IA_SwitchWeapon", "Gamepad_FaceButton_Top"),
    ("IA_Grenade", "Gamepad_RightShoulder"),
)


def log(message):
    unreal.log("[gamepad] {}".format(message))


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
    modifier = unreal.InputModifierSwizzleAxis()
    modifier.set_editor_property("order", unreal.InputAxisSwizzle.YXZ)
    return modifier


def dead_zone(lower=0.25):
    """
    Zona muerta del stick.

    Sin ella el personaje deriva solo: los sticks analogicos rara vez
    devuelven cero exacto en reposo.
    """
    modifier = unreal.InputModifierDeadZone()
    try:
        modifier.set_editor_property("lower_threshold", lower)
    except Exception as error:
        log("AVISO: zona muerta sin aplicar: {}".format(error))
    return modifier


def get_mapping_property(context):
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


def load_actions():
    actions = {}
    names = [name for name, _ in GAMEPAD_BUTTONS] + ["IA_Move", "IA_Look"]

    for name in names:
        asset = unreal.EditorAssetLibrary.load_asset(
            "{}/{}".format(ACTIONS_DIR, name))
        if asset:
            actions[name] = asset
        else:
            log("AVISO: falta {}".format(name))

    return actions


def run():
    context = unreal.EditorAssetLibrary.load_asset(INPUT_DIR + "/IMC_Combate")
    if not context:
        log("ERROR: falta IMC_Combate. Ejecuta antes build_training_zone.py")
        return

    actions = load_actions()
    if "IA_Move" not in actions or "IA_Look" not in actions:
        log("ERROR: sin IA_Move o IA_Look no hay nada que mapear")
        return

    prop = get_mapping_property(context)
    if not prop:
        log("ERROR: no se encontro donde guarda los mapeos este contexto")
        return

    container = context.get_editor_property(prop)
    inner = None
    if not isinstance(container, list):
        for candidate in ("mappings", "key_mappings"):
            try:
                container.get_editor_property(candidate)
                inner = candidate
                break
            except Exception:
                continue

    existing = list(container) if inner is None else list(
        container.get_editor_property(inner))

    log("mapeos actuales: {}".format(len(existing)))

    # Conservar solo los de teclado y raton: los de mando se reconstruyen.
    # Sin esto, reejecutar el script duplicaria cada mapeo.
    kept = []
    for mapping in existing:
        try:
            key = mapping.get_editor_property("key")
            if not str(key.key_name).startswith("Gamepad"):
                kept.append(mapping)
        except Exception:
            kept.append(mapping)

    log("conservados de teclado/raton: {}".format(len(kept)))

    new_mappings = []

    # Movimiento con el stick izquierdo. IA_Move espera (X = lateral,
    # Y = adelante), que es justo lo que entrega Gamepad_Left2D.
    new_mappings.append(make_mapping(
        actions["IA_Move"], "Gamepad_Left2D", [dead_zone()]))

    # Camara con el stick derecho.
    new_mappings.append(make_mapping(
        actions["IA_Look"], "Gamepad_Right2D", [dead_zone()]))

    for action_name, key_name in GAMEPAD_BUTTONS:
        if action_name in actions:
            new_mappings.append(make_mapping(actions[action_name], key_name))

    final = kept + new_mappings

    if inner is None:
        context.set_editor_property(prop, final)
    else:
        container.set_editor_property(inner, final)
        context.set_editor_property(prop, container)

    unreal.EditorAssetLibrary.save_loaded_asset(context)
    log("IMC_Combate guardado: {} mapeos ({} de mando)".format(
        len(final), len(new_mappings)))

    # Recompilar el personaje: asignar sin recompilar no surte efecto.
    player_bp = unreal.EditorAssetLibrary.load_asset(BP_JUGADOR)
    if player_bp:
        library = getattr(unreal, "BlueprintEditorLibrary", None)
        if library and hasattr(library, "compile_blueprint"):
            try:
                library.compile_blueprint(player_bp)
                log("BP_Jugador recompilado")
            except Exception as error:
                log("AVISO: no se pudo recompilar: {}".format(error))
        unreal.EditorAssetLibrary.save_loaded_asset(player_bp)

    log("")
    log("Listo. Conecta un mando y pulsa Play.")


try:
    run()
except Exception as error:
    import traceback
    unreal.log_error("[gamepad] FALLO: {}".format(error))
    unreal.log_error(traceback.format_exc())
