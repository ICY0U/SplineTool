#include "AutoGrindSettings.h"

namespace AutoGrind
{
	namespace
	{
		// Splits a name into words: at anything not a letter or digit, where lower case turns to upper case,
		// between letters and digits, and before the last capital of a run of capitals ("HTTPServer").
		void SplitWords(const FString& Name, TArray<FString>& Out)
		{
			FString Current;
			auto Flush = [&Out, &Current]()
			{
				if (!Current.IsEmpty())
				{
					Out.Add(Current);
					Current.Reset();
				}
			};
			for (int32 I = 0; I < Name.Len(); ++I)
			{
				const TCHAR C = Name[I];
				if (!FChar::IsAlnum(C))
				{
					Flush();
					continue;
				}
				if (!Current.IsEmpty())
				{
					const TCHAR Before = Name[I - 1];
					const bool bCaseBreak = FChar::IsLower(Before) && FChar::IsUpper(C);
					const bool bDigitBreak = FChar::IsDigit(Before) != FChar::IsDigit(C);
					const bool bAcronymBreak = I + 1 < Name.Len() && FChar::IsUpper(Before) && FChar::IsUpper(C) && FChar::IsLower(Name[I + 1]);
					if (bCaseBreak || bDigitBreak || bAcronymBreak)
					{
						Flush();
					}
				}
				Current.AppendChar(C);
			}
			Flush();
		}
	}
}

void UAutoGrindSettings::ApplyPreset(EAutoGrindPreset Preset)
{
	// The Skatepark preset is the defaults; the others start from it.
	MaxTopSlope = 45;
	MinDrop = 25;
	LowLedges = EAutoGrindLowLedges::Suggest;
	LowLedgeMinDrop = 12;
	SteepLip = 40;
	MinEdgeAngle = 30;
	GapBridge = 6;
	bCheckWalls = true;
	RailMaxWidth = 15;
	RailMaxThickness = 20;
	RailMinDrop = 10;
	RoundTopMaxWidth = 40;
	bDetectRidges = false;
	MinRidgeAngle = 70;
	MinLength = 50;
	MaxLineSlope = 45;
	MaxCorner = 40;
	bJoinAcrossMeshes = true;
	JoinGap = 10;
	RailJoinGap = 40;
	MaxDrop = 0;
	HighDrop = 150;
	KeepConfidence = 0.5;
	bRepairWinding = true;
	ProbeDistance = 15;
	SampleSpacing = 25;
	Clearance = 150;
	MaxDropSearch = 5000;
	WeldTolerance = 0.1;
	SimplifyTolerance = 1;
	switch (Preset)
	{
	case EAutoGrindPreset::Street:
		// Curbs and low walls are the street's ledges; roofs and high wall tops are out of reach.
		LowLedges = EAutoGrindLowLedges::Keep;
		MaxDrop = 400;
		HighDrop = 120;
		break;
	case EAutoGrindPreset::Strict:
		LowLedges = EAutoGrindLowLedges::Off;
		MinEdgeAngle = 45;
		MinLength = 100;
		MaxDrop = 300;
		KeepConfidence = 0.7;
		break;
	case EAutoGrindPreset::Loose:
		LowLedges = EAutoGrindLowLedges::Keep;
		LowLedgeMinDrop = 8;
		MinEdgeAngle = 20;
		MinLength = 30;
		bDetectRidges = true;
		KeepConfidence = 0.3;
		break;
	case EAutoGrindPreset::Skatepark:
	default:
		break;
	}
}

void UAutoGrindSettings::ResetDetection()
{
	ApplyPreset(EAutoGrindPreset::Skatepark);
}

AutoGrindCore::Settings UAutoGrindSettings::ToCore() const
{
	AutoGrindCore::Settings Out;
	Out.MaxTopSlopeDegrees = MaxTopSlope;
	Out.MinDrop = MinDrop;
	Out.LowLedges = LowLedges == EAutoGrindLowLedges::Off ? AutoGrindCore::LowLedgeMode::Off : LowLedges == EAutoGrindLowLedges::Keep ? AutoGrindCore::LowLedgeMode::Keep : AutoGrindCore::LowLedgeMode::Suggest;
	Out.LowLedgeMinDrop = LowLedgeMinDrop;
	Out.SteepLipDegrees = SteepLip;
	Out.MinEdgeAngleDegrees = MinEdgeAngle;
	Out.GapBridge = GapBridge;
	Out.RailMaxWidth = RailMaxWidth;
	Out.RailMaxThickness = RailMaxThickness;
	Out.RailMinDrop = RailMinDrop;
	Out.RoundTopMaxWidth = RoundTopMaxWidth;
	Out.bDetectRidges = bDetectRidges;
	Out.MinRidgeAngleDegrees = MinRidgeAngle;
	Out.MinLength = MinLength;
	Out.MaxLineSlopeDegrees = MaxLineSlope;
	Out.MaxCornerDegrees = MaxCorner;
	Out.bJoinAcrossMeshes = bJoinAcrossMeshes;
	Out.JoinGap = JoinGap;
	Out.RailJoinGap = RailJoinGap;
	Out.MaxDrop = MaxDrop;
	Out.HighDrop = HighDrop;
	Out.KeepConfidence = KeepConfidence;
	Out.bRepairWinding = bRepairWinding;
	Out.ProbeDistance = ProbeDistance;
	Out.SampleSpacing = SampleSpacing;
	Out.Clearance = Clearance;
	Out.MaxDropSearch = MaxDropSearch;
	Out.WeldTolerance = WeldTolerance;
	Out.SimplifyTolerance = SimplifyTolerance;
	return Out;
}

bool UAutoGrindSettings::IsExcludedName(const FString& Name) const
{
	if (ExcludeNames.IsEmpty())
	{
		return false;
	}
	TArray<FString> Words;
	AutoGrind::SplitWords(Name, Words);
	for (const FString& Word : Words)
	{
		for (const FString& Excluded : ExcludeNames)
		{
			if (Excluded.IsEmpty())
			{
				continue;
			}
			// A plural counts too: "Trees" for "Tree".
			if (Word.Equals(Excluded, ESearchCase::IgnoreCase) || Word.Equals(Excluded + TEXT("s"), ESearchCase::IgnoreCase) || Word.Equals(Excluded + TEXT("es"), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
	}
	return false;
}
