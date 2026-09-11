"""
Corrige el movimiento WASD.

Si el personaje se desplaza siempre en la misma dirección con cualquier
tecla, la causa suele ser una de dos:

  1. IA_Move no es de tipo Axis2D, así que Get<FVector2D>() no devuelve
     lo que el C++ espera.
  2. Los modificadores de las teclas no se guardaron, y las cuatro
     producen el mismo vector.

Este script comprueba ambas cosas, las arregla y deja en el registro lo
que encontró.

Ejecutar en la consola Python del editor:
    exec(open(r"<proyecto>/Content/Python/fix_movimiento.py").read())
"""

import unreal

INPUT_DIR = "/Game/Input"
ACTIONS_DIR = "/Game/Input/Actions"
BP_JUGADOR = "/Game/Blueprints/Core/BP_Jugador"

# Tecla -> (swizzle, negate_x, negate_y)
#
# IA_Move entrega un Vector2D que ShooterCharacter::Move lee como
# (X = lateral, Y = adelante). Una tecla produce 1.0 en X, así que
# adelante/atrás necesitan moverlo a Y con el swizzle antes de negar.
MOVE_KEYS = (
    ("W", True,  False, False),
    ("S", True,  False, True),
    ("D", False, False, False),
    ("A", False, True,  False),
)


def log(message):
    unreal.log("[movimiento] {}".format(message))


def make_key(key_name):
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    return key


def negate(x=False, y=False):
    modifier = unreal.InputModifierNegate()
    modifier.set_editor_property("x", x)
    modifier.set_editor_property("y", y)
    modifier.set_editor_property("z", False)
    return modifier


def swizzle():
    modifier = unreal.InputModifierSwizzleAxis()
    modifier.set_editor_property("order", unreal.InputAxisSwizzle.YXZ)
    return modifier


def get_mapping_container(context):
    """
    Devuelve (nombre_propiedad, nombre_campo_interno).

    En 5.8 'default_key_mappings' es una estructura que contiene la lista;
    en versiones previas 'mappings' era la lista directamente.
    """
    for prop in ("default_key_mappings", "mappings"):
        try:
            current = context.get_editor_property(prop)
        except Exception:
            continue

        if isinstance(current, list):
            return prop, None

        for inner in ("mappings", "key_mappings"):
            try:
                current.get_editor_property(inner)
                return prop, inner
            except Exception:
                continue

    return None, None


def run():
    context = unreal.EditorAssetLibrary.load_asset(INPUT_DIR + "/IMC_Combate")
    move = unreal.EditorAssetLibrary.load_asset(ACTIONS_DIR + "/IA_Move")

    if not context or not move:
        log("ERROR: falta IMC_Combate o IA_Move")
        return

    # --- 1. El tipo de la acción ---
    #
    # Ésta es la causa más probable del bug. build_training_zone.py crea las
    # acciones de COMBAT_ACTIONS como BOOLEAN; si IA_Move pasó por ahí en vez
    # de venir de la plantilla First Person, Get<FVector2D>() recibe un bool
    # convertido, que es (1,0) para cualquier tecla: las cuatro mueven de lado.
    try:
        value_type = move.get_editor_property("value_type")
        log("IA_Move.value_type = {}".format(value_type))

        if value_type != unreal.InputActionValueType.AXIS2D:
            move.set_editor_property(
                "value_type", unreal.InputActionValueType.AXIS2D)
            unreal.EditorAssetLibrary.save_loaded_asset(move)
            log("CORREGIDO: IA_Move era {}, pasa a Axis2D".format(value_type))
            log("  -> ÉSTA ERA LA CAUSA DEL BUG")
        else:
            log("IA_Move ya era Axis2D: la causa es otra, revisa los modificadores")
    except Exception as error:
        log("AVISO: no se pudo revisar el tipo: {}".format(error))

    # IA_Look también debe ser Axis2D para que el ratón funcione en dos ejes.
    look = unreal.EditorAssetLibrary.load_asset(ACTIONS_DIR + "/IA_Look")
    if look:
        try:
            if look.get_editor_property("value_type") != unreal.InputActionValueType.AXIS2D:
                look.set_editor_property(
                    "value_type", unreal.InputActionValueType.AXIS2D)
                unreal.EditorAssetLibrary.save_loaded_asset(look)
                log("CORREGIDO: IA_Look pasa a Axis2D")
        except Exception as error:
            log("AVISO: IA_Look sin revisar: {}".format(error))

    # --- 2. Los modificadores de cada tecla ---
    prop, inner = get_mapping_container(context)
    if not prop:
        log("ERROR: no se encontró dónde guarda los mapeos este contexto")
        return

    container = context.get_editor_property(prop)
    mappings = list(container) if inner is None else list(
        container.get_editor_property(inner))

    log("mapeos actuales: {}".format(len(mappings)))

    # Conservar todo lo que no sea movimiento.
    kept = [m for m in mappings
            if m.get_editor_property("action") != move]

    rebuilt = []
    for key_name, needs_swizzle, neg_x, neg_y in MOVE_KEYS:
        mapping = unreal.EnhancedActionKeyMapping()
        mapping.set_editor_property("action", move)
        mapping.set_editor_property("key", make_key(key_name))

        modifiers = []
        if needs_swizzle:
            modifiers.append(swizzle())
        if neg_x or neg_y:
            modifiers.append(negate(x=neg_x, y=neg_y))

        if modifiers:
            mapping.set_editor_property("modifiers", modifiers)

        rebuilt.append(mapping)
        log("  {} -> swizzle={} negate=({}, {}) [{} modificadores]".format(
            key_name, needs_swizzle, neg_x, neg_y, len(modifiers)))

    final = kept + rebuilt

    if inner is None:
        context.set_editor_property(prop, final)
    else:
        container.set_editor_property(inner, final)
        context.set_editor_property(prop, container)

    unreal.EditorAssetLibrary.save_loaded_asset(context)
    log("IMC_Combate guardado: {} mapeos".format(len(final)))

    # --- 3. Verificación: releer lo que quedó ---
    container = context.get_editor_property(prop)
    written = list(container) if inner is None else list(
        container.get_editor_property(inner))

    log("--- comprobación ---")
    log("IA_Move.value_type final = {}".format(
        move.get_editor_property("value_type")))

    for mapping in written:
        if mapping.get_editor_property("action") != move:
            continue
        key = mapping.get_editor_property("key")
        mods = mapping.get_editor_property("modifiers")
        log("  {:4} {} modificadores".format(
            str(key.key_name), len(mods) if mods else 0))

    # Si el tipo de la acción cambió, los mapeos guardados antes pueden
    # conservar el valor cacheado. Forzar una segunda escritura del contexto
    # garantiza que lo que quedó en disco corresponde al tipo actual.
    unreal.EditorAssetLibrary.save_loaded_asset(context)
    unreal.EditorAssetLibrary.save_loaded_asset(move)

    # --- 4. Recompilar el personaje ---
    player_bp = unreal.EditorAssetLibrary.load_asset(BP_JUGADOR)
    if player_bp:
        library = getattr(unreal, "BlueprintEditorLibrary", None)
        if library and hasattr(library, "compile_blueprint"):
            try:
                library.compile_blueprint(player_bp)
            except Exception as error:
                log("AVISO: no se pudo recompilar: {}".format(error))
        unreal.EditorAssetLibrary.save_loaded_asset(player_bp)
        log("BP_Jugador recompilado")

    log("")
    log("Listo. Vuelve a pulsar Play.")


try:
    run()
except Exception as error:
    import traceback
    unreal.log_error("[movimiento] FALLÓ: {}".format(error))
    unreal.log_error(traceback.format_exc())
