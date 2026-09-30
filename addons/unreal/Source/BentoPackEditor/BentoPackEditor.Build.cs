using UnrealBuildTool;

public class BentoPackEditor : ModuleRules
{
    public BentoPackEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "Paper2D",
                "BentoPack"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "UnrealEd",
                "AssetTools",
                "EditorStyle",
                "Paper2DEditor",
                "Projects",
                "Json",
                "JsonUtilities",
                "DesktopPlatform",
                "ContentBrowser",
                "WorkspaceMenuStructure"
            }
        );
    }
}
