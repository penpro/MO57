// Tests only (it takes sines, so nothing that makes the world may call it): where a lake's own shape puts a point, for the tests that walk its shore. Lakes have lobes and a ragged edge since generator v13 (FMOWorldGenDomain::LakeShapeQ2), so "0.9 of the
// radius along the long axis" says nothing about the shore: ask for q instead (q = 1 on the shore, < 1 in the water).
#pragma once

#include "CoreMinimal.h"
#include "MOWorldGen.h"

namespace MOLakeTest
{
	/** World XY of the first point at q = Q going out from the lake's centre along Angle in its unit frame (0 = along the long axis, PI = the other way). A round lake's answer is q x (cos, sin). */
	inline FVector2D PointAtQ(const FMOWorldGen::FLake& L, float Q, float Angle = 0.0f)
	{
		const float Ca = FMath::Cos(Angle), Sa = FMath::Sin(Angle);
		auto QAt = [&](float R) { return FMath::Sqrt(FMOWorldGen::LakeShapeQ2(L, R * Ca, R * Sa)); };
		float Lo = 0.0f, Hi = 0.0f;
		for (float R = 0.005f; R < 8.0f; R += 0.005f)
		{
			if (QAt(R) >= Q)
			{
				Hi = R;
				break;
			}
			Lo = R;
		}
		for (int32 I = 0; I < 12; ++I)
		{
			const float Mid = 0.5f * (Lo + Hi);
			(QAt(Mid) >= Q ? Hi : Lo) = Mid;
		}
		const float Lx = Hi * Ca * L.RadiusCm, Ly = Hi * Sa * L.RadiusCm * L.Aspect;
		return FVector2D(L.Center.X + (double)L.CosAngle * Lx - (double)L.SinAngle * Ly, L.Center.Y + (double)L.SinAngle * Lx + (double)L.CosAngle * Ly);
	}
}
