"""Runs automatically when the editor opens this project. Creates the two base materials the C++ expects
(/Game/Materials/M_BMLit and M_BMEmissive, each with a vector parameter "Color") if they do not exist yet.
Commit the generated .uasset files afterwards so packaged builds include them."""
import unreal

FOLDER = "/Game/Materials"


def make(name, emissive):
    path = f"{FOLDER}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return
    lib = unreal.MaterialEditingLibrary
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    color = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    if emissive:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        lib.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        lib.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
        rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 200)
        rough.set_editor_property("r", 0.8)
        lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.log(f"Blood Moon: created {path}")


make("M_BMLit", False)
make("M_BMEmissive", True)
