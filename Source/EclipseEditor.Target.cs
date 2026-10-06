// PROJECT ECLIPSE - editor target.
//
// Purpose
//   Builds UnrealEditor with the Eclipse module loaded. This is the everyday target: it is
//   the only one that compiles the editor-only content tools and the vertical slice
//   importer.

using UnrealBuildTool;
using System.Collections.Generic;

public class EclipseEditorTarget : TargetRules
{
	public EclipseEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;

		ExtraModuleNames.Add("Eclipse");
	}
}
