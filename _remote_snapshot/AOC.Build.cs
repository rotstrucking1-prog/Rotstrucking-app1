// AOC.Build.cs — Architect of Creation
// Full module dependency list for all game systems:
//   Core, AI, UI, Niagara VFX, ProceduralMesh (voxel terrain),
//   Enhanced Input, Landscape, GameplayAbilities, Water, JSON.

using UnrealBuildTool;
using System.IO;

public class AOC : ModuleRules
{
    public AOC(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Auto-add all subdirectories to include paths so cross-directory
        // includes like #include "AoCVoxelTypes.h" resolve correctly.
        PublicIncludePaths.Add(ModuleDirectory);
        string[] SubDirs = Directory.GetDirectories(ModuleDirectory, "*", SearchOption.AllDirectories);
        foreach (string Dir in SubDirs)
        {
            PublicIncludePaths.Add(Dir);
        }

        PublicDependencyModuleNames.AddRange(new string[]
        {
            // Engine core
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",

            // UI
            "UMG",
            "Slate",
            "SlateCore",

            // AI
            "AIModule",
            "NavigationSystem",
            "GameplayTasks",

            // Gameplay Ability System
            "GameplayAbilities",
            "GameplayTags",

            // VFX
            "Niagara",

            // Voxel terrain (ProceduralMeshComponent)
            "ProceduralMeshComponent",

            // Input
            "EnhancedInput",

            // Landscape (surface height queries for world generation)
            "Landscape",

            // Foliage (required by LandscapeEdit.h -> InstancedFoliageActor.h)
            "Foliage",

            // Rendering (FlushRenderingCommands for heightmap texture sync)
            "RenderCore",

            // Water (existing dependency)
            "Water",

            // Asset management
            "AssetRegistry",

            // JSON (AnimLab + NPC AI)
            "Json",
            "JsonUtilities"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });
    }
}
