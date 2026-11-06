/**
 * MODevCinema.h - the dev console's `cinema` mode: two captured camera positions and the move between them (Wes, Nov 6 2026)
 *
 * "cinema mode in console that has some controls to capture camera start and capture camera end and then you can press play on the cinema widget and it removes
 * everything from the screen and plays the camera movement from start to finish". AMOPlayerController holds one of these and drives the view through a camera actor.
 * Local to the player whose view it is: nothing is sent to the server and nothing is saved. Header only, so the interpolation can be tested on its own.
 */

#pragma once

#include "CoreMinimal.h"

/** Where the camera was when a capture button was pressed. */
struct FMODevCinemaShot
{
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	float FieldOfView = 90.0f;
	bool bSet = false;
};

struct FMODevCinema
{
	FMODevCinemaShot Start;
	FMODevCinemaShot End;
	float Seconds = 10.0f;
	/** Ease out of the start and into the end, rather than a constant speed. */
	bool bSmooth = true;
	/** Take the player's own body out of the shot while it plays. */
	bool bHideBody = true;

	bool IsReady() const { return Start.bSet && End.bSet; }

	/** The camera at Alpha (0 = the start, 1 = the end; clamped). The shortest way round for the rotation. */
	static FMODevCinemaShot Evaluate(const FMODevCinemaShot& From, const FMODevCinemaShot& To, float Alpha, bool bSmooth)
	{
		const float Clamped = FMath::Clamp(Alpha, 0.0f, 1.0f);
		const float Eased = bSmooth ? FMath::SmoothStep(0.0f, 1.0f, Clamped) : Clamped;
		FMODevCinemaShot Shot;
		Shot.Location = FMath::Lerp(From.Location, To.Location, (double)Eased);
		Shot.Rotation = FQuat::Slerp(From.Rotation, To.Rotation, Eased);
		Shot.FieldOfView = FMath::Lerp(From.FieldOfView, To.FieldOfView, Eased);
		Shot.bSet = true;
		return Shot;
	}
};
