#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateIcon.h"

class FSlateStyleSet;

// The plugin's own icon, for the tab, the menus and the toolbar button.
class FAutoGrindStyle
{
public:
	static void Register();
	static void Unregister();
	static FName GetStyleSetName();
	static FSlateIcon Icon();

private:
	static TSharedPtr<FSlateStyleSet> Style;
};
