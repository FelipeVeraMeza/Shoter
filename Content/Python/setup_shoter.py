"""
Crea los assets que el módulo C++ necesita para jugar.

Ejecutar desde la consola de Python del editor:
    exec(open(r"<ruta>/Content/Python/setup_shoter.py").read())

Es idempotente: si un asset ya existe, lo reutiliza en vez de duplicarlo.
"""

import unreal

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

INPUT_ACTIONS = "/Game/Input/Actions"
INPUT_ROOT = "/Game/Input"
WEAPONS_DIR = "/Game/Blueprints/Weapons"
CORE_DIR = "/Game/Blueprints/Core"

# Acción -> tecla. Boolean basta para todas: son pulsar/soltar.
COMBAT_ACTIONS = {
    "IA_Fire": unreal.InputCoreKeys.LEFT_MOUSE_BUTTON,
    "IA_Aim": unreal.InputCoreKeys.RIGHT_MOUSE_BUTTON,
    "IA_Reload": unreal.InputCoreKeys.R,
    "IA_Sprint": unreal.InputCoreKeys.LEFT_SHIFT,
    "IA_Crouch": unreal.InputCoreKeys.LEFT_CONTROL,
    "IA_SwitchWeapon": unreal.InputCoreKeys.Q,
}


def log(message):
    unreal.log("[setup_shoter] {}".format(message))


def load_or_create(path, name, asset_class, factory):
    """Devuelve el asset si ya existe; si no, lo crea."""
    full_path = "{}/{}".format(path, name)
    existing = unreal.EditorAssetLibrary.load_asset(full_path)
    if existing:
        log("ya existe: {}".format(full_path))
        return existing

    created = ASSET_TOOLS.create_asset(name, path, asset_class, factory)
    log("creado: {}".format(full_path))
    return created


def create_input_actions():
    actions = {}
    for name in COMBAT_ACTIONS:
        action = load_or_create(
            INPUT_ACTIONS, name, unreal.InputAction, unreal.InputActionFactory()
        )
        if action:
            action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
            unreal.EditorAssetLibrary.save_loaded_asset(action)
            actions[name] = action
    return actions


def create_mapping_context(actions):
    context = load_or_create(
        INPUT_ROOT, "IMC_Combate",
        unreal.InputMappingContext, unreal.InputMappingContextFactory()
    )
    if not context:
        return None

    # Mapear de cero cada vez evita duplicar teclas al reejecutar el script.
    context.set_editor_property("mappings", [])

    for name, key in COMBAT_ACTIONS.items():
        action = actions.get(name)
        if not action:
            continue
        mapping = unreal.EnhancedActionKeyMapping()
        mapping.set_editor_property("action", action)
        mapping.set_editor_property("key", key)
        context.mappings.append(mapping)

    unreal.EditorAssetLibrary.save_loaded_asset(context)
    log("IMC_Combate mapeado con {} teclas".format(len(context.mappings)))
    return context


def create_weapon_data():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.WeaponDataAsset)

    data = load_or_create("/Game/Weapons", "DA_Rifle", unreal.WeaponDataAsset, factory)
    if data:
        unreal.EditorAssetLibrary.save_loaded_asset(data)
    return data


def create_blueprint(path, name, parent_class):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return load_or_create(path, name, unreal.Blueprint, factory)


def run():
    for folder in (INPUT_ACTIONS, WEAPONS_DIR, CORE_DIR, "/Game/Weapons"):
        if not unreal.EditorAssetLibrary.does_directory_exist(folder):
            unreal.EditorAssetLibrary.make_directory(folder)

    log("--- creando Input Actions ---")
    actions = create_input_actions()

    log("--- creando Mapping Context ---")
    create_mapping_context(actions)

    log("--- creando datos del arma ---")
    create_weapon_data()

    log("--- creando Blueprints ---")
    weapon_bp = create_blueprint(WEAPONS_DIR, "BP_Rifle", unreal.WeaponBase)
    player_bp = create_blueprint(CORE_DIR, "BP_Jugador", unreal.ShooterCharacter)
    mode_bp = create_blueprint(CORE_DIR, "BP_ModoJuego", unreal.ShooterGameMode)

    for blueprint in (weapon_bp, player_bp, mode_bp):
        if blueprint:
            unreal.EditorAssetLibrary.save_loaded_asset(blueprint)

    log("")
    log("Assets listos. Falta asignarlos a mano (las referencias entre")
    log("Blueprints no se pueden fijar por script de forma fiable):")
    log("  BP_Rifle    -> Weapon Data = DA_Rifle")
    log("  BP_Jugador  -> Inventory/Default Weapons += BP_Rifle")
    log("  BP_Jugador  -> Input: contexts IMC_Default + IMC_Combate, y las 10 acciones")
    log("  BP_ModoJuego-> Default Pawn Class = BP_Jugador")


run()
