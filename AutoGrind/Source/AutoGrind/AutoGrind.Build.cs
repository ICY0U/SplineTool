using UnrealBuildTool;

public class AutoGrind : ModuleRules
{
	public AutoGrind(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// The detector core (Private/Core) is engine-free and keeps its helpers in an anonymous
		// namespace; building each file on its own keeps them from meeting other files' names.
		bUseUnity = false;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"ToolMenus",
			"PropertyEditor",
			"MeshDescription",
			"StaticMeshDescription",
		});
	}
}
