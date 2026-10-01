#include "AutoGrindSettings.h"

AutoGrindCore::Settings UAutoGrindSettings::ToCore() const
{
	AutoGrindCore::Settings Out;
	Out.MaxTopSlopeDegrees = MaxTopSlope;
	Out.MinDrop = MinDrop;
	Out.SteepLipDegrees = SteepLip;
	Out.MinLength = MinLength;
	Out.MaxLineSlopeDegrees = MaxLineSlope;
	Out.MaxCornerDegrees = MaxCorner;
	Out.RailMaxWidth = RailMaxWidth;
	Out.RailMinDrop = RailMinDrop;
	Out.GapBridge = GapBridge;
	Out.JoinGap = JoinGap;
	Out.RailJoinGap = RailJoinGap;
	Out.ProbeDistance = ProbeDistance;
	Out.SampleSpacing = SampleSpacing;
	Out.Clearance = Clearance;
	Out.MaxDropSearch = MaxDropSearch;
	Out.WeldTolerance = WeldTolerance;
	Out.SimplifyTolerance = SimplifyTolerance;
	return Out;
}
