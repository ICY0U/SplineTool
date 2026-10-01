#include "AutoGrindStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FSlateStyleSet> FAutoGrindStyle::Style;

FName FAutoGrindStyle::GetStyleSetName()
{
	static const FName Name(TEXT("AutoGrindStyle"));
	return Name;
}

void FAutoGrindStyle::Register()
{
	if (Style.IsValid())
	{
		return;
	}
	Style = MakeShared<FSlateStyleSet>(GetStyleSetName());
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AutoGrind")))
	{
		Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}
	const FString IconPath = Style->RootToContentDir(TEXT("Icon128"), TEXT(".png"));
	Style->Set("AutoGrind.Icon", new FSlateImageBrush(IconPath, FVector2D(40, 40)));
	Style->Set("AutoGrind.Icon.Small", new FSlateImageBrush(IconPath, FVector2D(16, 16)));
	FSlateStyleRegistry::RegisterSlateStyle(*Style);
}

void FAutoGrindStyle::Unregister()
{
	if (Style.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*Style);
		Style.Reset();
	}
}

FSlateIcon FAutoGrindStyle::Icon()
{
	return FSlateIcon(GetStyleSetName(), "AutoGrind.Icon", "AutoGrind.Icon.Small");
}
