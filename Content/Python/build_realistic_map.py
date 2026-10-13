"""
Construye el mapa realista "Distrito" y deja el juego configurado para él.

Qué hace, en orden:

  1. Materiales físicos con resistencia balística (hormigón, ladrillo,
     madera, chapa...): deciden qué atraviesa una bala.
  2. Materiales PBR: un material maestro procedural (variación de color y
     rugosidad en coordenadas de mundo, sin texturas) y sus instancias.
     Si el proyecto tiene materiales de Fab/Megascans, se usan esos.
  3. Armas realistas: fusil de asalto 5,56, pistola 9 mm y fusil de tirador
     7,62, con balística, selector de fuego y cargadores.
  4. Controles nuevos: asomarse (Q/E), selector de fuego (B), vendarse (H),
     granada (G) y pausa (P). Cambiar de arma pasa a la rueda y al 1.
  5. El nivel /Game/Maps/Lvl_Distrito: barrio urbano de 160 x 120 m con tres
     carriles (callejón norte, avenida central, patio industrial al sur),
     edificios con puertas y ventanas, coberturas, zonas de Dominación,
     apariciones, navegación para bots, cielo, nubes, niebla volumétrica y
     post-proceso.

Ejecutar en la consola Python del editor:
    exec(open(r"<proyecto>/Content/Python/build_realistic_map.py").read())

Es idempotente: reejecutarlo actualiza los assets y reconstruye solo los
actores que generó él mismo (etiqueta "ShoterGenerado"); lo que se haya
añadido a mano al nivel se conserva.
"""

import math
import random

import unreal

TAG = "ShoterGenerado"

LEVEL_PATH = "/Game/Maps/Lvl_Distrito"
REAL_DIR = "/Game/Realista"
MAT_DIR = REAL_DIR + "/Materiales"
PHYS_DIR = REAL_DIR + "/Fisica"
DATA_DIR = "/Game/Weapons"
WEAPONS_BP_DIR = "/Game/Blueprints/Weapons"
CORE_DIR = "/Game/Blueprints/Core"
INPUT_DIR = "/Game/Input"
ACTIONS_DIR = "/Game/Input/Actions"

CUBE = "/Engine/BasicShapes/Cube.Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder.Cylinder"

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LEVEL_SUB = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTOR_SUB = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MEL = unreal.MaterialEditingLibrary

# Semilla fija: el mapa sale igual en cada ejecución y en cada máquina.
random.seed(1337)


def log(message):
    unreal.log("[distrito] {}".format(message))


def warn(message):
    unreal.log_warning("[distrito] AVISO: {}".format(message))


def ensure_dirs(*paths):
    for path in paths:
        if not unreal.EditorAssetLibrary.does_directory_exist(path):
            unreal.EditorAssetLibrary.make_directory(path)


def load(path):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)
    return None


def save(asset):
    try:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
    except Exception as error:
        warn("no se pudo guardar {}: {}".format(asset, error))


def create_or_load(directory, name, asset_class, factory):
    existing = load("{}/{}".format(directory, name))
    if existing:
        return existing, False
    return ASSET_TOOLS.create_asset(name, directory, asset_class, factory), True


def blueprint_class(blueprint):
    """Clase generada por un Blueprint (en 5.8 es un método, no una propiedad)."""
    if not blueprint:
        return None
    try:
        generated = blueprint.generated_class()
        if generated:
            return generated
    except Exception:
        pass
    try:
        return unreal.load_object(None, blueprint.get_path_name() + "_C")
    except Exception:
        return None


def get_cdo(blueprint):
    generated = blueprint_class(blueprint)
    return unreal.get_default_object(generated) if generated else None


def compile_and_save(blueprint, label):
    """Asignar al CDO sin recompilar no llega al juego: falla en silencio."""
    if not blueprint:
        return
    library = getattr(unreal, "BlueprintEditorLibrary", None)
    try:
        if library and hasattr(library, "compile_blueprint"):
            library.compile_blueprint(blueprint)
        else:
            unreal.KismetEditorUtilities.compile_blueprint(blueprint)
    except Exception as error:
        warn("{} sin recompilar ({}): ábrelo y pulsa Compilar".format(label, error))
    save(blueprint)


def set_props(obj, values, label):
    """Asigna varias propiedades, avisando de cada una que falle sin abortar."""
    for name, value in values.items():
        try:
            obj.set_editor_property(name, value)
        except Exception as error:
            warn("{}.{} sin asignar: {}".format(label, name, error))


# ==========================================================================
# 1. Materiales físicos
# ==========================================================================

# Coste de atravesar 1 cm, en unidades de PenetrationPower (ver
# ShooterPhysicalMaterial.h). El fusil 5,56 tiene 30: cruza una valla de
# madera o una chapa, no una pared de ladrillo.
PHYSICAL_MATERIALS = {
    "Hormigon": 4.0,
    "Asfalto": 6.0,
    "Ladrillo": 3.0,
    "Madera": 1.0,
    "Chapa": 2.0,
    "Contenedor": 0.6,
    "Vehiculo": 2.5,
    "Arena": 8.0,
}


def build_physical_materials():
    factory = unreal.PhysicalMaterialFactoryNew()
    try:
        factory.set_editor_property("physical_material_class", unreal.ShooterPhysicalMaterial)
    except Exception as error:
        warn("la factory no admite clase propia ({}); se intenta igual".format(error))

    result = {}
    for name, resistance in PHYSICAL_MATERIALS.items():
        asset, _ = create_or_load(PHYS_DIR, "PM_" + name, unreal.ShooterPhysicalMaterial, factory)
        if not asset:
            warn("PM_{} no se pudo crear".format(name))
            continue
        try:
            asset.set_editor_property("penetration_resistance", resistance)
        except Exception as error:
            warn("PM_{} no es ShooterPhysicalMaterial: {}".format(name, error))
        save(asset)
        result[name] = asset

    log("materiales físicos: {}".format(len(result)))
    return result


# ==========================================================================
# 2. Materiales de superficie
# ==========================================================================

# nombre -> (color A, color B, rugosidad A, rugosidad B, metálico, escala
#            macro, material físico, palabras clave para buscar en Fab)
SURFACES = {
    "Asfalto":    ((0.045, 0.045, 0.048), (0.075, 0.072, 0.07), 0.82, 0.95, 0.0, 0.0015, "Asfalto", ("asphalt", "road")),
    "Hormigon":   ((0.32, 0.31, 0.29), (0.42, 0.41, 0.38), 0.78, 0.92, 0.0, 0.002, "Hormigon", ("concrete",)),
    "Acera":      ((0.38, 0.37, 0.35), (0.3, 0.29, 0.27), 0.75, 0.9, 0.0, 0.004, "Hormigon", ("pavement", "sidewalk", "tiles")),
    "Yeso":       ((0.48, 0.43, 0.36), (0.36, 0.33, 0.28), 0.85, 0.95, 0.0, 0.003, "Ladrillo", ("plaster", "stucco")),
    "YesoGris":   ((0.38, 0.39, 0.4), (0.3, 0.3, 0.31), 0.85, 0.95, 0.0, 0.003, "Ladrillo", ("plaster", "wall")),
    "Ladrillo":   ((0.33, 0.13, 0.08), (0.24, 0.1, 0.07), 0.8, 0.93, 0.0, 0.004, "Ladrillo", ("brick",)),
    "MetalRojo":  ((0.35, 0.06, 0.04), (0.22, 0.08, 0.05), 0.5, 0.75, 0.6, 0.003, "Contenedor", ("painted_metal", "container")),
    "MetalAzul":  ((0.05, 0.13, 0.28), (0.07, 0.1, 0.16), 0.5, 0.75, 0.6, 0.003, "Contenedor", ("painted_metal", "container")),
    "MetalVerde": ((0.08, 0.2, 0.1), (0.07, 0.12, 0.08), 0.5, 0.75, 0.6, 0.003, "Contenedor", ("painted_metal", "container")),
    "Oxido":      ((0.3, 0.14, 0.06), (0.18, 0.1, 0.06), 0.7, 0.92, 0.4, 0.004, "Chapa", ("rust", "corrugated")),
    "Madera":     ((0.32, 0.2, 0.11), (0.22, 0.14, 0.08), 0.7, 0.88, 0.0, 0.01, "Madera", ("wood", "plank")),
    "Arena":      ((0.42, 0.36, 0.25), (0.33, 0.28, 0.2), 0.88, 0.97, 0.0, 0.008, "Arena", ("sandbag", "burlap", "fabric")),
    "Coche":      ((0.2, 0.21, 0.22), (0.12, 0.12, 0.13), 0.35, 0.65, 0.7, 0.004, "Vehiculo", ("car_paint", "metal")),
    "Tierra":     ((0.2, 0.16, 0.11), (0.14, 0.12, 0.09), 0.9, 1.0, 0.0, 0.002, "Hormigon", ("soil", "dirt", "ground")),
}


def find_fab_materials():
    """
    Materiales de Fab/Megascans ya importados, por palabra clave.

    Con assets fotoescaneados el resultado es otro nivel de realismo; el
    material procedural es solo el respaldo para que el mapa sea jugable
    desde el primer momento.
    """
    found = []
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for root in ("/Game/Fab", "/Game/Megascans", "/Game/Quixel", "/Game/MSPresets"):
        if not unreal.EditorAssetLibrary.does_directory_exist(root):
            continue
        try:
            for data in registry.get_assets_by_path(root, recursive=True):
                class_name = ""
                try:
                    class_name = str(data.asset_class_path.asset_name)
                except Exception:
                    class_name = str(getattr(data, "asset_class", ""))
                if class_name == "MaterialInstanceConstant":
                    found.append((str(data.asset_name).lower(), "{}.{}".format(data.package_name, data.asset_name)))
        except Exception as error:
            warn("no se pudo explorar {}: {}".format(root, error))
    if found:
        log("materiales de Fab/Megascans encontrados: {}".format(len(found)))
    return found


def build_master_material():
    """
    Material maestro procedural.

    Mezcla dos colores y dos rugosidades con ruido en coordenadas de mundo:
    al no depender de las UV, los cubos escalados no estiran nada y cada
    pared tiene manchas distintas. Es la diferencia entre "gris de prototipo"
    y una superficie que el ojo acepta como hormigón o metal pintado.
    """
    path = MAT_DIR + "/M_Superficie"
    existing = load(path)
    if existing:
        return existing

    material = ASSET_TOOLS.create_asset("M_Superficie", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())

    def expression(cls, x, y):
        return MEL.create_material_expression(material, cls, x, y)

    def vector_param(name, value, x, y):
        node = expression(unreal.MaterialExpressionVectorParameter, x, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", unreal.LinearColor(value[0], value[1], value[2], 1.0))
        return node

    def scalar_param(name, value, x, y):
        node = expression(unreal.MaterialExpressionScalarParameter, x, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", value)
        return node

    def world_noise(scale_node, x, y, levels):
        position = expression(unreal.MaterialExpressionWorldPosition, x - 500, y)
        scaled = expression(unreal.MaterialExpressionMultiply, x - 250, y)
        MEL.connect_material_expressions(position, "", scaled, "A")
        MEL.connect_material_expressions(scale_node, "", scaled, "B")

        noise = expression(unreal.MaterialExpressionNoise, x, y)
        for name, value in (("scale", 1.0), ("output_min", 0.0), ("output_max", 1.0),
                            ("levels", levels), ("quality", 2)):
            try:
                noise.set_editor_property(name, value)
            except Exception as error:
                warn("Noise.{}: {}".format(name, error))
        MEL.connect_material_expressions(scaled, "", noise, "Position")
        return noise

    color_a = vector_param("ColorA", (0.35, 0.35, 0.35), -900, -300)
    color_b = vector_param("ColorB", (0.45, 0.45, 0.45), -900, -150)
    rough_a = scalar_param("RugosidadA", 0.8, -900, 250)
    rough_b = scalar_param("RugosidadB", 0.95, -900, 350)
    metallic = scalar_param("Metalico", 0.0, -900, 500)
    macro_scale = scalar_param("EscalaMacro", 0.002, -1500, -100)
    micro_scale = scalar_param("EscalaMicro", 0.05, -1500, 150)

    macro = world_noise(macro_scale, -650, 0, 5)
    micro = world_noise(micro_scale, -650, 200, 3)

    color_mix = expression(unreal.MaterialExpressionLinearInterpolate, -300, -200)
    MEL.connect_material_expressions(color_a, "", color_mix, "A")
    MEL.connect_material_expressions(color_b, "", color_mix, "B")
    MEL.connect_material_expressions(macro, "", color_mix, "Alpha")

    # Variación fina de brillo: el grano de la superficie.
    grain = expression(unreal.MaterialExpressionLinearInterpolate, -300, 0)
    grain.set_editor_property("const_a", 0.8)
    grain.set_editor_property("const_b", 1.05)
    MEL.connect_material_expressions(micro, "", grain, "Alpha")

    base_color = expression(unreal.MaterialExpressionMultiply, -100, -100)
    MEL.connect_material_expressions(color_mix, "", base_color, "A")
    MEL.connect_material_expressions(grain, "", base_color, "B")

    roughness = expression(unreal.MaterialExpressionLinearInterpolate, -300, 300)
    MEL.connect_material_expressions(rough_a, "", roughness, "A")
    MEL.connect_material_expressions(rough_b, "", roughness, "B")
    MEL.connect_material_expressions(micro, "", roughness, "Alpha")

    MEL.connect_material_property(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)

    MEL.recompile_material(material)
    save(material)
    log("M_Superficie creado")
    return material


def build_surfaces(physical):
    master = build_master_material()
    fab = find_fab_materials()

    materials = {}
    for name, (color_a, color_b, rough_a, rough_b, metal, scale, phys_key, keywords) in SURFACES.items():
        # Con Fab/Megascans disponible, se usa el primero que encaje.
        match = next((path for asset_name, path in fab
                      if any(keyword in asset_name for keyword in keywords)), None)
        if match:
            asset = unreal.EditorAssetLibrary.load_asset(match)
            if asset:
                materials[name] = asset
                log("superficie {} -> {}".format(name, match))
                continue

        instance, _ = create_or_load(MAT_DIR, "MI_" + name, unreal.MaterialInstanceConstant,
                                     unreal.MaterialInstanceConstantFactoryNew())
        if not instance:
            warn("MI_{} no se pudo crear".format(name))
            continue

        MEL.set_material_instance_parent(instance, master)
        MEL.set_material_instance_vector_parameter_value(instance, "ColorA", unreal.LinearColor(*color_a, 1.0))
        MEL.set_material_instance_vector_parameter_value(instance, "ColorB", unreal.LinearColor(*color_b, 1.0))
        MEL.set_material_instance_scalar_parameter_value(instance, "RugosidadA", rough_a)
        MEL.set_material_instance_scalar_parameter_value(instance, "RugosidadB", rough_b)
        MEL.set_material_instance_scalar_parameter_value(instance, "Metalico", metal)
        MEL.set_material_instance_scalar_parameter_value(instance, "EscalaMacro", scale)

        if phys_key in physical:
            try:
                instance.set_editor_property("phys_material", physical[phys_key])
            except Exception as error:
                warn("MI_{} sin material físico: {}".format(name, error))

        MEL.update_material_instance(instance)
        save(instance)
        materials[name] = instance

    log("superficies listas: {}".format(len(materials)))
    return materials


# ==========================================================================
# 3. Armas
# ==========================================================================

FM = unreal.WeaponFireMode

WEAPONS = {
    # Fusil de asalto 5,56x45: tres impactos al torso, uno a la cabeza.
    "DA_Rifle": {
        "bp": "BP_Rifle",
        "mesh": "/Game/Weapons/Rifle/Meshes/SKM_Rifle",
        "values": {
            "display_name": "Fusil de asalto 5,56",
            "fire_mode": FM.AUTOMATIC,
            "available_fire_modes": [FM.SEMI_AUTO, FM.AUTOMATIC],
            "damage": 40.0, "headshot_multiplier": 3.0,
            "stomach_multiplier": 0.9, "limb_multiplier": 0.6,
            "rounds_per_minute": 750.0, "magazine_size": 30, "max_reserve_ammo": 150,
            "reload_time": 2.4, "empty_reload_time": 3.1, "closed_bolt": True,
            "ads_time": 0.3, "range": 60000.0, "effective_range": 8000.0,
            "spread_hip": 3.0, "spread_ads": 0.08, "spread_per_shot": 0.25,
            "spread_max": 4.0, "spread_recovery_rate": 8.0,
            "base_recoil_vertical": 0.55, "base_recoil_horizontal": 0.22,
            "recoil_recovery_fraction": 0.7,
            "use_ballistics": True, "muzzle_velocity": 92000.0,
            "velocity_decay_distance": 90000.0, "zeroing_distance": 10000.0,
            "penetration_power": 30.0, "aim_fov": 55.0, "sway_multiplier": 1.0,
        },
    },
    # Pistola 9x19: cuatro impactos al torso; más lenta y con más caída.
    "DA_Pistola": {
        "bp": "BP_Pistola",
        "mesh": "/Game/Weapons/Pistol/Meshes/SKM_Pistol",
        "values": {
            "display_name": "Pistola 9 mm",
            "fire_mode": FM.SEMI_AUTO,
            "available_fire_modes": [FM.SEMI_AUTO],
            "damage": 30.0, "headshot_multiplier": 3.0,
            "stomach_multiplier": 0.9, "limb_multiplier": 0.6,
            "rounds_per_minute": 450.0, "magazine_size": 17, "max_reserve_ammo": 51,
            "reload_time": 1.8, "empty_reload_time": 2.2, "closed_bolt": True,
            "ads_time": 0.2, "range": 20000.0, "effective_range": 2500.0,
            "spread_hip": 2.2, "spread_ads": 0.18, "spread_per_shot": 0.4,
            "spread_max": 4.0, "spread_recovery_rate": 7.0,
            "base_recoil_vertical": 0.9, "base_recoil_horizontal": 0.3,
            "recoil_recovery_fraction": 0.85,
            "use_ballistics": True, "muzzle_velocity": 37000.0,
            "velocity_decay_distance": 40000.0, "zeroing_distance": 2500.0,
            "penetration_power": 12.0, "aim_fov": 70.0, "sway_multiplier": 0.7,
        },
    },
    # Fusil de tirador 7,62x51 con visor: dos al torso, atraviesa más.
    "DA_Tirador": {
        "bp": "BP_Tirador",
        "mesh": "/Game/Weapons/Rifle/Meshes/SKM_Rifle",
        "values": {
            "display_name": "Fusil de tirador 7,62",
            "fire_mode": FM.SEMI_AUTO,
            "available_fire_modes": [FM.SEMI_AUTO],
            "damage": 70.0, "headshot_multiplier": 3.0,
            "stomach_multiplier": 0.9, "limb_multiplier": 0.65,
            "rounds_per_minute": 300.0, "magazine_size": 20, "max_reserve_ammo": 60,
            "reload_time": 2.8, "empty_reload_time": 3.4, "closed_bolt": True,
            "ads_time": 0.4, "range": 80000.0, "effective_range": 15000.0,
            "spread_hip": 4.0, "spread_ads": 0.03, "spread_per_shot": 0.6,
            "spread_max": 5.0, "spread_recovery_rate": 6.0,
            "base_recoil_vertical": 1.6, "base_recoil_horizontal": 0.35,
            "recoil_recovery_fraction": 0.8,
            "use_ballistics": True, "muzzle_velocity": 83000.0,
            "velocity_decay_distance": 120000.0, "zeroing_distance": 10000.0,
            "penetration_power": 55.0, "aim_fov": 30.0, "sway_multiplier": 1.4,
        },
    },
}


def build_weapons():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.WeaponDataAsset)

    blueprints = {}
    for asset_name, spec in WEAPONS.items():
        data, created = create_or_load(DATA_DIR, asset_name, unreal.WeaponDataAsset, factory)
        if not data:
            warn("{} no se pudo crear".format(asset_name))
            continue

        set_props(data, spec["values"], asset_name)

        # La malla solo se asigna si falta: si ya hay una propia, se respeta.
        try:
            if not data.get_editor_property("weapon_mesh"):
                mesh = load(spec["mesh"])
                if mesh:
                    data.set_editor_property("weapon_mesh", mesh)
        except Exception as error:
            warn("{} sin malla: {}".format(asset_name, error))

        save(data)

        bp_factory = unreal.BlueprintFactory()
        bp_factory.set_editor_property("parent_class", unreal.WeaponBase)
        blueprint, _ = create_or_load(WEAPONS_BP_DIR, spec["bp"], unreal.Blueprint, bp_factory)
        cdo = get_cdo(blueprint)
        if cdo:
            cdo.set_editor_property("weapon_data", data)
        compile_and_save(blueprint, spec["bp"])
        blueprints[spec["bp"]] = blueprint
        log("{} {} -> {}".format(asset_name, "creado" if created else "actualizado", spec["bp"]))

    return blueprints


# ==========================================================================
# 4. Controles
# ==========================================================================

# Acción -> (propiedad en ShooterCharacter, teclas de teclado, botones de mando)
NEW_ACTIONS = {
    "IA_LeanLeft": ("lean_left_action", ["Q"], ["Gamepad_DPad_Left"]),
    "IA_LeanRight": ("lean_right_action", ["E"], ["Gamepad_DPad_Right"]),
    "IA_FireMode": ("fire_mode_action", ["B"], ["Gamepad_DPad_Up"]),
    "IA_Bandage": ("bandage_action", ["H"], ["Gamepad_DPad_Down"]),
    "IA_Grenade": ("grenade_action", ["G"], ["Gamepad_RightShoulder"]),
    "IA_Pause": ("pause_action", ["P", "Escape"], ["Gamepad_Special_Right"]),
    # Q pasa a asomarse: el cambio de arma va a la rueda y al 1.
    "IA_SwitchWeapon": ("switch_weapon_action", ["MouseScrollUp", "MouseScrollDown", "One"], ["Gamepad_FaceButton_Top"]),
}

# Acciones que ya existían, con todas sus teclas: el contexto se reconstruye
# entero en cada ejecución.
BASE_ACTIONS = {
    "IA_Move": ("move_action", [], []),
    "IA_Look": ("look_action", [], []),
    "IA_Jump": ("jump_action", ["SpaceBar"], ["Gamepad_FaceButton_Bottom"]),
    "IA_Fire": ("fire_action", ["LeftMouseButton"], ["Gamepad_RightTrigger"]),
    "IA_Aim": ("aim_action", ["RightMouseButton"], ["Gamepad_LeftTrigger"]),
    "IA_Reload": ("reload_action", ["R"], ["Gamepad_FaceButton_Left"]),
    "IA_Sprint": ("sprint_action", ["LeftShift"], ["Gamepad_LeftThumbstick"]),
    "IA_Crouch": ("crouch_action", ["LeftControl", "C"], ["Gamepad_FaceButton_Right"]),
}

MAPPING_PROPERTIES = ("default_key_mappings", "mappings")


def make_key(key_name):
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    return key


def make_mapping(action, key_name):
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    mapping.set_editor_property("key", make_key(key_name))
    return mapping


def read_mappings(context):
    """Devuelve (propiedad, contenedor, campo interno, lista) según la versión."""
    for prop in MAPPING_PROPERTIES:
        try:
            container = context.get_editor_property(prop)
        except Exception:
            continue
        if isinstance(container, list):
            return prop, container, None, list(container)
        for inner in ("mappings", "key_mappings"):
            try:
                return prop, container, inner, list(container.get_editor_property(inner))
            except Exception:
                continue
    return None, None, None, []


def build_input(player_bp):
    context = load(INPUT_DIR + "/IMC_Combate")
    if not context:
        warn("falta IMC_Combate: ejecuta antes build_training_zone.py")
        return

    factory = unreal.InputAction_Factory()
    actions = {}
    for name in NEW_ACTIONS:
        action, created = create_or_load(ACTIONS_DIR, name, unreal.InputAction, factory)
        if not action:
            warn("{} no se pudo crear".format(name))
            continue
        if created:
            action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
            save(action)
        actions[name] = action

    for name in BASE_ACTIONS:
        action = load("{}/{}".format(ACTIONS_DIR, name))
        if action:
            actions[name] = action
        else:
            warn("falta {}".format(name))

    # IA_Move e IA_Look deben ser vectores: si fueran booleanos, las cuatro
    # teclas de movimiento darían el mismo valor.
    for name in ("IA_Move", "IA_Look"):
        if name in actions:
            actions[name].set_editor_property("value_type", unreal.InputActionValueType.AXIS2D)
            save(actions[name])

    prop, container, inner, _ = read_mappings(context)
    if prop is None:
        warn("IMC_Combate no expone mapeos conocidos; asigna las teclas a mano")
        return

    # Se reconstruye la lista entera: idempotente y sin restos de mapeos
    # anteriores rotos.
    mappings = []

    def add(action_name, key_name, modifiers=None):
        if action_name not in actions:
            return
        mapping = make_mapping(actions[action_name], key_name)
        if modifiers:
            mapping.set_editor_property("modifiers", modifiers)
        mappings.append(mapping)

    # Los modificadores se crean DENTRO del contexto. Creados con su
    # constructor viven en el paquete transitorio, y al guardar el asset se
    # pierden en silencio: era la causa de que WASD movieran siempre igual.
    def swizzle():
        modifier = unreal.new_object(unreal.InputModifierSwizzleAxis, outer=context)
        modifier.set_editor_property("order", unreal.InputAxisSwizzle.YXZ)
        return modifier

    def negate(x=False, y=False):
        modifier = unreal.new_object(unreal.InputModifierNegate, outer=context)
        modifier.set_editor_property("x", x)
        modifier.set_editor_property("y", y)
        modifier.set_editor_property("z", False)
        return modifier

    def dead_zone():
        modifier = unreal.new_object(unreal.InputModifierDeadZone, outer=context)
        modifier.set_editor_property("lower_threshold", 0.2)
        return modifier

    # Movimiento: X = lateral, Y = adelante (ShooterCharacter::Move).
    for forward, back, right, left in (("W", "S", "D", "A"), ("Up", "Down", "Right", "Left")):
        add("IA_Move", forward, [swizzle()])
        add("IA_Move", back, [swizzle(), negate(y=True)])
        add("IA_Move", right)
        add("IA_Move", left, [negate(x=True)])
    add("IA_Move", "Gamepad_Left2D", [dead_zone()])

    add("IA_Look", "Mouse2D")
    add("IA_Look", "Gamepad_Right2D", [dead_zone()])

    for name, (_, keys, pads) in list(BASE_ACTIONS.items()) + list(NEW_ACTIONS.items()):
        if name in ("IA_Move", "IA_Look"):
            continue
        for key_name in keys + pads:
            add(name, key_name)

    if inner is None:
        context.set_editor_property(prop, mappings)
    else:
        container.set_editor_property(inner, mappings)
        context.set_editor_property(prop, container)
    save(context)

    # Comprobación: lo que quedó guardado, no lo que se intentó guardar.
    _, _, _, stored = read_mappings(context)
    for mapping in stored:
        key_name = str(mapping.get_editor_property("key").get_editor_property("key_name"))
        if key_name in ("W", "S", "A", "D"):
            modifiers = mapping.get_editor_property("modifiers") or []
            outers = [m.get_outer().get_name() for m in modifiers if m]
            log("  {} -> {} modificadores en {}".format(key_name, len(modifiers), outers))
    log("IMC_Combate: {} mapeos".format(len(stored)))

    cdo = get_cdo(player_bp)
    if cdo:
        # Un solo contexto: con IMC_Default también activo, la plantilla y
        # este contexto mapean IA_Move a las mismas teclas y se pisan.
        cdo.set_editor_property("default_mapping_contexts", [context])
        for name, (property_name, _, _) in list(BASE_ACTIONS.items()) + list(NEW_ACTIONS.items()):
            if name in actions and property_name:
                try:
                    cdo.set_editor_property(property_name, actions[name])
                except Exception as error:
                    warn("BP_Jugador.{}: {}".format(property_name, error))


def configure_player(player_bp, weapon_bps):
    cdo = get_cdo(player_bp)
    if not cdo:
        warn("BP_Jugador sin CDO accesible")
        return

    # Fusil como principal y pistola como secundaria: la configuración
    # estándar de un fusilero.
    loadout = [blueprint_class(weapon_bps[name]) for name in ("BP_Rifle", "BP_Pistola") if name in weapon_bps]
    loadout = [cls for cls in loadout if cls]
    try:
        inventory = cdo.get_editor_property("inventory")
        if inventory and loadout:
            inventory.set_editor_property("default_weapons", loadout)
            log("BP_Jugador -> equipo: fusil + pistola")
    except Exception as error:
        warn("equipo sin asignar ({}): ponlo a mano en BP_Jugador > Inventory".format(error))

    # La salud realista vive en el componente: sin regeneración y con heridas.
    try:
        health = cdo.get_editor_property("health_component")
        if health:
            set_props(health, {"regen_rate": 0.0, "max_bandages": 2}, "HealthComponent")
    except Exception as error:
        warn("salud sin ajustar: {}".format(error))


def build_game_mode(player_bp):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.ShooterGameMode)
    mode_bp, _ = create_or_load(CORE_DIR, "BP_ModoRealista", unreal.Blueprint, factory)
    cdo = get_cdo(mode_bp)
    if cdo:
        values = {
            "endless_match": False,
            "team_based": True,
            "game_mode_type": unreal.ShooterGameModeType.TEAM_DEATHMATCH,
            "match_duration": 900.0,
            "score_to_win": 60,
            "respawn_delay": 6.0,
            "minimum_safe_spawn_distance": 3000.0,
            # Ocho jugadores: los huecos se rellenan con bots.
            "desired_player_count": 8,
        }
        set_props(cdo, values, "BP_ModoRealista")
        try:
            cdo.set_editor_property("bot_controller_class", unreal.ShooterBotController)
        except Exception as error:
            warn("bots sin controlador: {}".format(error))
        player_class = blueprint_class(player_bp)
        if player_class:
            cdo.set_editor_property("default_pawn_class", player_class)
    compile_and_save(mode_bp, "BP_ModoRealista")
    return mode_bp


# ==========================================================================
# 5. Nivel
# ==========================================================================

class Builder(object):
    """Coloca la geometría y lleva la cuenta de lo creado."""

    def __init__(self, materials, physical):
        self.materials = materials
        self.physical = physical
        self.cube = load(CUBE)
        self.cylinder = load(CYLINDER)
        self.count = 0

        # Con la iluminación estática desactivada (Lumen), la geometría
        # STATIC no exige construir luces y es más barata: las sombras
        # virtuales y Lumen cachean lo que no se mueve.
        allow_static = True
        try:
            allow_static = unreal.SystemLibrary.get_console_variable_bool_value("r.AllowStaticLighting")
        except Exception:
            pass
        self.mobility = unreal.ComponentMobility.MOVABLE if allow_static else unreal.ComponentMobility.STATIC
        log("movilidad de la geometría: {}".format("MOVABLE" if allow_static else "STATIC"))

    def tag(self, actor, label, folder):
        actor.set_actor_label(label)
        try:
            actor.set_editor_property("tags", [unreal.Name(TAG)])
        except Exception:
            pass
        try:
            actor.set_folder_path(unreal.Name("Distrito/" + folder))
        except Exception:
            pass

    def mesh(self, mesh, center, size, material_key, label, folder, yaw=0.0, pitch=0.0, base_size=100.0):
        actor = ACTOR_SUB.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(*center), unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0))
        if not actor:
            return None

        component = actor.static_mesh_component
        component.set_mobility(self.mobility)
        component.set_static_mesh(mesh)
        actor.set_actor_scale3d(unreal.Vector(size[0] / base_size, size[1] / base_size, size[2] / base_size))

        material = self.materials.get(material_key)
        if material:
            component.set_material(0, material)

        # El trazado de balas usa la colisión simple, que no lee el material
        # físico del material visual: hay que fijarlo en el componente.
        phys_key = SURFACES.get(material_key, (None,) * 8)[6]
        if phys_key in self.physical:
            try:
                component.set_phys_material_override(self.physical[phys_key])
            except Exception:
                pass

        self.tag(actor, label, folder)
        self.count += 1
        return actor

    def box(self, x, y, z, sx, sy, sz, material, label, folder, yaw=0.0, pitch=0.0):
        """Caja con la base en z (no el centro): así se apilan sin cuentas."""
        return self.mesh(self.cube, (x, y, z + sz / 2.0), (sx, sy, sz), material, label, folder, yaw, pitch)

    def cylinder_at(self, x, y, z, diameter, height, material, label, folder):
        return self.mesh(self.cylinder, (x, y, z + height / 2.0), (diameter, diameter, height),
                         material, label, folder)

    def actor(self, cls, location, rotation=(0.0, 0.0, 0.0), label="", folder="Logica"):
        """rotation = (pitch, yaw, roll)."""
        actor = ACTOR_SUB.spawn_actor_from_class(
            cls, unreal.Vector(*location),
            unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]))
        if actor:
            self.tag(actor, label or cls.__name__, folder)
        return actor


# --- Piezas de construcción ------------------------------------------------

FLOOR_HEIGHT = 380.0
WALL = 25.0


def wall_run(b, start, direction, length, z0, height, openings, material, label, folder):
    """
    Muro recto con huecos. openings: (centro desde el inicio, ancho,
    altura inferior, altura superior). Se trocea en piezas macizas
    alrededor de cada hueco.
    """
    dx, dy = direction
    along_x = abs(dx) > 0

    def piece(a, c, bottom, top, suffix):
        if c - a < 1.0 or top - bottom < 1.0:
            return
        mid = (a + c) / 2.0
        x = start[0] + dx * mid
        y = start[1] + dy * mid
        sx, sy = (c - a, WALL) if along_x else (WALL, c - a)
        b.box(x, y, z0 + bottom, sx, sy, top - bottom, material, label + suffix, folder)

    cursor = 0.0
    for index, (center, width, bottom, top) in enumerate(sorted(openings)):
        a, c = center - width / 2.0, center + width / 2.0
        piece(cursor, a, 0.0, height, "_{}".format(index))
        piece(a, c, 0.0, bottom, "_{}b".format(index))
        piece(a, c, top, height, "_{}t".format(index))
        cursor = c
    piece(cursor, length, 0.0, height, "_fin")


def building(b, name, cx, cy, width, depth, floors, material, doors=None, windows=None,
             floor_height=FLOOR_HEIGHT, ramp=False, folder="Edificios"):
    """
    Edificio hueco y transitable. doors/windows: {lado: [desplazamientos
    desde el centro del lado]}. Lados: S (y-), N (y+), W (x-), E (x+).
    Las puertas solo van en planta baja; las ventanas, en todas.
    """
    doors = doors or {}
    windows = windows or {}
    x0, x1 = cx - width / 2.0, cx + width / 2.0
    y0, y1 = cy - depth / 2.0, cy + depth / 2.0

    sides = {
        "S": ((x0, y0), (1, 0), width),
        "N": ((x0, y1), (1, 0), width),
        "W": ((x0, y0), (0, 1), depth),
        "E": ((x1, y0), (0, 1), depth),
    }

    for floor in range(floors):
        z0 = floor * floor_height
        for side, (start, direction, length) in sides.items():
            openings = []
            if floor == 0:
                openings += [(length / 2.0 + offset, 130.0, 0.0, 230.0) for offset in doors.get(side, [])]
            openings += [(length / 2.0 + offset, 130.0, 100.0, 240.0) for offset in windows.get(side, [])]
            wall_run(b, start, direction, length, z0, floor_height, openings, material,
                     "{}_P{}_{}".format(name, floor, side), folder)

        # Forjado del piso superior, con hueco para la rampa.
        if floor > 0:
            slab_z = z0 - 20.0
            if ramp:
                hole_x0, hole_x1 = x0 + 150.0, x0 + 1050.0
                hole_y0 = y1 - 300.0
                b.box((hole_x1 + x1) / 2.0, cy, slab_z, x1 - hole_x1, depth, 20.0, "Hormigon",
                      "{}_Forjado{}_a".format(name, floor), folder)
                b.box((x0 + hole_x0) / 2.0, cy, slab_z, hole_x0 - x0, depth, 20.0, "Hormigon",
                      "{}_Forjado{}_b".format(name, floor), folder)
                b.box((hole_x0 + hole_x1) / 2.0, (y0 + hole_y0) / 2.0, slab_z, hole_x1 - hole_x0,
                      hole_y0 - y0, 20.0, "Hormigon", "{}_Forjado{}_c".format(name, floor), folder)
            else:
                b.box(cx, cy, slab_z, width, depth, 20.0, "Hormigon", "{}_Forjado{}".format(name, floor), folder)

    # Rampa interior pegada al muro norte: 26° de pendiente, transitable.
    if ramp and floors > 1:
        run, rise = 900.0, floor_height
        length = math.hypot(run, rise)
        angle = math.degrees(math.atan2(rise, run))
        rx = x0 + 150.0 + run / 2.0
        ry = y1 - 160.0
        b.mesh(b.cube, (rx, ry, rise / 2.0 - 10.0), (length, 220.0, 20.0), "Hormigon",
               "{}_Rampa".format(name), folder, yaw=0.0, pitch=angle)

    # Cubierta con pretil: se ve desde la calle como un edificio acabado.
    top = floors * floor_height
    b.box(cx, cy, top, width + 40.0, depth + 40.0, 30.0, "Hormigon", "{}_Cubierta".format(name), folder)
    b.box(cx, cy, top + 30.0, width + 40.0, depth + 40.0, 4.0, "YesoGris", "{}_Remate".format(name), folder)


def solid_block(b, name, cx, cy, width, depth, height, material, folder="Fondo"):
    """Edificio de fondo, macizo: no se entra, solo cierra el horizonte."""
    b.box(cx, cy, 0.0, width, depth, height, material, name, folder)
    b.box(cx, cy, height, width + 30.0, depth + 30.0, 40.0, "Hormigon", name + "_Cornisa", folder)


def car(b, name, x, y, yaw):
    """Coche abandonado: chasis y cabina. Cobertura que detiene balas ligeras."""
    rad = math.radians(yaw)
    b.box(x, y, 25.0, 440.0, 180.0, 80.0, "Coche", name + "_Chasis", "Calle", yaw=yaw)
    cabin_x = x - math.cos(rad) * 30.0
    cabin_y = y - math.sin(rad) * 30.0
    b.box(cabin_x, cabin_y, 105.0, 230.0, 165.0, 55.0, "Coche", name + "_Cabina", "Calle", yaw=yaw)
    for side in (-1, 1):
        for axle in (-1, 1):
            wx = x + math.cos(rad) * 140.0 * axle - math.sin(rad) * 80.0 * side
            wy = y + math.sin(rad) * 140.0 * axle + math.cos(rad) * 80.0 * side
            b.box(wx, wy, 0.0, 65.0, 25.0, 65.0, "Asfalto", "{}_Rueda{}{}".format(name, side, axle), "Calle", yaw=yaw)


def container(b, name, x, y, yaw, color, stacked=False):
    b.box(x, y, 0.0, 606.0, 244.0, 259.0, color, name, "Contenedores", yaw=yaw)
    if stacked:
        b.box(x, y, 259.0, 606.0, 244.0, 259.0, random.choice(("MetalRojo", "MetalAzul", "MetalVerde", "Oxido")),
              name + "_Alto", "Contenedores", yaw=yaw)


def sandbags(b, name, x, y, yaw, length=300.0):
    b.box(x, y, 0.0, length, 60.0, 75.0, "Arena", name, "Coberturas", yaw=yaw)


def jersey(b, name, x, y, yaw):
    b.box(x, y, 0.0, 300.0, 60.0, 85.0, "Hormigon", name, "Coberturas", yaw=yaw)


def crate(b, name, x, y, size=110.0, stack=1):
    for level in range(stack):
        b.box(x, y, level * size, size, size, size, "Madera", "{}_{}".format(name, level), "Coberturas",
              yaw=random.uniform(-8.0, 8.0))


def wood_fence(b, name, x, y, length, along_x=True):
    """Valla de tablones de 4 cm: oculta pero no protege. Se atraviesa."""
    sx, sy = (length, 4.0) if along_x else (4.0, length)
    b.box(x, y, 0.0, sx, sy, 190.0, "Madera", name, "Coberturas")


def sheet_fence(b, name, x, y, length, along_x=True):
    """Chapa ondulada: igual que la valla, ocultación sin protección."""
    sx, sy = (length, 3.0) if along_x else (3.0, length)
    b.box(x, y, 0.0, sx, sy, 220.0, "Oxido", name, "Coberturas")


# --- Distribución ---------------------------------------------------------
#
# 160 x 120 m. Eje X: de la base Alfa (oeste, x-) a la base Bravo (este,
# x+). Tres carriles paralelos al eje X:
#   norte  (y ~ +4100): callejón estrecho, combate cercano
#   centro (y ~ 0):     avenida de 16 m con plaza, distancia media
#   sur    (y ~ -3800): patio industrial con nave y contenedores, media-larga
# Los edificios entre carriles tienen puertas a ambos lados: rotar entre
# carriles es posible pero expone.

HALF_X, HALF_Y = 8000.0, 6000.0


def build_ground(b):
    b.box(0, 0, -100.0, HALF_X * 2 + 2000, HALF_Y * 2 + 2000, 100.0, "Hormigon", "Suelo_Base", "Suelo")
    b.box(0, 0, -99.0, HALF_X * 2, 1600.0, 100.0, "Asfalto", "Avenida", "Suelo")
    for side in (-1, 1):
        b.box(0, side * 1000.0, 0.0, HALF_X * 2, 400.0, 15.0, "Acera", "Acera_{}".format(side), "Suelo")
    b.box(0, -3600.0, -99.0, HALF_X * 2, 4800.0, 100.0, "Tierra", "Patio_Tierra", "Suelo")
    b.box(0, 4100.0, -99.0, HALF_X * 2, 1400.0, 100.0, "Acera", "Callejon_Suelo", "Suelo")


def build_perimeter(b):
    """Fachadas altas alrededor: cierran el mapa sin muros invisibles."""
    heights = (1400.0, 1100.0, 1700.0, 1250.0, 1550.0, 1000.0, 1350.0, 1600.0)
    finishes = ("Yeso", "Ladrillo", "YesoGris", "Hormigon")
    index = 0
    for side in (-1, 1):
        x = -HALF_X
        while x < HALF_X:
            width = random.choice((1600.0, 2000.0, 2400.0))
            width = min(width, HALF_X - x)
            solid_block(b, "Fondo_NS_{}".format(index), x + width / 2.0, side * (HALF_Y + 400.0),
                        width, 800.0, heights[index % len(heights)], finishes[index % len(finishes)])
            x += width
            index += 1
        y = -HALF_Y
        while y < HALF_Y:
            depth = min(random.choice((2000.0, 2400.0, 3000.0)), HALF_Y - y)
            solid_block(b, "Fondo_EW_{}".format(index), side * (HALF_X + 400.0), y + depth / 2.0,
                        800.0, depth, heights[(index + 3) % len(heights)], finishes[(index + 1) % len(finishes)])
            y += depth
            index += 1


def build_north_band(b):
    """Edificios entre la avenida y el callejón, y naves al fondo."""
    # Fachadas a la avenida con ventanas: posiciones de tiro sobre la calle.
    building(b, "Bloque_NO", -5200.0, 2400.0, 2400.0, 1800.0, 1, "Ladrillo",
             doors={"S": [-500.0], "N": [600.0]}, windows={"S": [300.0, 800.0], "N": [-600.0], "E": [0.0]},
             floor_height=420.0)
    building(b, "Bloque_Central", -1200.0, 2400.0, 3000.0, 1800.0, 2, "Yeso",
             doors={"S": [600.0], "N": [-900.0], "E": [-300.0]},
             windows={"S": [-1100.0, -500.0, 0.0, 1100.0], "N": [300.0, 900.0], "W": [-400.0, 400.0]},
             ramp=True)
    building(b, "Bloque_NE", 2600.0, 2400.0, 2200.0, 1800.0, 1, "YesoGris",
             doors={"S": [0.0], "W": [400.0], "N": [-500.0]}, windows={"S": [-700.0, 700.0], "N": [500.0]},
             floor_height=420.0)
    building(b, "Bloque_E", 5800.0, 2400.0, 1800.0, 1800.0, 1, "Ladrillo",
             doors={"S": [-300.0], "N": [300.0]}, windows={"S": [400.0], "W": [0.0]},
             floor_height=420.0)

    # Naves macizas al norte del callejón.
    x = -7400.0
    index = 0
    while x < 7400.0:
        width = random.choice((1800.0, 2200.0, 2600.0))
        solid_block(b, "Nave_Norte_{}".format(index), x + width / 2.0, 5400.0, width, 1200.0,
                    random.choice((600.0, 750.0, 900.0)), random.choice(("Ladrillo", "YesoGris", "Oxido")),
                    folder="Callejon")
        x += width + 200.0
        index += 1


def build_alley(b):
    """Callejón norte: estrecho, con obstáculos que obligan a moverse de uno en uno."""
    for index, x in enumerate((-6200.0, -3600.0, -600.0, 1600.0, 4400.0, 6400.0)):
        b.box(x, 4700.0, 0.0, 190.0, 110.0, 125.0, random.choice(("MetalVerde", "MetalAzul")),
              "Contenedor_Basura_{}".format(index), "Callejon", yaw=random.uniform(-15.0, 15.0))
    for index, (x, y, stack) in enumerate(((-4800.0, 3700.0, 2), (-2400.0, 4500.0, 1), (900.0, 3650.0, 2),
                                           (3300.0, 4550.0, 1), (5200.0, 3750.0, 3))):
        crate(b, "Cajas_{}".format(index), x, y, stack=stack)
    # Vallas de madera y chapa: tapan la vista pero las balas pasan.
    wood_fence(b, "Valla_Madera_0", -3000.0, 4100.0, 600.0, along_x=False)
    wood_fence(b, "Valla_Madera_1", 3900.0, 4100.0, 600.0, along_x=False)
    sheet_fence(b, "Valla_Chapa_0", 400.0, 4300.0, 500.0, along_x=False)


def build_avenue(b):
    """Avenida central: coches, barreras y la plaza con el monumento."""
    for index, (x, y, yaw) in enumerate(((-6000.0, -350.0, 12.0), (-4300.0, 420.0, -20.0),
                                         (-2600.0, -150.0, 35.0), (2300.0, 300.0, -30.0),
                                         (3900.0, -420.0, 8.0), (5800.0, 250.0, -15.0))):
        car(b, "Coche_{}".format(index), x, y, yaw)

    for index, (x, y, yaw) in enumerate(((-3400.0, 600.0, 0.0), (-1700.0, -550.0, 20.0),
                                         (1700.0, 550.0, -20.0), (3300.0, -600.0, 0.0))):
        jersey(b, "Barrera_{}".format(index), x, y, yaw)

    # Plaza (zona B): monumento con escalones como cobertura de 360°.
    b.box(0, 0, 0.0, 700.0, 700.0, 45.0, "Hormigon", "Monumento_Base", "Plaza")
    b.box(0, 0, 45.0, 420.0, 420.0, 90.0, "Hormigon", "Monumento_Escalon", "Plaza")
    b.box(0, 0, 135.0, 220.0, 220.0, 260.0, "YesoGris", "Monumento_Pedestal", "Plaza")
    b.cylinder_at(0, 0, 395.0, 90.0, 600.0, "Hormigon", "Monumento_Columna", "Plaza")

    # Nidos de sacos terreros a la salida de cada base.
    for side in (-1, 1):
        sandbags(b, "Sacos_Base_{}_a".format(side), side * 5000.0, 700.0, 90.0, 260.0)
        sandbags(b, "Sacos_Base_{}_b".format(side), side * 5000.0, -700.0, 90.0, 260.0)

    # Farolas: verticales que rompen la avenida y dan escala.
    for index, x in enumerate(range(-7000, 7001, 1750)):
        for side in (-1, 1):
            b.cylinder_at(float(x), side * 1150.0, 15.0, 18.0, 650.0, "Oxido",
                          "Farola_{}_{}".format(index, side), "Calle")


def build_industrial(b):
    """Patio sur: nave grande (zona C), contenedores y un muro con huecos."""
    building(b, "Nave_Industrial", 0.0, -3800.0, 4200.0, 2600.0, 1, "Oxido",
             doors={"N": [-1200.0, 0.0, 1200.0], "S": [-800.0, 800.0], "W": [0.0], "E": [0.0]},
             windows={"N": [-600.0, 600.0], "S": [0.0]}, floor_height=700.0, folder="Industrial")
    # Interior de la nave: estanterías y palés para el combate cercano.
    for index, (x, y) in enumerate(((-1300.0, -3400.0), (-1300.0, -4300.0), (1300.0, -3400.0), (1300.0, -4300.0))):
        b.box(x, y, 0.0, 900.0, 120.0, 260.0, "Oxido", "Estanteria_{}".format(index), "Industrial")
    for index, (x, y) in enumerate(((-400.0, -3300.0), (500.0, -4500.0), (0.0, -3900.0))):
        crate(b, "Pales_{}".format(index), x, y, size=120.0, stack=1 + index % 2)

    # Muro del patio con huecos: separa la avenida del patio.
    for index, (x, length) in enumerate(((-6800.0, 2000.0), (-3700.0, 2400.0), (-500.0, 1200.0),
                                         (2700.0, 2400.0), (6000.0, 2400.0))):
        b.box(x, -1350.0, 0.0, length, 30.0, 240.0, "Hormigon", "Muro_Patio_{}".format(index), "Industrial")

    # Contenedores a ambos lados de la nave, algunos apilados.
    colors = ("MetalRojo", "MetalAzul", "MetalVerde", "Oxido")
    layout = (
        (-6600.0, -2400.0, 0.0, True), (-6600.0, -2700.0, 0.0, False), (-5200.0, -4200.0, 90.0, True),
        (-4800.0, -5300.0, 0.0, False), (-3600.0, -2500.0, 15.0, False), (-3400.0, -4700.0, 90.0, True),
        (3500.0, -2400.0, -10.0, False), (3600.0, -4800.0, 90.0, True), (4900.0, -3100.0, 0.0, True),
        (5200.0, -5200.0, 0.0, False), (6600.0, -2500.0, 0.0, False), (6600.0, -4200.0, 90.0, True),
    )
    for index, (x, y, yaw, stacked) in enumerate(layout):
        container(b, "Contenedor_{}".format(index), x, y, yaw, colors[index % len(colors)], stacked)

    for index, (x, y, yaw) in enumerate(((-2300.0, -2200.0, 0.0), (2300.0, -2200.0, 0.0),
                                         (-2400.0, -5400.0, 90.0), (2400.0, -5400.0, 90.0))):
        sandbags(b, "Sacos_Patio_{}".format(index), x, y, yaw)


def build_gameplay(b):
    # Apariciones: seis por equipo, en los tres carriles de su base.
    spawns = 0
    for team, x, yaw in (("Alfa", -7300.0, 0.0), ("Bravo", 7300.0, 180.0)):
        for index, y in enumerate((-4600.0, -2600.0, -400.0, 400.0, 3000.0, 4200.0)):
            if b.actor(unreal.PlayerStart, (x, y, 110.0), (0.0, yaw, 0.0),
                       "Aparicion_{}_{}".format(team, index), "Apariciones"):
                spawns += 1
    log("apariciones: {}".format(spawns))

    # Zonas de Dominación, una por carril.
    zone_class = getattr(unreal, "ControlZone", None)
    if zone_class:
        for label, (x, y) in (("A", (0.0, 4100.0)), ("B", (0.0, 0.0)), ("C", (0.0, -3800.0))):
            zone = b.actor(zone_class, (x, y, 150.0), label="Zona_" + label, folder="Dominacion")
            if zone:
                set_props(zone, {"zone_label": label, "zone_radius": 450.0}, "Zona_" + label)
    else:
        warn("ControlZone no expuesta: coloca las zonas a mano")

    # Navegación para los bots. Un volumen creado con spawn_actor_from_class
    # nace sin geometría de pincel; con la factory sí la tiene.
    nav = None
    try:
        nav = ACTOR_SUB.spawn_actor_from_object(unreal.NavMeshBoundsVolume.static_class(),
                                                unreal.Vector(0, 0, 300))
    except Exception:
        nav = None
    if not nav:
        nav = b.actor(unreal.NavMeshBoundsVolume, (0.0, 0.0, 300.0), label="Navegacion")
    if nav:
        b.tag(nav, "Navegacion", "Logica")
        origin, extent = nav.get_actor_bounds(False)
        if extent.x < 1.0:
            warn("el volumen de navegación quedó sin forma: escálalo a mano para cubrir el mapa")
        else:
            nav.set_actor_scale3d(unreal.Vector(HALF_X / extent.x, HALF_Y / extent.y, 1000.0 / extent.z))
            log("navegación: volumen de {:.0f} x {:.0f} m".format(HALF_X / 50.0, HALF_Y / 50.0))


def build_atmosphere(b):
    """Tarde despejada con bruma: sol bajo, sombras largas, aire con volumen."""
    sun = b.actor(unreal.DirectionalLight, (0.0, 0.0, 3000.0), (-32.0, 35.0, 0.0), "Sol", "Iluminacion")
    if sun:
        light = sun.get_component_by_class(unreal.DirectionalLightComponent)
        set_props(light, {
            "mobility": unreal.ComponentMobility.MOVABLE,
            "intensity": 10.0,
            "use_temperature": True,
            "temperature": 5200.0,
            "atmosphere_sun_light": True,
            "light_source_angle": 0.53,
            "cast_cloud_shadows": True,
        }, "Sol")

    sky = b.actor(unreal.SkyLight, (0.0, 0.0, 1000.0), label="Luz_Cielo", folder="Iluminacion")
    if sky:
        component = sky.get_component_by_class(unreal.SkyLightComponent)
        set_props(component, {"mobility": unreal.ComponentMobility.MOVABLE, "real_time_capture": True},
                  "Luz_Cielo")

    b.actor(unreal.SkyAtmosphere, (0.0, 0.0, 0.0), label="Atmosfera", folder="Iluminacion")

    cloud_class = getattr(unreal, "VolumetricCloud", None)
    if cloud_class:
        b.actor(cloud_class, (0.0, 0.0, 0.0), label="Nubes", folder="Iluminacion")

    fog = b.actor(unreal.ExponentialHeightFog, (0.0, 0.0, -100.0), label="Niebla", folder="Iluminacion")
    if fog:
        component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
        # En 5.8 la propiedad es bEnableVolumetricFog, no bVolumetricFog.
        set_props(component, {
            "fog_density": 0.015,
            "fog_height_falloff": 0.25,
            "enable_volumetric_fog": True,
        }, "Niebla")

    volume = b.actor(unreal.PostProcessVolume, (0.0, 0.0, 500.0), label="PostProceso", folder="Iluminacion")
    if volume:
        volume.set_editor_property("unbound", True)
        settings = volume.get_editor_property("settings")
        for name, value in (
            # Sin compensación, la exposición automática quema las fachadas
            # claras a pleno sol y el color se lava.
            ("auto_exposure_bias", -1.0),
            ("bloom_intensity", 0.45),
            ("vignette_intensity", 0.3),
            ("film_grain_intensity", 0.12),
            ("motion_blur_amount", 0.0),
            ("ambient_occlusion_intensity", 0.6),
            ("white_temp", 6300.0),
            ("lumen_final_gather_quality", 1.5),
            ("lumen_reflection_quality", 1.0),
        ):
            try:
                settings.set_editor_property("override_" + name, True)
                settings.set_editor_property(name, value)
            except Exception as error:
                warn("post-proceso {}: {}".format(name, error))
        volume.set_editor_property("settings", settings)


def clear_generated():
    removed = [actor for actor in ACTOR_SUB.get_all_level_actors()
               if TAG in [str(tag) for tag in actor.tags]]
    if removed:
        ACTOR_SUB.destroy_actors(removed)
    log("actores generados anteriormente eliminados: {}".format(len(removed)))


def open_level():
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
        LEVEL_SUB.load_level(LEVEL_PATH)
        clear_generated()
        log("nivel existente abierto: {}".format(LEVEL_PATH))
    else:
        LEVEL_SUB.new_level(LEVEL_PATH)
        log("nivel creado: {}".format(LEVEL_PATH))


def set_level_game_mode(mode_bp):
    generated = blueprint_class(mode_bp)
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        settings = world.get_world_settings()
        if settings and generated:
            settings.set_editor_property("default_game_mode", generated)
            log("GameMode del nivel -> BP_ModoRealista")
    except Exception as error:
        warn("asigna BP_ModoRealista a mano en World Settings: {}".format(error))


# ==========================================================================

def run():
    log("motor: {}".format(unreal.SystemLibrary.get_engine_version()))
    ensure_dirs(REAL_DIR, MAT_DIR, PHYS_DIR, DATA_DIR, WEAPONS_BP_DIR, CORE_DIR, "/Game/Maps")

    log("--- materiales ---")
    physical = build_physical_materials()
    materials = build_surfaces(physical)

    log("--- armas y jugador ---")
    player_bp = load(CORE_DIR + "/BP_Jugador")
    if not player_bp:
        warn("falta BP_Jugador: ejecuta antes build_training_zone.py")
    weapon_bps = build_weapons()
    mode_bp = None
    if player_bp:
        build_input(player_bp)
        configure_player(player_bp, weapon_bps)
        compile_and_save(player_bp, "BP_Jugador")
        mode_bp = build_game_mode(player_bp)

    log("--- nivel ---")
    open_level()
    b = Builder(materials, physical)
    build_ground(b)
    build_perimeter(b)
    build_north_band(b)
    build_alley(b)
    build_avenue(b)
    build_industrial(b)
    build_gameplay(b)
    build_atmosphere(b)
    if mode_bp:
        set_level_game_mode(mode_bp)
    log("piezas de geometría: {}".format(b.count))

    LEVEL_SUB.save_current_level()
    log("")
    log("Distrito listo. Pulsa Play (Net Mode: Play As Client para probar en red).")
    log("Controles nuevos: Q/E asomarse, B selector, H vendarse, G granada,")
    log("Shift apuntando = contener la respiración, rueda o 1 = cambiar arma.")


try:
    run()
except Exception as error:
    import traceback
    unreal.log_error("[distrito] FALLÓ: {}".format(error))
    unreal.log_error(traceback.format_exc())
    unreal.log_error("[distrito] Copia este error y pásalo para diagnosticarlo.")
