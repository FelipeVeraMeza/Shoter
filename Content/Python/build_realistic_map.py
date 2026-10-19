"""
Construye el mapa realista "Distrito", el decorado del menú y deja el juego
configurado para ellos.

Qué hace, en orden:

  1. Materiales físicos con resistencia balística (hormigón, ladrillo,
     madera, chapa...): deciden qué atraviesa una bala.
  2. Materiales PBR: un material maestro procedural (variación de color y
     rugosidad en coordenadas de mundo, sin texturas) y sus instancias.
     Si el proyecto tiene materiales de Fab/Megascans, se usan esos.
  3. Materiales de efectos (/Game/Realista/Efectos): fogonazo, trazadora,
     retícula holográfica, marca de impacto, polvo/humo y fuego. Los usa
     el código C++ (ShooterEffects) en lugar de sistemas Niagara.
  4. Armas realistas: fusil de asalto 5,56 con visor holográfico, pistola
     9 mm y fusil de tirador 7,62 con visor de aumentos, con balística,
     animaciones de brazos y del cuerpo, y sonido.
  5. Controles: asomarse (Q/E), selector de fuego (B), vendarse (H),
     granada (G) y pausa (P/Esc). Cambiar de arma pasa a la rueda y al 1.
  6. Jugador: cuerpo animado con el fusil (ABP_TP_Rifle de la plantilla),
     Manny para Alfa y Quinn para Bravo, granadas.
  7. Modo realista: equipos, bots, armas elegibles en el equipamiento.
  8. /Game/Maps/Lvl_Distrito: barrio en ruinas de 160 x 120 m con tres
     carriles, edificios con interiores, coches, autobús calcinado, fuegos
     con humo, sacos terreros, escombros, cráteres, farolas, tendido
     eléctrico, luz dorada de atardecer con bruma y rayos de sol.
  9. /Game/Maps/Lvl_Menu: decorado con cámara para el menú principal.

Ejecutar en la consola Python del editor:
    exec(open(r"<proyecto>/Content/Python/build_realistic_map.py").read())

Es idempotente: reejecutarlo actualiza los assets y reconstruye solo los
actores que generó él mismo (etiqueta "ShoterGenerado"); lo que se haya
añadido a mano a los niveles se conserva.
"""

import math
import random

import unreal

TAG = "ShoterGenerado"

LEVEL_PATH = "/Game/Maps/Lvl_Distrito"
MENU_LEVEL_PATH = "/Game/Maps/Lvl_Menu"
REAL_DIR = "/Game/Realista"
MAT_DIR = REAL_DIR + "/Materiales"
PHYS_DIR = REAL_DIR + "/Fisica"
FX_DIR = REAL_DIR + "/Efectos"
DATA_DIR = "/Game/Weapons"
WEAPONS_BP_DIR = "/Game/Blueprints/Weapons"
CORE_DIR = "/Game/Blueprints/Core"
INPUT_DIR = "/Game/Input"
ACTIONS_DIR = "/Game/Input/Actions"

CUBE = "/Engine/BasicShapes/Cube.Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder.Cylinder"
SPHERE = "/Engine/BasicShapes/Sphere.Sphere"

ANIMS = "/Game/Characters/Mannequins/Anims"
MANNY = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
QUINN = "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"
ABP_TP_RIFLE = "/Game/Variant_Shooter/Anims/ABP_TP_Rifle"
ABP_TP_PISTOL = "/Game/Variant_Shooter/Anims/ABP_TP_Pistol"
GUNSHOT = "/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02"

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LEVEL_SUB = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTOR_SUB = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MEL = unreal.MaterialEditingLibrary


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
    if obj is None:
        warn("{}: objeto inexistente".format(label))
        return
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
    # Añadidas para el detalle del mapa, el arma y los efectos.
    "Escombro":   ((0.36, 0.34, 0.31), (0.22, 0.21, 0.19), 0.9, 1.0, 0.0, 0.02, "Hormigon", ("rubble", "debris")),
    "Quemado":    ((0.03, 0.025, 0.022), (0.1, 0.065, 0.04), 0.85, 0.97, 0.3, 0.006, "Vehiculo", ("burnt", "charred")),
    "Goma":       ((0.02, 0.02, 0.02), (0.035, 0.035, 0.035), 0.8, 0.92, 0.0, 0.01, "Vehiculo", ("rubber", "tire")),
    "Cristal":    ((0.01, 0.012, 0.014), (0.02, 0.025, 0.028), 0.08, 0.2, 0.0, 0.01, "Vehiculo", ("glass",)),
    "MarcaVial":  ((0.72, 0.7, 0.62), (0.52, 0.5, 0.45), 0.6, 0.8, 0.0, 0.01, "Asfalto", ("road_marking", "paint")),
    "Lona":       ((0.13, 0.15, 0.08), (0.08, 0.1, 0.06), 0.85, 0.95, 0.0, 0.01, "Madera", ("tarp", "canvas")),
    "Teja":       ((0.35, 0.12, 0.06), (0.25, 0.1, 0.06), 0.75, 0.9, 0.0, 0.006, "Ladrillo", ("roof_tile", "terracotta")),
    "Laton":      ((0.75, 0.55, 0.2), (0.58, 0.4, 0.14), 0.28, 0.45, 1.0, 0.05, "Chapa", ()),
    "MetalNegro": ((0.025, 0.025, 0.027), (0.045, 0.045, 0.048), 0.42, 0.6, 0.6, 0.05, "Chapa", ()),
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


class Graph(object):
    """Ayuda para montar un grafo de material sin repetir llamadas."""

    def __init__(self, material):
        self.material = material
        self.y = 0

    def node(self, cls, x=-600, **props):
        self.y += 140
        expression = MEL.create_material_expression(self.material, cls, x, self.y - 1200)
        for name, value in props.items():
            try:
                expression.set_editor_property(name, value)
            except Exception as error:
                warn("{}.{}: {}".format(cls.__name__, name, error))
        return expression

    def link(self, source, target, target_input="", source_output=""):
        MEL.connect_material_expressions(source, source_output, target, target_input)
        return target

    def output(self, source, material_property, source_output=""):
        MEL.connect_material_property(source, source_output, material_property)

    # --- Nodos frecuentes ---

    def const(self, value):
        return self.node(unreal.MaterialExpressionConstant, r=value)

    def color(self, rgb):
        return self.node(unreal.MaterialExpressionConstant3Vector,
                         constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

    def multiply(self, a, b=None, const_b=1.0):
        node = self.node(unreal.MaterialExpressionMultiply, const_b=const_b)
        self.link(a, node, "A")
        if b is not None:
            self.link(b, node, "B")
        return node

    def add(self, a, b=None, const_b=0.0):
        node = self.node(unreal.MaterialExpressionAdd, const_b=const_b)
        self.link(a, node, "A")
        if b is not None:
            self.link(b, node, "B")
        return node

    def subtract(self, a, b=None, const_b=0.0):
        node = self.node(unreal.MaterialExpressionSubtract, const_b=const_b)
        self.link(a, node, "A")
        if b is not None:
            self.link(b, node, "B")
        return node

    def one_minus(self, a):
        return self.link(a, self.node(unreal.MaterialExpressionOneMinus))

    def saturate(self, a):
        return self.link(a, self.node(unreal.MaterialExpressionSaturate))

    def power(self, a, exponent):
        node = self.node(unreal.MaterialExpressionPower, const_exponent=exponent)
        return self.link(a, node, "Base")

    def lerp(self, a, b, alpha):
        node = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, node, "A")
        self.link(b, node, "B")
        self.link(alpha, node, "Alpha")
        return node

    def maximum(self, a, b):
        node = self.node(unreal.MaterialExpressionMax)
        self.link(a, node, "A")
        self.link(b, node, "B")
        return node

    def radial(self):
        """Distancia al centro de las UV, de 0 (centro) a 1 (borde)."""
        uv = self.node(unreal.MaterialExpressionTextureCoordinate, x=-1400)
        center = self.node(unreal.MaterialExpressionConstant2Vector, x=-1400, r=0.5, g=0.5)
        distance = self.node(unreal.MaterialExpressionDistance)
        self.link(uv, distance, "A")
        self.link(center, distance, "B")
        return self.multiply(distance, const_b=2.0)

    def scalar_param(self, name, value):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)

    def vector_param(self, name, rgb):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                         default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


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
                      if keywords and any(keyword in asset_name for keyword in keywords)), None)
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
# 3. Materiales de efectos
# ==========================================================================
#
# Los busca el C++ por ruta (Effects/ShooterEffects.cpp): fogonazos,
# trazadoras, retícula, marcas de impacto, polvo y humo, fuego. Se rehacen
# enteros en cada ejecución para que un cambio aquí llegue siempre.

def fresh_material(name, settings):
    path = FX_DIR + "/" + name
    material = load(path)
    if material:
        MEL.delete_all_material_expressions(material)
    else:
        material = ASSET_TOOLS.create_asset(name, FX_DIR, unreal.Material, unreal.MaterialFactoryNew())
    set_props(material, settings, name)
    return material, Graph(material)


def finish_material(material, name):
    MEL.recompile_material(material)
    save(material)
    log("efecto {} listo".format(name))


def build_effect_materials():
    blend = unreal.BlendMode
    shading = unreal.MaterialShadingModel
    prop = unreal.MaterialProperty

    additive = {"blend_mode": blend.BLEND_ADDITIVE, "shading_model": shading.MSM_UNLIT, "two_sided": True}

    # Fogonazo: núcleo blanco-amarillo que se apaga hacia el borde.
    material, g = fresh_material("M_Fogonazo", additive)
    falloff = g.power(g.saturate(g.one_minus(g.radial())), 2.4)
    hot = g.lerp(g.color((1.0, 0.42, 0.1)), g.color((1.0, 0.85, 0.55)), g.power(falloff, 3.0))
    g.output(g.multiply(g.multiply(hot, falloff), const_b=30.0), prop.MP_EMISSIVE_COLOR)
    finish_material(material, "M_Fogonazo")

    # Trazadora: fósforo incandescente, naranja rojizo.
    material, g = fresh_material("M_Trazadora", additive)
    g.output(g.multiply(g.color((1.0, 0.32, 0.07)), const_b=45.0), prop.MP_EMISSIVE_COLOR)
    finish_material(material, "M_Trazadora")

    # Retícula holográfica: punto central y anillo rojos sobre un cristal
    # apenas tintado. Unlit translúcido: brilla igual a la sombra.
    material, g = fresh_material("M_Reticula", {
        "blend_mode": blend.BLEND_TRANSLUCENT, "shading_model": shading.MSM_UNLIT, "two_sided": True})
    r = g.radial()
    dot = g.saturate(g.multiply(g.subtract(g.const(0.035), r), const_b=220.0))
    ring_distance = g.link(g.subtract(r, const_b=0.36), g.node(unreal.MaterialExpressionAbs))
    ring = g.saturate(g.multiply(g.subtract(g.const(0.014), ring_distance), const_b=220.0))
    reticle = g.maximum(dot, ring)
    glass = g.multiply(g.color((0.25, 0.4, 0.38)), const_b=0.04)
    g.output(g.lerp(glass, g.multiply(g.color((1.0, 0.04, 0.02)), const_b=25.0), reticle), prop.MP_EMISSIVE_COLOR)
    g.output(g.add(g.multiply(reticle, const_b=0.9), const_b=0.06), prop.MP_OPACITY)
    finish_material(material, "M_Reticula")

    # Marca de impacto: agujero oscuro con un halo desconchado.
    material, g = fresh_material("M_Impacto", {
        "material_domain": unreal.MaterialDomain.MD_DEFERRED_DECAL, "blend_mode": blend.BLEND_TRANSLUCENT})
    r = g.radial()
    hole = g.saturate(g.multiply(g.subtract(g.const(0.3), r), const_b=8.0))
    halo = g.power(g.saturate(g.one_minus(r)), 1.4)
    g.output(g.lerp(g.color((0.2, 0.19, 0.18)), g.color((0.012, 0.012, 0.012)), hole), prop.MP_BASE_COLOR)
    g.output(g.const(0.95), prop.MP_ROUGHNESS)
    g.output(g.saturate(g.maximum(hole, g.multiply(halo, const_b=0.75))), prop.MP_OPACITY)
    finish_material(material, "M_Impacto")

    # Polvo y humo: esfera translúcida de bordes suaves (el centro opaco y
    # el contorno transparente, por Fresnel) con algo de ruido. Parámetros
    # Color y Opacidad: los anima el código.
    material, g = fresh_material("M_Polvo", {
        "blend_mode": blend.BLEND_TRANSLUCENT, "shading_model": shading.MSM_DEFAULT_LIT})
    try:
        material.set_editor_property("translucency_lighting_mode",
                                     unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    except Exception as error:
        warn("M_Polvo sin modo de iluminación translúcida: {}".format(error))
    color = g.vector_param("Color", (0.5, 0.48, 0.45))
    opacity = g.scalar_param("Opacidad", 0.5)
    fresnel = g.node(unreal.MaterialExpressionFresnel, exponent=1.6, base_reflect_fraction=0.0)
    soft = g.power(g.saturate(g.one_minus(fresnel)), 1.5)
    position = g.node(unreal.MaterialExpressionWorldPosition, x=-1400)
    noise = g.node(unreal.MaterialExpressionNoise, scale=1.0, levels=3, output_min=0.55, output_max=1.0, quality=1)
    g.link(g.multiply(position, const_b=0.015), noise, "Position")
    g.output(color, prop.MP_BASE_COLOR)
    g.output(g.const(1.0), prop.MP_ROUGHNESS)
    g.output(g.saturate(g.multiply(g.multiply(soft, noise), opacity)), prop.MP_OPACITY)
    finish_material(material, "M_Polvo")

    # Fuego: llama que se apaga hacia arriba y hacia los lados, con ruido
    # que sube con el tiempo. La altura se mide en el mundo respecto al
    # centro del plano, así que no depende de cómo estén orientadas sus UV.
    material, g = fresh_material("M_Fuego", additive)
    world = g.node(unreal.MaterialExpressionWorldPosition, x=-1600)
    center = g.node(unreal.MaterialExpressionObjectPositionWS, x=-1600)
    radius = g.node(unreal.MaterialExpressionObjectRadius, x=-1600)
    offset = g.subtract(world, center)
    height = g.node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
    g.link(offset, height)
    divide = g.node(unreal.MaterialExpressionDivide)
    g.link(height, divide, "A")
    g.link(radius, divide, "B")
    vertical = g.saturate(g.subtract(g.const(0.55), divide))
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, x=-1600)
    u = g.node(unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
    g.link(uv, u)
    across = g.link(g.subtract(u, const_b=0.5), g.node(unreal.MaterialExpressionAbs))
    sides = g.power(g.saturate(g.one_minus(g.multiply(across, const_b=2.0))), 1.4)
    time = g.node(unreal.MaterialExpressionTime, x=-1600)
    rising = g.multiply(time, g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.0, 0.0, 1.8, 1.0)))
    flow = g.subtract(g.multiply(world, const_b=0.022), rising)
    noise = g.node(unreal.MaterialExpressionNoise, scale=1.0, levels=4, output_min=0.0, output_max=1.0, quality=1)
    g.link(flow, noise, "Position")
    shape = g.saturate(g.subtract(g.multiply(g.multiply(g.multiply(vertical, sides), noise), const_b=2.6), const_b=0.12))
    flame_color = g.lerp(g.color((1.0, 0.22, 0.03)), g.color((1.0, 0.7, 0.28)), shape)
    g.output(g.multiply(g.multiply(flame_color, shape), const_b=22.0), prop.MP_EMISSIVE_COLOR)
    finish_material(material, "M_Fuego")


# ==========================================================================
# 4. Armas
# ==========================================================================

FM = unreal.WeaponFireMode

WEAPONS = {
    # Fusil de asalto 5,56x45 con visor holográfico: tres impactos al torso,
    # uno a la cabeza.
    "DA_Rifle": {
        "bp": "BP_Rifle",
        "mesh": "/Game/Weapons/Rifle/Meshes/SKM_Rifle",
        "hold": "RIFLE",
        "sight": "HOLOGRAPHIC",
        "idle": ANIMS + "/Rifle/MF_Rifle_Idle_ADS",
        "reload": ANIMS + "/Rifle/MM_Rifle_Reload",
        "body": ABP_TP_RIFLE,
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
            "hip_sight_offset": unreal.Vector(36.0, 11.0, -7.5), "aim_eye_distance": 24.0,
            "visual_kick_back": 2.0, "visual_kick_pitch": 2.2,
            "tracer_every_n_shots": 3, "shell_length": 4.5, "muzzle_flash_scale": 1.0,
            "fire_sound_pitch": 1.0, "fire_sound_volume": 1.0,
        },
    },
    # Pistola 9x19: cuatro impactos al torso; más lenta y con más caída.
    "DA_Pistola": {
        "bp": "BP_Pistola",
        "mesh": "/Game/Weapons/Pistol/Meshes/SKM_Pistol",
        "hold": "PISTOL",
        "sight": "IRON",
        "idle": ANIMS + "/Pistol/MF_Pistol_Idle_ADS",
        "reload": ANIMS + "/Pistol/MM_Pistol_Reload",
        "body": ABP_TP_PISTOL,
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
            "hip_sight_offset": unreal.Vector(34.0, 9.0, -7.0), "aim_eye_distance": 30.0,
            "visual_kick_back": 1.6, "visual_kick_pitch": 4.0,
            "tracer_every_n_shots": 0, "shell_length": 2.0, "muzzle_flash_scale": 0.6,
            "fire_sound_pitch": 1.25, "fire_sound_volume": 0.8,
        },
    },
    # Fusil de tirador 7,62x51 con visor de aumentos: dos al torso, atraviesa más.
    "DA_Tirador": {
        "bp": "BP_Tirador",
        "mesh": "/Game/Weapons/Rifle/Meshes/SKM_Rifle",
        "hold": "RIFLE",
        "sight": "SCOPE",
        "idle": ANIMS + "/Rifle/MF_Rifle_Idle_ADS",
        "reload": ANIMS + "/Rifle/MM_Rifle_Reload",
        "body": ABP_TP_RIFLE,
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
            "penetration_power": 55.0, "aim_fov": 20.0, "sway_multiplier": 1.4,
            "hip_sight_offset": unreal.Vector(36.0, 11.0, -7.5), "aim_eye_distance": 22.0,
            "visual_kick_back": 3.2, "visual_kick_pitch": 3.5,
            "tracer_every_n_shots": 0, "shell_length": 5.1, "muzzle_flash_scale": 1.3,
            "fire_sound_pitch": 0.78, "fire_sound_volume": 1.3,
        },
    },
}


def load_anim_class(path):
    blueprint = load(path)
    return blueprint_class(blueprint) if blueprint else None


def build_weapons():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.WeaponDataAsset)
    gunshot = load(GUNSHOT)
    if not gunshot:
        warn("sin el sonido de disparo de la plantilla: las armas sonarán mudas")

    blueprints = {}
    for asset_name, spec in WEAPONS.items():
        data, created = create_or_load(DATA_DIR, asset_name, unreal.WeaponDataAsset, factory)
        if not data:
            warn("{} no se pudo crear".format(asset_name))
            continue

        set_props(data, spec["values"], asset_name)

        # Empuñadura y visor: los enums solo existen dentro del editor.
        try:
            data.set_editor_property("hold_type", getattr(unreal.WeaponHoldType, spec["hold"]))
            data.set_editor_property("sight_type", getattr(unreal.WeaponSightType, spec["sight"]))
        except Exception as error:
            warn("{} sin empuñadura o visor: {}".format(asset_name, error))

        # Referencias duras a animaciones y sonido: así se empaquetan con el
        # juego (el C++ tiene rutas de respaldo, pero esas no se cocinan solas).
        extra = {}
        idle = load(spec["idle"])
        reload = load(spec["reload"])
        body = load_anim_class(spec["body"])
        if idle:
            extra["first_person_idle_anim"] = idle
        else:
            warn("{}: falta {}".format(asset_name, spec["idle"]))
        if reload:
            extra["first_person_reload_anim"] = reload
        if body:
            extra["third_person_anim_class"] = body
        else:
            warn("{}: falta {}".format(asset_name, spec["body"]))
        if gunshot and not data.get_editor_property("fire_sound"):
            extra["fire_sound"] = gunshot
        set_props(data, extra, asset_name)

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
# 5. Controles
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


# ==========================================================================
# 6. Jugador y modo de juego
# ==========================================================================

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

    # El cuerpo que ven los demás: maniquí con la pose de fusil de la
    # plantilla (el C++ la cambia a la de pistola al sacar la pistola).
    manny = load(MANNY)
    quinn = load(QUINN)
    rifle_body = load_anim_class(ABP_TP_RIFLE)
    try:
        mesh_component = cdo.get_editor_property("mesh")
        if manny:
            try:
                mesh_component.set_editor_property("skeletal_mesh_asset", manny)
            except Exception:
                mesh_component.set_editor_property("skeletal_mesh", manny)
        if rifle_body:
            mesh_component.set_editor_property("anim_class", rifle_body)
        # El maniquí nace mirando +Y con el origen en los pies.
        mesh_component.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, -96.0))
        mesh_component.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, -90.0))
        log("BP_Jugador -> cuerpo con ABP_TP_Rifle")
    except Exception as error:
        warn("cuerpo sin configurar: {}".format(error))

    set_props(cdo, {
        "alpha_team_mesh": manny,
        "bravo_team_mesh": quinn,
        "grenade_class": unreal.ShooterGrenade,
    }, "BP_Jugador")


def build_game_mode(player_bp, weapon_bps):
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
            "marksman_bot_chance": 0.25,
            "post_match_restart_delay": 25.0,
        }
        set_props(cdo, values, "BP_ModoRealista")
        try:
            cdo.set_editor_property("bot_controller_class", unreal.ShooterBotController)
        except Exception as error:
            warn("bots sin controlador: {}".format(error))

        # Lo que ofrece la pantalla de equipamiento: el primero es el de fábrica.
        options = [blueprint_class(weapon_bps[name]) for name in ("BP_Rifle", "BP_Tirador") if name in weapon_bps]
        set_props(cdo, {"primary_weapon_options": [cls for cls in options if cls]}, "BP_ModoRealista")

        player_class = blueprint_class(player_bp)
        if player_class:
            cdo.set_editor_property("default_pawn_class", player_class)
    compile_and_save(mode_bp, "BP_ModoRealista")
    return mode_bp


# ==========================================================================
# 7. Construcción
# ==========================================================================

class Builder(object):
    """Coloca la geometría y lleva la cuenta de lo creado."""

    def __init__(self, materials, physical, folder_root="Distrito"):
        self.materials = materials
        self.physical = physical
        self.folder_root = folder_root
        self.cube = load(CUBE)
        self.cylinder = load(CYLINDER)
        self.sphere = load(SPHERE)
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

    def tag(self, actor, label, folder):
        actor.set_actor_label(label)
        try:
            actor.set_editor_property("tags", [unreal.Name(TAG)])
        except Exception:
            pass
        try:
            actor.set_folder_path(unreal.Name(self.folder_root + "/" + folder))
        except Exception:
            pass

    def mesh(self, mesh, center, size, material_key, label, folder, yaw=0.0, pitch=0.0, roll=0.0,
             base_size=100.0, collision=True):
        actor = ACTOR_SUB.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(*center), unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll))
        if not actor:
            return None

        component = actor.static_mesh_component
        component.set_mobility(self.mobility)
        component.set_static_mesh(mesh)
        actor.set_actor_scale3d(unreal.Vector(size[0] / base_size, size[1] / base_size, size[2] / base_size))

        material = self.materials.get(material_key)
        if material:
            component.set_material(0, material)

        if not collision:
            # Cables, tubos de andamio y adornos finos: ni detienen balas ni
            # bloquean el paso.
            try:
                component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            except Exception:
                pass
        else:
            # El trazado de balas usa la colisión simple, que no lee el
            # material físico del material visual: hay que fijarlo aquí.
            phys_key = SURFACES.get(material_key, (None,) * 8)[6]
            if phys_key in self.physical:
                try:
                    component.set_phys_material_override(self.physical[phys_key])
                except Exception:
                    pass

        self.tag(actor, label, folder)
        self.count += 1
        return actor

    def box(self, x, y, z, sx, sy, sz, material, label, folder, yaw=0.0, pitch=0.0, roll=0.0, collision=True):
        """Caja con la base en z (no el centro): así se apilan sin cuentas."""
        return self.mesh(self.cube, (x, y, z + sz / 2.0), (sx, sy, sz), material, label, folder,
                         yaw, pitch, roll, collision=collision)

    def cylinder_at(self, x, y, z, diameter, height, material, label, folder, collision=True):
        return self.mesh(self.cylinder, (x, y, z + height / 2.0), (diameter, diameter, height),
                         material, label, folder, collision=collision)

    def tube(self, start, end, diameter, material, label, folder, collision=False):
        """Cilindro entre dos puntos: tuberías, cables, ramas, barandillas."""
        dx, dy, dz = end[0] - start[0], end[1] - start[1], end[2] - start[2]
        length = math.sqrt(dx * dx + dy * dy + dz * dz)
        if length < 1.0:
            return None
        yaw = math.degrees(math.atan2(dy, dx))
        pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
        center = ((start[0] + end[0]) / 2.0, (start[1] + end[1]) / 2.0, (start[2] + end[2]) / 2.0)
        # El cilindro del motor tiene el eje en Z: se tumba (pitch -90) y se
        # orienta con el yaw y el cabeceo del tramo.
        return self.mesh(self.cylinder, center, (diameter, diameter, length), material, label, folder,
                         yaw=yaw, pitch=pitch - 90.0, collision=collision)

    def blob(self, x, y, z, sx, sy, sz, material, label, folder, collision=True):
        return self.mesh(self.sphere, (x, y, z), (sx, sy, sz), material, label, folder, collision=collision)

    def actor(self, cls, location, rotation=(0.0, 0.0, 0.0), label="", folder="Logica"):
        """rotation = (pitch, yaw, roll)."""
        actor = ACTOR_SUB.spawn_actor_from_class(
            cls, unreal.Vector(*location),
            unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]))
        if actor:
            self.tag(actor, label or cls.__name__, folder)
        return actor


def rotate(x, y, yaw):
    rad = math.radians(yaw)
    return x * math.cos(rad) - y * math.sin(rad), x * math.sin(rad) + y * math.cos(rad)


# --- Piezas de construcción ------------------------------------------------

FLOOR_HEIGHT = 380.0
WALL = 25.0

SIDE_NORMALS = {"S": (0.0, -1.0), "N": (0.0, 1.0), "W": (-1.0, 0.0), "E": (1.0, 0.0)}


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


def opening_trim(b, start, direction, normal, z0, openings, label, folder):
    """
    Alféizares, dinteles y marcos alrededor de puertas y ventanas: es lo que
    hace que un hueco en un muro se lea como ventana y no como un agujero.
    """
    dx, dy = direction
    nx, ny = normal
    along_x = abs(dx) > 0
    for index, (center, width, bottom, top) in enumerate(openings):
        x = start[0] + dx * center + nx * (WALL / 2.0 + 4.0)
        y = start[1] + dy * center + ny * (WALL / 2.0 + 4.0)
        span = width + 24.0
        sx, sy = (span, 10.0) if along_x else (10.0, span)
        if bottom > 1.0:
            # Ventana: alféizar saliente y dintel.
            b.box(x, y, z0 + bottom - 7.0, sx * 1.0, sy * 1.6, 7.0, "Hormigon",
                  "{}_Alfeizar{}".format(label, index), folder)
        b.box(x, y, z0 + top, sx, sy, 16.0, "Hormigon", "{}_Dintel{}".format(label, index), folder)
        # Jambas: dos listones verticales a los lados del hueco.
        for side in (-1, 1):
            jx = x + dx * side * (width / 2.0 + 6.0)
            jy = y + dy * side * (width / 2.0 + 6.0)
            jsx, jsy = (12.0, 8.0) if along_x else (8.0, 12.0)
            b.box(jx, jy, z0 + bottom, jsx, jsy, top - bottom, "Madera" if bottom < 1.0 else "Hormigon",
                  "{}_Jamba{}_{}".format(label, index, side), folder)


def building(b, name, cx, cy, width, depth, floors, material, doors=None, windows=None,
             floor_height=FLOOR_HEIGHT, ramp=False, folder="Edificios", roof_props=True, balconies=None):
    """
    Edificio hueco y transitable. doors/windows: {lado: [desplazamientos
    desde el centro del lado]}. Lados: S (y-), N (y+), W (x-), E (x+).
    Las puertas solo van en planta baja; las ventanas, en todas.
    """
    doors = doors or {}
    windows = windows or {}
    balconies = balconies or {}
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
            label = "{}_P{}_{}".format(name, floor, side)
            wall_run(b, start, direction, length, z0, floor_height, openings, material, label, folder)
            opening_trim(b, start, direction, SIDE_NORMALS[side], z0, openings, label, folder)

            # Imposta entre plantas: una banda saliente marca cada forjado.
            if floor > 0:
                nx, ny = SIDE_NORMALS[side]
                mid = length / 2.0
                bx = start[0] + direction[0] * mid + nx * (WALL / 2.0 + 5.0)
                by = start[1] + direction[1] * mid + ny * (WALL / 2.0 + 5.0)
                sx, sy = (length + 30.0, 12.0) if direction[0] else (12.0, length + 30.0)
                b.box(bx, by, z0 - 10.0, sx, sy, 20.0, "Hormigon", "{}_Imposta".format(label), folder)

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

    # Rampa interior pegada al muro norte: 23° de pendiente, transitable.
    if ramp and floors > 1:
        run, rise = 900.0, floor_height
        length = math.hypot(run, rise)
        angle = math.degrees(math.atan2(rise, run))
        rx = x0 + 150.0 + run / 2.0
        ry = y1 - 160.0
        b.mesh(b.cube, (rx, ry, rise / 2.0 - 10.0), (length, 220.0, 20.0), "Hormigon",
               "{}_Rampa".format(name), folder, yaw=0.0, pitch=angle)
        # Barandilla del hueco de la rampa en la planta alta.
        b.box(x0 + 600.0, y1 - 305.0, floor_height, 900.0, 6.0, 100.0, "MetalNegro",
              "{}_Barandilla".format(name), folder)

    # Balcones: losa volada y antepecho de hierro.
    for side, offsets in balconies.items():
        nx, ny = SIDE_NORMALS[side]
        start, direction, length = sides[side]
        for index, offset in enumerate(offsets):
            px = start[0] + direction[0] * (length / 2.0 + offset) + nx * 55.0
            py = start[1] + direction[1] * (length / 2.0 + offset) + ny * 55.0
            sx, sy = (170.0, 90.0) if direction[0] else (90.0, 170.0)
            z = floor_height
            b.box(px, py, z - 14.0, sx, sy, 14.0, "Hormigon", "{}_Balcon{}_{}".format(name, side, index), folder)
            rx = px + nx * 42.0
            ry = py + ny * 42.0
            rsx, rsy = (170.0, 4.0) if direction[0] else (4.0, 170.0)
            b.box(rx, ry, z, rsx, rsy, 95.0, "MetalNegro", "{}_Antepecho{}_{}".format(name, side, index), folder)

    # Cubierta con pretil: se ve desde la calle como un edificio acabado.
    top = floors * floor_height
    b.box(cx, cy, top, width + 40.0, depth + 40.0, 30.0, "Hormigon", "{}_Cubierta".format(name), folder)
    for side, (start, direction, length) in sides.items():
        nx, ny = SIDE_NORMALS[side]
        px = start[0] + direction[0] * length / 2.0 + nx * 8.0
        py = start[1] + direction[1] * length / 2.0 + ny * 8.0
        sx, sy = (length + 40.0, 24.0) if direction[0] else (24.0, length + 40.0)
        b.box(px, py, top + 30.0, sx, sy, 70.0, "YesoGris", "{}_Pretil{}".format(name, side), folder)

    if roof_props:
        # Máquinas de aire acondicionado, depósito de agua y chimenea.
        b.box(cx - width * 0.25, cy + depth * 0.2, top + 30.0, 120.0, 90.0, 110.0, "MetalNegro",
              "{}_Clima".format(name), folder)
        b.box(cx - width * 0.25 + 140.0, cy + depth * 0.2, top + 30.0, 90.0, 90.0, 80.0, "YesoGris",
              "{}_Clima2".format(name), folder)
        b.cylinder_at(cx + width * 0.25, cy - depth * 0.15, top + 30.0, 140.0, 180.0, "Oxido",
                      "{}_Deposito".format(name), folder)
        b.box(cx + width * 0.1, cy + depth * 0.32, top + 30.0, 60.0, 60.0, 160.0, "Ladrillo",
              "{}_Chimenea".format(name), folder)
        b.cylinder_at(cx - width * 0.4, cy - depth * 0.35, top + 30.0, 6.0, 420.0, "MetalNegro",
                      "{}_Antena".format(name), folder, collision=False)


def furnish(b, name, cx, cy, width, depth, z, style, folder="Interiores"):
    """
    Mobiliario pegado a las esquinas y al centro: cobertura interior sin
    tapar las puertas, que van a mitad de los muros.
    """
    x0, x1 = cx - width / 2.0, cx + width / 2.0
    y0, y1 = cy - depth / 2.0, cy + depth / 2.0
    inset = 90.0
    corners = ((x0 + inset, y0 + inset), (x1 - inset, y0 + inset), (x0 + inset, y1 - inset), (x1 - inset, y1 - inset))

    if style == "vivienda":
        # Sofá, armario, mesa volcada y estantería.
        sx, sy = corners[0]
        b.box(sx + 60.0, sy, z, 200.0, 85.0, 45.0, "Lona", name + "_Sofa", folder)
        b.box(sx + 60.0, sy - 30.0, z + 45.0, 200.0, 25.0, 45.0, "Lona", name + "_SofaRespaldo", folder)
        ax, ay = corners[1]
        b.box(ax, ay, z, 110.0, 60.0, 200.0, "Madera", name + "_Armario", folder)
        tx, ty = corners[2]
        b.box(tx + 40.0, ty, z, 150.0, 8.0, 80.0, "Madera", name + "_MesaVolcada", folder)
        b.box(tx + 40.0, ty - 40.0, z, 8.0, 8.0, 40.0, "Madera", name + "_PataMesa", folder, yaw=30.0)
        ex, ey = corners[3]
        b.box(ex, ey, z, 40.0, 140.0, 190.0, "Madera", name + "_Estanteria", folder)
        b.box(cx, cy, z, 120.0, 80.0, 75.0, "Madera", name + "_Mesa", folder, yaw=random.uniform(-25.0, 25.0))
        b.box(cx + 90.0, cy + 40.0, z, 45.0, 45.0, 45.0, "Madera", name + "_Silla", folder, yaw=40.0)
        b.box(cx - 80.0, cy - 60.0, z, 190.0, 90.0, 20.0, "Lona", name + "_Colchon", folder, yaw=15.0)
    elif style == "tienda":
        # Mostrador y estanterías de tienda saqueada.
        b.box(cx, cy - depth * 0.2, z, width * 0.4, 70.0, 100.0, "Madera", name + "_Mostrador", folder)
        for index, (px, py) in enumerate(corners):
            b.box(px, py, z, 60.0, 180.0, 180.0, "MetalNegro", "{}_Estante{}".format(name, index), folder,
                  yaw=0.0 if index % 2 else 90.0)
        for index in range(5):
            b.box(cx + random.uniform(-width * 0.3, width * 0.3), cy + random.uniform(-depth * 0.3, depth * 0.3), z,
                  45.0, 35.0, 30.0, "Arena", "{}_Caja{}".format(name, index), folder, yaw=random.uniform(0, 90))
    else:
        # Oficina: mesas, archivadores y una pizarra.
        for index, (px, py) in enumerate(corners):
            b.box(px, py, z, 150.0, 75.0, 75.0, "Madera", "{}_Escritorio{}".format(name, index), folder,
                  yaw=random.uniform(-10.0, 10.0))
            b.box(px + (40.0 if index % 2 else -40.0), py, z, 45.0, 60.0, 130.0, "MetalNegro",
                  "{}_Archivador{}".format(name, index), folder)
        b.box(cx, cy, z, 160.0, 160.0, 75.0, "Madera", name + "_MesaReuniones", folder)


def rubble_pile(b, name, x, y, radius, count, materials=("Escombro", "Ladrillo", "Hormigon"), folder="Escombros",
                height=1.0):
    """
    Montón de cascotes: bloques de varios tamaños girados al azar, más altos
    en el centro. Los pequeños se pisan; el núcleo sirve de cobertura baja.
    """
    for index in range(count):
        distance = radius * math.sqrt(random.random())
        angle = random.uniform(0.0, math.tau)
        px = x + math.cos(angle) * distance
        py = y + math.sin(angle) * distance
        size = random.uniform(18.0, 70.0) * (1.4 - distance / max(radius, 1.0))
        b.box(px, py, -size * 0.25, size * random.uniform(0.8, 1.8), size * random.uniform(0.6, 1.2),
              size * random.uniform(0.4, 0.9) * height, random.choice(materials),
              "{}_{}".format(name, index), folder, yaw=random.uniform(0.0, 180.0),
              pitch=random.uniform(-25.0, 25.0), roll=random.uniform(-25.0, 25.0))


def ruined_building(b, name, cx, cy, width, depth, material, gaps=None, folder="Ruinas"):
    """
    Edificio derrumbado: muros de altura desigual, sin cubierta, con una losa
    caída en diagonal y escombros dentro. gaps: {lado: [desplazamientos]}
    donde el muro está abierto (puertas o boquetes).
    """
    gaps = gaps or {}
    x0, x1 = cx - width / 2.0, cx + width / 2.0
    y0, y1 = cy - depth / 2.0, cy + depth / 2.0
    sides = {
        "S": ((x0, y0), (1, 0), width),
        "N": ((x0, y1), (1, 0), width),
        "W": ((x0, y0), (0, 1), depth),
        "E": ((x1, y0), (0, 1), depth),
    }
    for side, (start, direction, length) in sides.items():
        cursor = 0.0
        index = 0
        holes = [(length / 2.0 + offset - 80.0, length / 2.0 + offset + 80.0) for offset in gaps.get(side, [])]
        while cursor < length:
            chunk = min(random.uniform(90.0, 180.0), length - cursor)
            mid = cursor + chunk / 2.0
            if not any(a <= mid <= c for a, c in holes):
                # Más alto en las esquinas, que aguantan mejor.
                corner = min(cursor, length - cursor - chunk) < 200.0
                height = random.uniform(300.0, 520.0) if corner else random.uniform(90.0, 380.0)
                px = start[0] + direction[0] * mid
                py = start[1] + direction[1] * mid
                sx, sy = (chunk, WALL) if direction[0] else (WALL, chunk)
                b.box(px, py, 0.0, sx, sy, height, material, "{}_{}{}".format(name, side, index), folder)
            cursor += chunk
            index += 1

    # Forjado desplomado: una losa apoyada en diagonal hace de rampa y de cobertura.
    b.mesh(b.cube, (cx - width * 0.15, cy, 150.0), (width * 0.5, depth * 0.45, 22.0), "Hormigon",
           name + "_LosaCaida", folder, yaw=12.0, pitch=-24.0)
    rubble_pile(b, name + "_Cascotes", cx + width * 0.2, cy - depth * 0.15, min(width, depth) * 0.3, 26, folder=folder)
    rubble_pile(b, name + "_Cascotes2", cx - width * 0.3, cy + depth * 0.25, min(width, depth) * 0.2, 14, folder=folder)
    # Vigas retorcidas que asoman.
    for index in range(3):
        sx = cx + random.uniform(-width * 0.3, width * 0.3)
        sy = cy + random.uniform(-depth * 0.3, depth * 0.3)
        b.tube((sx, sy, 20.0), (sx + random.uniform(-150, 150), sy + random.uniform(-150, 150), random.uniform(180, 320)),
               14.0, "Oxido", "{}_Viga{}".format(name, index), folder)


def solid_block(b, name, cx, cy, width, depth, height, material, folder="Fondo", face=None):
    """
    Edificio de fondo, macizo: no se entra, solo cierra el horizonte. Con
    face (normal hacia el mapa), lleva ventanas oscuras en esa fachada.
    """
    b.box(cx, cy, 0.0, width, depth, height, material, name, folder)
    b.box(cx, cy, height, width + 30.0, depth + 30.0, 40.0, "Hormigon", name + "_Cornisa", folder)
    if not face:
        return

    nx, ny = face
    span = width if nx == 0 else depth
    facade_x = cx + nx * (width / 2.0 + 3.0)
    facade_y = cy + ny * (depth / 2.0 + 3.0)
    columns = max(int(span // 420.0), 1)
    rows = max(int((height - 350.0) // 380.0), 0)
    for column in range(columns):
        offset = -span / 2.0 + (column + 0.5) * span / columns
        for row in range(rows):
            z = 260.0 + row * 380.0
            wx = facade_x + (offset if nx == 0 else 0.0)
            wy = facade_y + (offset if ny == 0 else 0.0)
            sx, sy = (120.0, 6.0) if nx == 0 else (6.0, 120.0)
            # Algunas ventanas tapiadas con ladrillo, otras negras y vacías.
            boarded = random.random() < 0.18
            b.box(wx, wy, z, sx, sy, 160.0, "Ladrillo" if boarded else "Cristal",
                  "{}_Ventana{}_{}".format(name, column, row), folder, collision=False)


# --- Mobiliario urbano -----------------------------------------------------

def car(b, name, x, y, yaw, burnt=False, folder="Calle"):
    """Turismo abandonado: carrocería, capó y maletero inclinados, cabina con lunas, ruedas."""
    paint = "Quemado" if burnt else random.choice(("Coche", "MetalAzul", "MetalRojo", "Coche"))
    glass = "Quemado" if burnt else "Cristal"
    tilt = random.uniform(-3.0, 3.0) if burnt else 0.0

    def part(lx, ly, z, sx, sy, sz, material, suffix, pitch=0.0):
        px, py = rotate(lx, ly, yaw)
        b.box(x + px, y + py, z, sx, sy, sz, material, name + suffix, folder, yaw=yaw, pitch=pitch, roll=tilt)

    part(0.0, 0.0, 28.0, 440.0, 178.0, 52.0, paint, "_Carroceria")
    part(150.0, 0.0, 70.0, 140.0, 172.0, 18.0, paint, "_Capo", pitch=-6.0)
    part(-170.0, 0.0, 70.0, 100.0, 172.0, 16.0, paint, "_Maletero", pitch=4.0)
    part(-25.0, 0.0, 80.0, 230.0, 166.0, 52.0, glass, "_Cabina")
    part(-25.0, 0.0, 130.0, 200.0, 160.0, 8.0, paint, "_Techo")
    part(225.0, 0.0, 24.0, 12.0, 182.0, 22.0, "MetalNegro", "_Paragolpes")
    part(-225.0, 0.0, 24.0, 12.0, 182.0, 22.0, "MetalNegro", "_ParagolpesTrasero")
    for side in (-1, 1):
        for axle in (-1, 1):
            if burnt and axle == 1 and side == 1:
                continue  # rueda que falta: el coche descansa sobre la llanta
            px, py = rotate(140.0 * axle, 82.0 * side, yaw)
            b.mesh(b.cylinder, (x + px, y + py, 33.0), (66.0, 66.0, 24.0), "Goma",
                   "{}_Rueda{}{}".format(name, side, axle), folder, yaw=yaw, roll=90.0)


def bus(b, name, x, y, yaw, folder="Calle"):
    """Autobús calcinado cruzado en la calzada: la cobertura grande del carril central."""
    def part(lx, ly, z, sx, sy, sz, material, suffix, pitch=0.0, roll=0.0):
        px, py = rotate(lx, ly, yaw)
        b.box(x + px, y + py, z, sx, sy, sz, material, name + suffix, folder, yaw=yaw, pitch=pitch, roll=roll)

    part(0.0, 0.0, 40.0, 1150.0, 250.0, 110.0, "Quemado", "_Chasis")
    # Franja de ventanas: pilares entre huecos para que se pueda disparar a través.
    for index in range(9):
        part(-500.0 + index * 125.0, 0.0, 150.0, 22.0, 250.0, 120.0, "Quemado", "_Pilar{}".format(index))
    part(0.0, 0.0, 270.0, 1150.0, 250.0, 14.0, "Quemado", "_Techo", roll=2.0)
    part(560.0, 0.0, 40.0, 30.0, 250.0, 230.0, "Quemado", "_Frente")
    part(-560.0, 0.0, 40.0, 30.0, 250.0, 230.0, "Quemado", "_Trasera")
    for index, lx in enumerate((-380.0, 380.0)):
        for side in (-1, 1):
            px, py = rotate(lx, 112.0 * side, yaw)
            b.mesh(b.cylinder, (x + px, y + py, 45.0), (90.0, 90.0, 30.0), "Goma",
                   "{}_Rueda{}{}".format(name, index, side), folder, yaw=yaw, roll=90.0)


def street_lamp(b, name, x, y, facing, broken=False, folder="Calle"):
    """Farola: báculo, brazo hacia la calzada y luminaria."""
    lean = random.uniform(8.0, 18.0) if broken else 0.0
    b.mesh(b.cylinder, (x, y, 340.0), (18.0, 18.0, 650.0), "MetalNegro", name + "_Baculo", folder,
           pitch=lean, collision=True)
    if broken:
        return
    b.box(x, y + facing * 70.0, 640.0, 10.0, 150.0, 10.0, "MetalNegro", name + "_Brazo", folder, collision=False)
    b.box(x, y + facing * 140.0, 625.0, 30.0, 55.0, 14.0, "MetalNegro", name + "_Luminaria", folder, collision=False)


def sandbag_wall(b, name, x, y, yaw, length=300.0, rows=5, folder="Coberturas"):
    """
    Muro de sacos terreros apilados a matajunta: cada hilada desplazada medio
    saco, como se construyen de verdad.
    """
    bag_length, bag_depth, bag_height = 58.0, 34.0, 15.0
    count = max(int(length // bag_length), 1)
    for row in range(rows):
        shift = bag_length / 2.0 if row % 2 else 0.0
        for column in range(count - (1 if row % 2 else 0)):
            lx = -length / 2.0 + bag_length / 2.0 + column * bag_length + shift
            for layer in (-1, 1):
                ly = layer * bag_depth * 0.5
                px, py = rotate(lx, ly, yaw)
                b.box(x + px, y + py, row * bag_height * 0.92, bag_length * random.uniform(0.94, 1.02),
                      bag_depth, bag_height, "Arena", "{}_{}_{}_{}".format(name, row, column, layer), folder,
                      yaw=yaw + random.uniform(-4.0, 4.0), roll=random.uniform(-3.0, 3.0))


def jersey(b, name, x, y, yaw, folder="Coberturas"):
    """Barrera de hormigón con su perfil escalonado."""
    b.box(x, y, 0.0, 300.0, 62.0, 25.0, "Hormigon", name + "_Base", folder, yaw=yaw)
    b.box(x, y, 25.0, 300.0, 38.0, 30.0, "Hormigon", name + "_Medio", folder, yaw=yaw)
    b.box(x, y, 55.0, 300.0, 20.0, 30.0, "Hormigon", name + "_Remate", folder, yaw=yaw)


def container(b, name, x, y, yaw, color, stacked=False, open_doors=False, folder="Contenedores"):
    b.box(x, y, 0.0, 606.0, 244.0, 259.0, color, name, folder, yaw=yaw)
    # Nervios verticales de la chapa: rompen el lateral liso.
    for index in range(6):
        px, py = rotate(-250.0 + index * 100.0, 124.0, yaw)
        b.box(x + px, y + py, 10.0, 8.0, 6.0, 239.0, color, "{}_Nervio{}".format(name, index), folder, yaw=yaw,
              collision=False)
    if open_doors:
        px, py = rotate(318.0, -60.0, yaw)
        b.box(x + px, y + py, 0.0, 6.0, 122.0, 255.0, color, name + "_Puerta", folder, yaw=yaw + 70.0)
    if stacked:
        b.box(x, y, 259.0, 606.0, 244.0, 259.0, random.choice(("MetalRojo", "MetalAzul", "MetalVerde", "Oxido")),
              name + "_Alto", folder, yaw=yaw + random.uniform(-3.0, 3.0))


def crate(b, name, x, y, size=110.0, stack=1, folder="Coberturas"):
    for level in range(stack):
        b.box(x, y, level * size, size, size, size, "Madera", "{}_{}".format(name, level), folder,
              yaw=random.uniform(-8.0, 8.0))


def barrel(b, name, x, y, material="Oxido", tipped=False, folder="Props"):
    if tipped:
        b.mesh(b.cylinder, (x, y, 29.0), (58.0, 58.0, 88.0), material, name, folder,
               yaw=random.uniform(0.0, 180.0), roll=90.0)
    else:
        b.cylinder_at(x, y, 0.0, 58.0, 88.0, material, name, folder)


def pallet_stack(b, name, x, y, levels, folder="Props"):
    for level in range(levels):
        b.box(x, y, level * 15.0, 120.0, 100.0, 14.0, "Madera", "{}_{}".format(name, level), folder,
              yaw=random.uniform(-6.0, 6.0))


def tire_stack(b, name, x, y, count, folder="Props"):
    for level in range(count):
        b.mesh(b.cylinder, (x + random.uniform(-4, 4), y + random.uniform(-4, 4), 11.0 + level * 22.0),
               (70.0, 70.0, 22.0), "Goma", "{}_{}".format(name, level), folder)


def trash_bags(b, name, x, y, count, folder="Props"):
    for index in range(count):
        size = random.uniform(45.0, 70.0)
        b.blob(x + random.uniform(-80, 80), y + random.uniform(-80, 80), size * 0.3, size, size * 0.85, size * 0.65,
               "Goma", "{}_{}".format(name, index), folder, collision=False)


def dumpster(b, name, x, y, yaw, color, folder="Callejon"):
    b.box(x, y, 12.0, 190.0, 110.0, 113.0, color, name, folder, yaw=yaw)
    px, py = rotate(0.0, -40.0, yaw)
    b.box(x + px, y + py, 125.0, 190.0, 115.0, 6.0, "Goma", name + "_Tapa", folder, yaw=yaw, roll=-35.0, collision=False)
    for side in (-1, 1):
        px, py = rotate(80.0 * side, 0.0, yaw)
        b.cylinder_at(x + px, y + py, 0.0, 14.0, 12.0, "Goma", "{}_Rueda{}".format(name, side), folder, collision=False)


def dead_tree(b, name, x, y, folder="Plaza"):
    height = random.uniform(420.0, 620.0)
    b.cylinder_at(x, y, 0.0, 32.0, height, "Madera", name + "_Tronco", folder)
    for index in range(4):
        angle = random.uniform(0.0, math.tau)
        z = height * random.uniform(0.55, 0.95)
        reach = random.uniform(120.0, 220.0)
        b.tube((x, y, z), (x + math.cos(angle) * reach, y + math.sin(angle) * reach, z + random.uniform(60, 160)),
               random.uniform(8.0, 14.0), "Madera", "{}_Rama{}".format(name, index), folder)
    # Alcorque: el cuadrado de bordillo alrededor del árbol.
    b.box(x, y, 0.0, 160.0, 160.0, 12.0, "Hormigon", name + "_Alcorque", folder)


def fire(b, name, x, y, z, scale, darkness=0.8, smoke=True, intensity=80.0, folder="Fuegos"):
    """Fuego con luz y humo (AShooterFireSource, en C++)."""
    fire_class = getattr(unreal, "ShooterFireSource", None)
    if not fire_class:
        warn("ShooterFireSource no disponible: compila el C++ antes de ejecutar el script")
        return None
    actor = b.actor(fire_class, (x, y, z), label=name, folder=folder)
    set_props(actor, {
        "fire_scale": scale,
        "smoke_darkness": darkness,
        "emit_smoke": smoke,
        "light_intensity": intensity,
    }, name)
    return actor


def crater(b, name, x, y, size, folder="Crateres"):
    """Cráter de impacto en el suelo: la marca de impacto a gran escala."""
    material = load(FX_DIR + "/M_Impacto")
    if not material:
        return
    actor = b.actor(unreal.DecalActor, (x, y, 0.0), (-90.0, random.uniform(0.0, 360.0), 0.0), name, folder)
    if not actor:
        return
    try:
        component = actor.get_editor_property("decal")
        component.set_decal_material(material)
        component.set_editor_property("decal_size", unreal.Vector(60.0, size, size))
    except Exception as error:
        warn("{}: {}".format(name, error))
    # Tierra levantada alrededor: el borde del cráter.
    for index in range(7):
        angle = index / 7.0 * math.tau + random.uniform(-0.2, 0.2)
        b.box(x + math.cos(angle) * size * 0.85, y + math.sin(angle) * size * 0.85, -8.0,
              size * 0.45, size * 0.25, random.uniform(10.0, 22.0), "Tierra",
              "{}_Borde{}".format(name, index), folder, yaw=math.degrees(angle) + 90.0,
              pitch=random.uniform(-8.0, 8.0))


def haze(b, name, x, y, z, scale, density=0.004, folder="Bruma"):
    """Bolsa de bruma o humo local (Local Fog Volume, UE 5.3+)."""
    fog_class = getattr(unreal, "LocalFogVolume", None)
    if not fog_class:
        return None
    actor = b.actor(fog_class, (x, y, z), label=name, folder=folder)
    if not actor:
        return None
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale * 0.45))
    component_class = getattr(unreal, "LocalFogVolumeComponent", None)
    component = actor.get_component_by_class(component_class) if component_class else None
    if component:
        set_props(component, {
            "radial_fog_extinction": density * 0.5,
            "height_fog_extinction": density,
            "height_fog_falloff": 600.0,
            "fog_albedo": unreal.LinearColor(0.55, 0.5, 0.45, 1.0),
        }, name)
    return actor


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
        # Bordillo: la arista de piedra entre acera y calzada.
        b.box(0, side * 805.0, 0.0, HALF_X * 2, 14.0, 17.0, "Hormigon", "Bordillo_{}".format(side), "Suelo")
    b.box(0, -3600.0, -99.0, HALF_X * 2, 4800.0, 100.0, "Tierra", "Patio_Tierra", "Suelo")
    b.box(0, 4100.0, -99.0, HALF_X * 2, 1400.0, 100.0, "Acera", "Callejon_Suelo", "Suelo")

    # Marcas viales: línea central discontinua, bordes y pasos de peatones.
    x = -HALF_X + 300.0
    index = 0
    while x < HALF_X - 300.0:
        if abs(x) > 700.0:  # la plaza interrumpe la línea
            b.box(x, 0.0, 1.0, 320.0, 14.0, 1.0, "MarcaVial", "Linea_Central_{}".format(index), "Marcas",
                  collision=False)
        x += 900.0
        index += 1
    for side in (-1, 1):
        b.box(0.0, side * 760.0, 1.0, HALF_X * 2 - 400.0, 12.0, 1.0, "MarcaVial", "Linea_Borde_{}".format(side),
              "Marcas", collision=False)
    for crossing, cx in enumerate((-2200.0, 2200.0)):
        for stripe in range(12):
            b.box(cx, -660.0 + stripe * 120.0, 1.0, 380.0, 55.0, 1.0, "MarcaVial",
                  "Paso_{}_{}".format(crossing, stripe), "Marcas", collision=False)

    # Tapas de alcantarilla.
    for index, (x, y) in enumerate(((-5600.0, 300.0), (-1800.0, -350.0), (1300.0, 420.0), (5200.0, -280.0))):
        b.cylinder_at(x, y, 0.0, 70.0, 2.0, "MetalNegro", "Alcantarilla_{}".format(index), "Marcas", collision=False)


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
                        width, 800.0, heights[index % len(heights)], finishes[index % len(finishes)],
                        face=(0.0, -side))
            x += width
            index += 1
        y = -HALF_Y
        while y < HALF_Y:
            depth = min(random.choice((2000.0, 2400.0, 3000.0)), HALF_Y - y)
            solid_block(b, "Fondo_EW_{}".format(index), side * (HALF_X + 400.0), y + depth / 2.0,
                        800.0, depth, heights[(index + 3) % len(heights)], finishes[(index + 1) % len(finishes)],
                        face=(-side, 0.0))
            y += depth
            index += 1


def build_north_band(b):
    """Edificios entre la avenida y el callejón, y naves al fondo."""
    # Fachadas a la avenida con ventanas: posiciones de tiro sobre la calle.
    building(b, "Bloque_NO", -5200.0, 2400.0, 2400.0, 1800.0, 1, "Ladrillo",
             doors={"S": [-500.0], "N": [600.0]}, windows={"S": [300.0, 800.0], "N": [-600.0], "E": [0.0]},
             floor_height=420.0)
    furnish(b, "Bloque_NO", -5200.0, 2400.0, 2400.0, 1800.0, 0.0, "tienda")

    building(b, "Bloque_Central", -1200.0, 2400.0, 3000.0, 1800.0, 2, "Yeso",
             doors={"S": [600.0], "N": [-900.0], "E": [-300.0]},
             windows={"S": [-1100.0, -500.0, 0.0, 1100.0], "N": [300.0, 900.0], "W": [-400.0, 400.0]},
             ramp=True, balconies={"S": [-500.0, 1100.0]})
    furnish(b, "Bloque_Central_PB", -800.0, 2300.0, 2000.0, 1300.0, 0.0, "oficina")
    furnish(b, "Bloque_Central_P1", -900.0, 2200.0, 2200.0, 1200.0, FLOOR_HEIGHT, "vivienda")

    building(b, "Bloque_NE", 2600.0, 2400.0, 2200.0, 1800.0, 1, "YesoGris",
             doors={"S": [0.0], "W": [400.0], "N": [-500.0]}, windows={"S": [-700.0, 700.0], "N": [500.0]},
             floor_height=420.0)
    furnish(b, "Bloque_NE", 2600.0, 2400.0, 2200.0, 1800.0, 0.0, "vivienda")

    # Andamio en la fachada del bloque NE: tubos y tablones.
    for index in range(6):
        px = 1700.0 + index * 360.0
        b.cylinder_at(px, 1440.0, 0.0, 6.0, 640.0, "Oxido", "Andamio_Tubo_{}".format(index), "Andamio",
                      collision=False)
        b.cylinder_at(px, 1360.0, 0.0, 6.0, 640.0, "Oxido", "Andamio_TuboExt_{}".format(index), "Andamio",
                      collision=False)
    for level, z in enumerate((210.0, 420.0, 630.0)):
        b.box(2600.0, 1400.0, z, 1850.0, 90.0, 5.0, "Madera", "Andamio_Tablon_{}".format(level), "Andamio")
        b.tube((1700.0, 1360.0, z + 100.0), (3500.0, 1360.0, z + 100.0), 5.0, "Oxido",
               "Andamio_Baranda_{}".format(level), "Andamio")
    b.box(2600.0, 1350.0, 0.0, 1850.0, 4.0, 640.0, "Lona", "Andamio_Lona", "Andamio", collision=False)

    # El bloque del este está en ruinas: un cañonazo lo abrió.
    ruined_building(b, "Bloque_E", 5800.0, 2400.0, 1800.0, 1800.0, "Ladrillo",
                    gaps={"S": [-300.0, 400.0], "N": [300.0], "W": [0.0]})
    fire(b, "Fuego_Ruina", 5900.0, 2500.0, 0.0, 1.6, darkness=0.7)

    # Naves macizas al norte del callejón.
    x = -7400.0
    index = 0
    while x < 7400.0:
        width = random.choice((1800.0, 2200.0, 2600.0))
        solid_block(b, "Nave_Norte_{}".format(index), x + width / 2.0, 5400.0, width, 1200.0,
                    random.choice((600.0, 750.0, 900.0)), random.choice(("Ladrillo", "YesoGris", "Oxido")),
                    folder="Callejon", face=(0.0, -1.0))
        x += width + 200.0
        index += 1


def build_alley(b):
    """Callejón norte: estrecho, con obstáculos que obligan a moverse de uno en uno."""
    for index, x in enumerate((-6200.0, -3600.0, -600.0, 1600.0, 4400.0, 6400.0)):
        dumpster(b, "Contenedor_Basura_{}".format(index), x, 4700.0, random.uniform(-15.0, 15.0),
                 random.choice(("MetalVerde", "MetalAzul")))
        trash_bags(b, "Bolsas_{}".format(index), x + 180.0, 4720.0, random.randint(3, 6))
    for index, (x, y, stack) in enumerate(((-4800.0, 3700.0, 2), (-2400.0, 4500.0, 1), (900.0, 3650.0, 2),
                                           (3300.0, 4550.0, 1), (5200.0, 3750.0, 3))):
        crate(b, "Cajas_{}".format(index), x, y, stack=stack)
    # Vallas de madera y chapa: tapan la vista pero las balas pasan.
    b.box(-3000.0, 4100.0, 0.0, 4.0, 600.0, 190.0, "Madera", "Valla_Madera_0", "Coberturas")
    b.box(3900.0, 4100.0, 0.0, 4.0, 600.0, 190.0, "Madera", "Valla_Madera_1", "Coberturas")
    b.box(400.0, 4300.0, 0.0, 3.0, 500.0, 220.0, "Oxido", "Valla_Chapa_0", "Coberturas")

    # Tendido eléctrico: postes de madera y cables combados entre ellos.
    previous = None
    for index, x in enumerate(range(-7200, 7201, 1800)):
        b.cylinder_at(float(x), 4850.0, 0.0, 26.0, 900.0, "Madera", "Poste_{}".format(index), "Tendido")
        b.box(float(x), 4850.0, 840.0, 12.0, 160.0, 12.0, "Madera", "Poste_Cruceta_{}".format(index), "Tendido",
              collision=False)
        if previous is not None:
            for wire, offset in enumerate((-60.0, 60.0)):
                mid = ((previous + x) / 2.0, 4850.0 + offset, 790.0)
                b.tube((previous, 4850.0 + offset, 850.0), mid, 2.0, "MetalNegro",
                       "Cable_{}_{}a".format(index, wire), "Tendido")
                b.tube(mid, (float(x), 4850.0 + offset, 850.0), 2.0, "MetalNegro",
                       "Cable_{}_{}b".format(index, wire), "Tendido")
        previous = float(x)

    for index, (x, y) in enumerate(((-5600.0, 3600.0), (2200.0, 4650.0))):
        barrel(b, "Bidon_Fuego_{}".format(index), x, y)
        fire(b, "Fuego_Bidon_{}".format(index), x, y, 88.0, 0.5, darkness=0.85, intensity=60.0)
    for index, (x, y) in enumerate(((-1500.0, 3700.0), (4800.0, 4600.0), (-6800.0, 4500.0))):
        tire_stack(b, "Neumaticos_{}".format(index), x, y, random.randint(2, 4))
    rubble_pile(b, "Escombros_Callejon_0", -200.0, 3600.0, 160.0, 10)
    rubble_pile(b, "Escombros_Callejon_1", 6000.0, 3650.0, 180.0, 12)
    haze(b, "Bruma_Callejon", 0.0, 4200.0, 150.0, 8.0, density=0.0025)


def build_avenue(b):
    """Avenida central: coches, autobús, barreras, farolas y la plaza con el monumento."""
    for index, (x, y, yaw, burnt) in enumerate(((-6000.0, -350.0, 12.0, False), (-4300.0, 420.0, -20.0, True),
                                                (-2600.0, -150.0, 35.0, False), (2300.0, 300.0, -30.0, True),
                                                (3900.0, -420.0, 8.0, False), (5800.0, 250.0, -15.0, False))):
        car(b, "Coche_{}".format(index), x, y, yaw, burnt=burnt)
    fire(b, "Fuego_Coche_1", -4300.0, 420.0, 60.0, 2.2, darkness=0.95, intensity=110.0)
    fire(b, "Fuego_Coche_3", 2300.0, 300.0, 60.0, 2.0, darkness=0.95, intensity=100.0)

    # Autobús calcinado y camión, en espejo: la gran cobertura de cada mitad.
    bus(b, "Autobus", 4600.0, -60.0, 24.0)
    fire(b, "Fuego_Autobus", 4300.0, -150.0, 150.0, 2.8, darkness=0.9, intensity=130.0)
    bus(b, "Camion", -4600.0, 60.0, -156.0)

    for index, (x, y, yaw) in enumerate(((-3400.0, 600.0, 0.0), (-1700.0, -550.0, 20.0),
                                         (1700.0, 550.0, -20.0), (3300.0, -600.0, 0.0))):
        jersey(b, "Barrera_{}".format(index), x, y, yaw)

    # Plaza (zona B): monumento con escalones como cobertura de 360°.
    b.box(0, 0, 0.0, 700.0, 700.0, 45.0, "Hormigon", "Monumento_Base", "Plaza")
    b.box(0, 0, 45.0, 420.0, 420.0, 90.0, "Hormigon", "Monumento_Escalon", "Plaza")
    b.box(0, 0, 135.0, 220.0, 220.0, 260.0, "YesoGris", "Monumento_Pedestal", "Plaza")
    b.cylinder_at(0, 0, 395.0, 90.0, 380.0, "Hormigon", "Monumento_Columna", "Plaza")
    # La columna partida: el trozo superior yace al pie del monumento.
    b.mesh(b.cylinder, (280.0, -420.0, 45.0), (90.0, 90.0, 260.0), "Hormigon", "Monumento_Fuste_Caido", "Plaza",
           yaw=35.0, roll=90.0)
    rubble_pile(b, "Escombros_Plaza", 320.0, -300.0, 220.0, 14, materials=("Hormigon", "YesoGris"))
    for index, (x, y) in enumerate(((-650.0, 650.0), (650.0, 650.0), (-650.0, -650.0), (650.0, -650.0))):
        dead_tree(b, "Arbol_{}".format(index), x, y)
    for index, (x, y, yaw) in enumerate(((-500.0, 0.0, 90.0), (500.0, 0.0, 90.0))):
        b.box(x, y, 0.0, 180.0, 50.0, 45.0, "Madera", "Banco_{}".format(index), "Plaza", yaw=yaw)
        bx, by = rotate(0.0, 22.0, yaw)
        b.box(x + bx, y + by, 45.0, 180.0, 8.0, 45.0, "Madera", "Banco_Respaldo_{}".format(index), "Plaza", yaw=yaw)

    # Nidos de sacos terreros a la salida de cada base.
    for side in (-1, 1):
        sandbag_wall(b, "Sacos_Base_{}_a".format(side), side * 5000.0, 700.0, 90.0, 260.0)
        sandbag_wall(b, "Sacos_Base_{}_b".format(side), side * 5000.0, -700.0, 90.0, 260.0)

    # Farolas a ambos lados; alguna doblada por la metralla.
    for index, x in enumerate(range(-7000, 7001, 1750)):
        for side in (-1, 1):
            street_lamp(b, "Farola_{}_{}".format(index, side), float(x), side * 1150.0, -side,
                        broken=random.random() < 0.2)

    for index, (x, y, size) in enumerate(((-3000.0, -300.0, 220.0), (-900.0, 500.0, 160.0), (1200.0, -450.0, 200.0),
                                          (3100.0, 200.0, 180.0), (-6500.0, 100.0, 150.0), (6400.0, -400.0, 170.0))):
        crater(b, "Crater_Avenida_{}".format(index), x, y, size)

    for index, (x, y) in enumerate(((-2000.0, 1050.0), (2600.0, -1050.0), (-6200.0, -1050.0), (6100.0, 1050.0))):
        rubble_pile(b, "Escombros_Acera_{}".format(index), x, y, 150.0, 9)
    haze(b, "Bruma_Avenida_Oeste", -4300.0, 300.0, 300.0, 10.0, density=0.003)
    haze(b, "Bruma_Avenida_Este", 4400.0, -100.0, 300.0, 10.0, density=0.003)


def build_industrial(b):
    """Patio sur: nave grande (zona C) con pasarela, contenedores y un muro con huecos."""
    building(b, "Nave_Industrial", 0.0, -3800.0, 4200.0, 2600.0, 1, "Oxido",
             doors={"N": [-1200.0, 0.0, 1200.0], "S": [-800.0, 800.0], "W": [0.0], "E": [0.0]},
             windows={"N": [-600.0, 600.0], "S": [0.0]}, floor_height=700.0, folder="Industrial",
             roof_props=False)
    # Interior de la nave: estanterías y palés para el combate cercano.
    for index, (x, y) in enumerate(((-1300.0, -3400.0), (-1300.0, -4300.0), (1300.0, -3400.0), (1300.0, -4300.0))):
        b.box(x, y, 0.0, 900.0, 120.0, 260.0, "Oxido", "Estanteria_{}".format(index), "Industrial")
        for shelf in range(3):
            b.box(x + random.uniform(-300, 300), y, 80.0 + shelf * 85.0, 120.0, 100.0, 50.0, "Madera",
                  "Estanteria_{}_Carga{}".format(index, shelf), "Industrial")
    for index, (x, y) in enumerate(((-400.0, -3300.0), (500.0, -4500.0), (0.0, -3900.0))):
        crate(b, "Pales_{}".format(index), x, y, size=120.0, stack=1 + index % 2)

    # Pasarela elevada junto al muro norte, con rampa de acceso: la zona C
    # se disputa también desde arriba.
    b.box(0.0, -2680.0, 300.0, 3400.0, 220.0, 20.0, "MetalNegro", "Pasarela", "Industrial")
    b.box(0.0, -2790.0, 320.0, 3400.0, 6.0, 100.0, "MetalNegro", "Pasarela_Barandilla", "Industrial")
    run, rise = 950.0, 300.0
    b.mesh(b.cube, (-1700.0 - run / 2.0 + 120.0, -2680.0, rise / 2.0), (math.hypot(run, rise), 200.0, 20.0),
           "MetalNegro", "Pasarela_Rampa", "Industrial", pitch=math.degrees(math.atan2(rise, run)))
    for index, x in enumerate((-1500.0, -500.0, 500.0, 1500.0)):
        b.cylinder_at(x, -2680.0, 0.0, 18.0, 300.0, "MetalNegro", "Pasarela_Pilar_{}".format(index), "Industrial")
    # Viga del puente grúa y tuberías por los muros.
    b.box(0.0, -3800.0, 590.0, 4100.0, 40.0, 50.0, "MetalRojo", "Puente_Grua", "Industrial", collision=False)
    for index, z in enumerate((450.0, 520.0)):
        b.tube((-2050.0, -4950.0, z), (2050.0, -4950.0, z), 22.0, "Oxido", "Tuberia_{}".format(index), "Industrial")

    # Muro del patio con huecos: separa la avenida del patio.
    for index, (x, length) in enumerate(((-6800.0, 2000.0), (-3700.0, 2400.0), (-500.0, 1200.0),
                                         (2700.0, 2400.0), (6000.0, 2400.0))):
        b.box(x, -1350.0, 0.0, length, 30.0, 240.0, "Hormigon", "Muro_Patio_{}".format(index), "Industrial")
        b.box(x, -1350.0, 240.0, length, 40.0, 10.0, "Hormigon", "Muro_Patio_Albardilla_{}".format(index), "Industrial")

    # Contenedores a ambos lados de la nave, algunos apilados o abiertos.
    colors = ("MetalRojo", "MetalAzul", "MetalVerde", "Oxido")
    layout = (
        (-6600.0, -2400.0, 0.0, True), (-6600.0, -2700.0, 0.0, False), (-5200.0, -4200.0, 90.0, True),
        (-4800.0, -5300.0, 0.0, False), (-3600.0, -2500.0, 15.0, False), (-3400.0, -4700.0, 90.0, True),
        (3500.0, -2400.0, -10.0, False), (3600.0, -4800.0, 90.0, True), (4900.0, -3100.0, 0.0, True),
        (5200.0, -5200.0, 0.0, False), (6600.0, -2500.0, 0.0, False), (6600.0, -4200.0, 90.0, True),
    )
    for index, (x, y, yaw, stacked) in enumerate(layout):
        container(b, "Contenedor_{}".format(index), x, y, yaw, colors[index % len(colors)], stacked,
                  open_doors=index in (4, 9))

    for index, (x, y, yaw) in enumerate(((-2300.0, -2200.0, 0.0), (2300.0, -2200.0, 0.0),
                                         (-2400.0, -5400.0, 90.0), (2400.0, -5400.0, 90.0))):
        sandbag_wall(b, "Sacos_Patio_{}".format(index), x, y, yaw)

    # Bastiones de tierra (gaviones) que cierran el patio por los lados.
    for side in (-1, 1):
        for index in range(4):
            b.box(side * 7300.0, -2600.0 - index * 120.0, 0.0, 110.0, 110.0, 135.0, "Arena",
                  "Gavion_{}_{}".format(side, index), "Industrial")

    for index, (x, y) in enumerate(((-5800.0, -3300.0), (5600.0, -4000.0), (-2600.0, -3200.0), (2700.0, -4600.0))):
        barrel(b, "Bidon_{}a".format(index), x, y, random.choice(("Oxido", "MetalAzul", "MetalRojo")))
        barrel(b, "Bidon_{}b".format(index), x + 65.0, y + 20.0, "Oxido", tipped=index % 2 == 0)
        pallet_stack(b, "Pales_Patio_{}".format(index), x - 150.0, y + 120.0, random.randint(2, 6))
    fire(b, "Fuego_Patio_0", -5800.0, -3300.0, 88.0, 0.6, darkness=0.8, intensity=60.0)
    fire(b, "Fuego_Patio_1", 5600.0, -4000.0, 88.0, 0.6, darkness=0.8, intensity=60.0)

    for index, (x, y, size) in enumerate(((-4300.0, -3600.0, 260.0), (4200.0, -3000.0, 240.0), (-1800.0, -5600.0, 200.0))):
        crater(b, "Crater_Patio_{}".format(index), x, y, size)
    rubble_pile(b, "Escombros_Patio_0", -4500.0, -1700.0, 260.0, 16)
    rubble_pile(b, "Escombros_Patio_1", 4600.0, -1750.0, 240.0, 14)
    haze(b, "Bruma_Patio", 0.0, -3800.0, 200.0, 14.0, density=0.002)


def build_gameplay(b):
    # Apariciones: seis por equipo, en los tres carriles de su base. La
    # etiqueta de equipo la lee ShooterGameMode::ChoosePlayerStart.
    spawns = 0
    for team, x, yaw in (("Alfa", -7300.0, 0.0), ("Bravo", 7300.0, 180.0)):
        for index, y in enumerate((-4600.0, -2600.0, -400.0, 400.0, 3000.0, 4200.0)):
            start = b.actor(unreal.PlayerStart, (x, y, 110.0), (0.0, yaw, 0.0),
                            "Aparicion_{}_{}".format(team, index), "Apariciones")
            if start:
                set_props(start, {"player_start_tag": unreal.Name(team)}, "Aparicion")
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


def build_atmosphere(b, sun_pitch=-13.0, sun_yaw=152.0):
    """
    Atardecer dorado con bruma: sol muy bajo y cálido, sombras largas, aire
    con volumen y rayos de luz a través del humo y entre los edificios.
    """
    sun = b.actor(unreal.DirectionalLight, (0.0, 0.0, 3000.0), (sun_pitch, sun_yaw, 0.0), "Sol", "Iluminacion")
    if sun:
        light = sun.get_component_by_class(unreal.DirectionalLightComponent)
        set_props(light, {
            "mobility": unreal.ComponentMobility.MOVABLE,
            "intensity": 7.0,
            "use_temperature": True,
            "temperature": 3800.0,
            "atmosphere_sun_light": True,
            "light_source_angle": 1.2,
            "cast_cloud_shadows": True,
            # Los rayos de luz: dispersión en la niebla volumétrica y el
            # efecto de pantalla clásico cuando se mira hacia el sol.
            "volumetric_scattering_intensity": 2.5,
            "enable_light_shaft_bloom": True,
            "bloom_scale": 0.3,
            "bloom_threshold": 0.5,
            "enable_light_shaft_occlusion": True,
            "occlusion_mask_darkness": 0.35,
        }, "Sol")

    sky = b.actor(unreal.SkyLight, (0.0, 0.0, 1000.0), label="Luz_Cielo", folder="Iluminacion")
    if sky:
        component = sky.get_component_by_class(unreal.SkyLightComponent)
        set_props(component, {"mobility": unreal.ComponentMobility.MOVABLE, "real_time_capture": True,
                              "intensity": 0.8}, "Luz_Cielo")

    atmosphere = b.actor(unreal.SkyAtmosphere, (0.0, 0.0, 0.0), label="Atmosfera", folder="Iluminacion")
    if atmosphere:
        component = atmosphere.get_component_by_class(unreal.SkyAtmosphereComponent)
        # Más dispersión de Mie: el polvo en suspensión de una ciudad en guerra.
        set_props(component, {
            "mie_scattering_scale": 0.012,
            "mie_anisotropy": 0.82,
            "aerial_pespective_view_distance_scale": 2.5,
        }, "Atmosfera")

    cloud_class = getattr(unreal, "VolumetricCloud", None)
    if cloud_class:
        b.actor(cloud_class, (0.0, 0.0, 0.0), label="Nubes", folder="Iluminacion")

    fog = b.actor(unreal.ExponentialHeightFog, (0.0, 0.0, -100.0), label="Niebla", folder="Iluminacion")
    if fog:
        component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
        # En 5.8 la propiedad es bEnableVolumetricFog, no bVolumetricFog.
        set_props(component, {
            "fog_density": 0.035,
            "fog_height_falloff": 0.16,
            "start_distance": 400.0,
            "enable_volumetric_fog": True,
            "volumetric_fog_scattering_distribution": 0.8,
            "volumetric_fog_albedo": unreal.Color(235, 215, 190, 255),
            "volumetric_fog_extinction_scale": 1.6,
            "volumetric_fog_distance": 9000.0,
            "fog_inscattering_luminance": unreal.LinearColor(0.42, 0.3, 0.2, 1.0),
            "directional_inscattering_luminance": unreal.LinearColor(1.0, 0.62, 0.3, 1.0),
            "directional_inscattering_exponent": 6.0,
        }, "Niebla")

    volume = b.actor(unreal.PostProcessVolume, (0.0, 0.0, 500.0), label="PostProceso", folder="Iluminacion")
    if volume:
        volume.set_editor_property("unbound", True)
        settings = volume.get_editor_property("settings")
        for name, value in (
            # Sin compensación, la exposición automática quema las fachadas
            # claras a pleno sol y el color se lava.
            ("auto_exposure_bias", -0.7),
            ("bloom_intensity", 0.6),
            ("vignette_intensity", 0.42),
            ("film_grain_intensity", 0.2),
            ("scene_fringe_intensity", 0.3),
            ("motion_blur_amount", 0.0),
            ("ambient_occlusion_intensity", 0.75),
            # Gradación sucia: algo desaturada, contraste alto, luces
            # cálidas y sombras frías, como en las referencias.
            ("white_temp", 7000.0),
            ("color_saturation", unreal.Vector4(0.82, 0.82, 0.82, 0.88)),
            ("color_contrast", unreal.Vector4(1.08, 1.08, 1.08, 1.1)),
            ("color_gain_highlights", unreal.Vector4(1.06, 0.98, 0.86, 1.0)),
            ("color_gain_shadows", unreal.Vector4(0.9, 0.97, 1.03, 1.0)),
            ("lens_flare_intensity", 0.25),
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


def open_level(path):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        LEVEL_SUB.load_level(path)
        clear_generated()
        log("nivel existente abierto: {}".format(path))
    else:
        LEVEL_SUB.new_level(path)
        log("nivel creado: {}".format(path))


def set_level_game_mode(game_mode_class, label):
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        settings = world.get_world_settings()
        if settings and game_mode_class:
            settings.set_editor_property("default_game_mode", game_mode_class)
            log("GameMode del nivel -> {}".format(label))
    except Exception as error:
        warn("asigna {} a mano en World Settings: {}".format(label, error))


# ==========================================================================
# 8. Decorado del menú
# ==========================================================================

def build_menu_level(materials, physical):
    """
    Lvl_Menu: un rincón del Distrito al atardecer, con una cámara fija. El
    menú (en Slate, desde C++) ocupa la izquierda y la escena se ve a la
    derecha.
    """
    open_level(MENU_LEVEL_PATH)

    # build_menu.py dejó luces sin etiqueta: con las nuevas habría dos soles.
    stale = [actor for actor in ACTOR_SUB.get_all_level_actors()
             if isinstance(actor, (unreal.DirectionalLight, unreal.SkyLight, unreal.SkyAtmosphere))]
    if stale:
        ACTOR_SUB.destroy_actors(stale)

    b = Builder(materials, physical, folder_root="Menu")
    b.box(0, 0, -100.0, 8000.0, 8000.0, 100.0, "Asfalto", "Suelo", "Suelo")
    b.box(0, 900.0, 0.0, 6000.0, 400.0, 15.0, "Acera", "Acera", "Suelo")
    ruined_building(b, "Ruina_Menu", 600.0, 1900.0, 1600.0, 1400.0, "Ladrillo", gaps={"S": [0.0]})
    solid_block(b, "Fondo_Menu_0", -1800.0, 2600.0, 2000.0, 800.0, 1500.0, "Yeso", face=(0.0, -1.0))
    solid_block(b, "Fondo_Menu_1", 2800.0, 2700.0, 2200.0, 800.0, 1200.0, "YesoGris", face=(0.0, -1.0))
    car(b, "Coche_Menu", 500.0, 150.0, 25.0, burnt=True)
    fire(b, "Fuego_Menu", 500.0, 150.0, 60.0, 2.2, darkness=0.95, intensity=120.0)
    sandbag_wall(b, "Sacos_Menu", -300.0, -350.0, 15.0, 360.0)
    jersey(b, "Barrera_Menu", 1500.0, -150.0, -30.0)
    rubble_pile(b, "Escombros_Menu", 1300.0, 700.0, 300.0, 18)
    crater(b, "Crater_Menu", -700.0, 400.0, 220.0)
    street_lamp(b, "Farola_Menu", -1200.0, 1050.0, -1, broken=True)
    for index in range(3):
        barrel(b, "Bidon_Menu_{}".format(index), 1900.0 + index * 70.0, 500.0, "Oxido", tipped=index == 2)

    build_atmosphere(b, sun_pitch=-9.0, sun_yaw=200.0)

    camera = b.actor(unreal.CameraActor, (-1400.0, -1500.0, 260.0), (-4.0, 52.0, 0.0), "Camara_Menu", "Logica")
    if camera:
        try:
            component = camera.get_editor_property("camera_component")
            component.set_editor_property("field_of_view", 60.0)
        except Exception as error:
            warn("cámara del menú: {}".format(error))

    set_level_game_mode(getattr(unreal, "ShooterMenuGameMode", None), "ShooterMenuGameMode")
    LEVEL_SUB.save_current_level()
    log("decorado del menú: {} piezas".format(b.count))


# ==========================================================================

def run():
    # Semilla fija: el mapa sale igual en cada ejecución y en cada máquina.
    random.seed(1337)

    log("motor: {}".format(unreal.SystemLibrary.get_engine_version()))
    ensure_dirs(REAL_DIR, MAT_DIR, PHYS_DIR, FX_DIR, DATA_DIR, WEAPONS_BP_DIR, CORE_DIR, "/Game/Maps")

    log("--- materiales ---")
    physical = build_physical_materials()
    materials = build_surfaces(physical)
    build_effect_materials()

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
        mode_bp = build_game_mode(player_bp, weapon_bps)

    log("--- menú ---")
    build_menu_level(materials, physical)

    log("--- nivel ---")
    random.seed(1337)
    open_level(LEVEL_PATH)
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
        set_level_game_mode(blueprint_class(mode_bp), "BP_ModoRealista")
    log("piezas de geometría: {}".format(b.count))

    LEVEL_SUB.save_current_level()
    log("")
    log("Distrito listo. Pulsa Play (Net Mode: Play As Client para probar en red).")
    log("El juego empaquetado arranca en Lvl_Menu: Jugar > Desplegar en Distrito.")
    log("F1 muestra los controles en partida; L abre el equipamiento.")


try:
    run()
except Exception as error:
    import traceback
    unreal.log_error("[distrito] FALLÓ: {}".format(error))
    unreal.log_error(traceback.format_exc())
    unreal.log_error("[distrito] Copia este error y pásalo para diagnosticarlo.")
