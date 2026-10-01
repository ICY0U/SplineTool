#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AutoGrindTestCommandlet.generated.h"

// In-engine checks for what the standalone scorer cannot see: reading UE meshes with the right
// winding, collision traces as the drop test, mirrored transforms and the viewport preview.
//   UnrealEditor-Cmd RollerSkate.uproject -run=AutoGrindTest -unattended -nullrhi
// Logs AUTOGRIND_TEST_PASS on success; any failed check stops with a fatal error naming it.
UCLASS()
class UAutoGrindTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
