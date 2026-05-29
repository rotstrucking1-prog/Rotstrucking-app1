// AOC.Build.cs — Architect of Creation
// Full module dependency list for all game systems:
//   Core, AI, UI, Niagara VFX, ProceduralMesh (voxel terrain),
//   Enhanced Input, Landscape (surface height queries).

using UnrealBuildTool;

public class AOC : ModuleRules
{
    public AOC(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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

            // VFX
            "Niagara",

            // Voxel terrain (ProceduralMeshComponent)
            "ProceduralMeshComponent",

            // Input
            "EnhancedInput",

            // Landscape (surface height queries for world generation)
            "Landscape"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });
    }
}
