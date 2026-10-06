// PROJECT ECLIPSE - Unreal build rules for the single runtime module "Eclipse".
//
// Purpose
//   Declares the dependencies of the Eclipse gameplay module. Everything gameplay
//   related lives in this one module; there is deliberately no plugin split while the
//   vertical slice is in development, because the systems are still changing shape and a
//   plugin boundary would add build friction without buying anything.

using UnrealBuildTool;

public class Eclipse : ModuleRules
{
	public Eclipse(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"GameplayTasks",
			"GameplayAbilities",
			"UMG",
			"Slate",
			"SlateCore",
			"AIModule",
			"NavigationSystem",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ChaosVehicles",
			"Niagara",
			"AudioModulation",
			"AudioMixer",
			"CommonUI",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"PhysicsCore",
			"RenderCore",
			"ApplicationCore",
			"Json",
			"JsonUtilities"
		});

		// Editor-only: content validation and the vertical slice importer.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"AssetRegistry",
				"EditorSubsystem",
				"ToolMenus"
			});
		}

		// Server targets do not need rendering or audio.
		if (Target.Type == TargetType.Server)
		{
			PrivateDependencyModuleNames.Remove("Niagara");
			PrivateDependencyModuleNames.Remove("AudioMixer");
		}
	}
}
