#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AutoGrindInspectCommandlet.generated.h"

// Dumps every GrindActor in a map, so generated ones can be compared with hand-placed ones.
//   UnrealEditor-Cmd RollerSkate.uproject -run=AutoGrindInspect -map=/Game/MainFolder/CustomMaps/Test_Map/Test_Map -unattended -nullrhi
UCLASS()
class UAutoGrindInspectCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
