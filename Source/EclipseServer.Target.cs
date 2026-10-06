// PROJECT ECLIPSE - dedicated server target.
//
// Purpose
//   Builds the headless server. The server target is not optional for this project: the
//   whole authority model depends on every gameplay system running without a renderer or a
//   local player, so building it is what keeps that honest.

using UnrealBuildTool;
using System.Collections.Generic;

public class EclipseServerTarget : TargetRules
{
	public EclipseServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;

		ExtraModuleNames.Add("Eclipse");
	}
}
