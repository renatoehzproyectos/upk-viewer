#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "UnrealMaterial/UnMaterial.h"
#include "UnrealMaterial/UnMaterial3.h"

// Used by some UEViewer export paths referenced from material/texture code.
bool GExportInProgress = false;

void UMaterial3::PostLoad()
{
	guard(UMaterial3::PostLoad);
	Super::PostLoad();
	unguard;
}
