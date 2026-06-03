import unreal, os

REPO = r"C:\Users\Bradh\Downloads\Rotstrucking-app1"
RIGDIR = os.path.join(REPO, "rigwork")
DEST = "/Game/Characters/AoC"
MEL = unreal.MaterialEditingLibrary
ATH = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

def import_tex(png, dest, name, srgb, masks=False):
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    ATH.import_asset_tasks([task])
    p = task.get_editor_property("imported_object_paths")
    if not p:
        print("TEX_FAIL:", png); return None
    path = str(p[0])
    t = unreal.load_asset(path)
    if t:
        t.set_editor_property("srgb", srgb)
        if masks:
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        EAL.save_asset(path)
    return path

def make_material(tag, dest):
    matname = "M_" + tag.capitalize()
    matpath = dest + "/" + matname
    if EAL.does_asset_exist(matpath):
        EAL.delete_asset(matpath)
    mat = ATH.create_asset(matname, dest, unreal.Material, unreal.MaterialFactoryNew())
    # textures
    base = import_tex(os.path.join(RIGDIR, tag + "_BaseColor.png"), dest, "T_" + tag + "_BaseColor", True)
    norm = import_tex(os.path.join(RIGDIR, tag + "_Normal.png"), dest, "T_" + tag + "_Normal", False)
    orm  = import_tex(os.path.join(RIGDIR, tag + "_ORM.png"), dest, "T_" + tag + "_ORM", False, masks=True)
    # set normal texture compression to normalmap
    nt = unreal.load_asset(norm)
    if nt:
        nt.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        nt.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
        EAL.save_asset(norm)
    # base color node
    bc = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, -300)
    bc.texture = unreal.load_asset(base)
    MEL.connect_material_property(bc, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    # normal node
    nn = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 100)
    nn.texture = unreal.load_asset(norm)
    nn.sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    MEL.connect_material_property(nn, "RGB", unreal.MaterialProperty.MP_NORMAL)
    # ORM node: R=AO, G=Roughness, B=Metallic
    on = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 500)
    on.texture = unreal.load_asset(orm)
    on.sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    MEL.connect_material_property(on, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    MEL.connect_material_property(on, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(on, "B", unreal.MaterialProperty.MP_METALLIC)
    MEL.recompile_material(mat)
    EAL.save_asset(matpath)
    return matpath

def assign(tag, dest, matpath):
    # find skeletal mesh in dest/Tag
    d = dest + "/" + tag.capitalize()
    assets = EAL.list_assets(d, recursive=True)
    sm = None
    for a in assets:
        obj = unreal.load_asset(a)
        if isinstance(obj, unreal.SkeletalMesh):
            sm = obj; smpath = a; break
    if not sm:
        print("NO_SKELMESH_FOR:", tag); return
    mat = unreal.load_asset(matpath)
    mats = sm.get_editor_property("materials")
    for i in range(len(mats)):
        mats[i].set_editor_property("material_interface", mat)
    sm.set_editor_property("materials", mats)
    EAL.save_asset(smpath)
    print("ASSIGNED_%s -> %s (%d slots)" % (tag, matpath, len(mats)))

print("=== MATERIAL SETUP START ===")
for tag in ["warrior", "oracle"]:
    dest = DEST + "/" + tag.capitalize()
    mp = make_material(tag, dest)
    print("MATERIAL_%s:" % tag, mp)
    assign(tag, dest, mp)
print("=== MATERIAL SETUP DONE ===")
