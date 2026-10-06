// PROJECT ECLIPSE - Eclipse module implementation.
//
// Purpose
//   Registers the primary game module and answers the module-level helpers declared in
//   Eclipse.h. Nothing gameplay related belongs here.

#include "Eclipse.h"

#include "Core/EclipseTypes.h"

DEFINE_LOG_CATEGORY(LogEclipse);

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, Eclipse, "Eclipse");

namespace EclipseModule
{
	const TCHAR* GetProjectVersion()
	{
		return TEXT("0.1.0");
	}

	int32 GetSaveSchemaVersion()
	{
		return UEclipseSaveGameVersion::Current;
	}
}
