// PROJECT ECLIPSE - standalone game target.
//
// Purpose
//   Builds the shipping/development client. The target exists separately from the editor
//   target because the game must build without the editor-only modules (content validation
//   and the vertical slice importer) that the Eclipse module pulls in when bBuildEditor.

using UnrealBuildTool;
using System.Collections.Generic;

public class EclipseTarget : TargetRules
{
	public EclipseTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;

		ExtraModuleNames.Add("Eclipse");
	}
}
