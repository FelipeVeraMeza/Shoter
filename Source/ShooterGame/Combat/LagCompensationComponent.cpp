#include "Combat/LagCompensationComponent.h"
#include "Player/ShooterCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ULagCompensationComponent::ULagCompensationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

void ULagCompensationComponent::BeginPlay()
{
	Super::BeginPlay();

	// El historial solo sirve en el servidor: es quien resuelve los impactos.
	if (GetOwnerRole() != ROLE_Authority)
	{
		SetComponentTickEnabled(false);
	}
}

void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SaveFrame();
}

void ULagCompensationComponent::SaveFrame()
{
	const AShooterCharacter* Character = Cast<AShooterCharacter>(GetOwner());
	if (!Character || !Character->GetCapsuleComponent())
	{
		return;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();

	FShooterFramePackage Frame;
	Frame.Time = GetWorld()->GetTimeSeconds();
	Frame.Location = Capsule->GetComponentLocation();
	Frame.Rotation = Capsule->GetComponentRotation();
	Frame.CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	Frame.CapsuleRadius = Capsule->GetScaledCapsuleRadius();

	History.Add(Frame);

	const float OldestAllowed = Frame.Time - MaxRecordTime;
	int32 FirstValid = 0;
	while (FirstValid < History.Num() && History[FirstValid].Time < OldestAllowed)
	{
		++FirstValid;
	}
	if (FirstValid > 0)
	{
		History.RemoveAt(0, FirstValid, false);
	}
}

bool ULagCompensationComponent::GetFrameAtTime(float WorldTime, FShooterFramePackage& OutFrame) const
{
	if (History.Num() == 0)
	{
		return false;
	}

	if (WorldTime >= History.Last().Time)
	{
		OutFrame = History.Last();
		return true;
	}

	if (WorldTime <= History[0].Time)
	{
		OutFrame = History[0];
		return true;
	}

	for (int32 i = History.Num() - 1; i > 0; --i)
	{
		const FShooterFramePackage& Newer = History[i];
		const FShooterFramePackage& Older = History[i - 1];

		if (WorldTime >= Older.Time && WorldTime <= Newer.Time)
		{
			const float Span = Newer.Time - Older.Time;
			const float Alpha = Span > KINDA_SMALL_NUMBER ? (WorldTime - Older.Time) / Span : 1.f;

			OutFrame.Time = WorldTime;
			OutFrame.Location = FMath::Lerp(Older.Location, Newer.Location, Alpha);
			OutFrame.Rotation = FMath::Lerp(Older.Rotation, Newer.Rotation, Alpha);
			OutFrame.CapsuleHalfHeight = FMath::Lerp(Older.CapsuleHalfHeight, Newer.CapsuleHalfHeight, Alpha);
			OutFrame.CapsuleRadius = FMath::Lerp(Older.CapsuleRadius, Newer.CapsuleRadius, Alpha);
			return true;
		}
	}

	return false;
}

bool ULagCompensationComponent::RayIntersectsCapsule(
	const FVector& RayStart,
	const FVector& RayEnd,
	const FShooterFramePackage& Frame,
	FVector& OutImpactPoint,
	float& OutDistanceAlongRay,
	float& OutHeightFraction)
{
	const float SegmentHalfLength = FMath::Max(Frame.CapsuleHalfHeight - Frame.CapsuleRadius, 0.f);
	const FVector CapsuleUp = Frame.Rotation.Quaternion().GetUpVector();

	const FVector AxisTop = Frame.Location + CapsuleUp * SegmentHalfLength;
	const FVector AxisBottom = Frame.Location - CapsuleUp * SegmentHalfLength;

	FVector ClosestOnRay, ClosestOnAxis;
	FMath::SegmentDistToSegmentSafe(RayStart, RayEnd, AxisBottom, AxisTop, ClosestOnRay, ClosestOnAxis);

	if (FVector::Dist(ClosestOnRay, ClosestOnAxis) > Frame.CapsuleRadius)
	{
		return false;
	}

	OutImpactPoint = ClosestOnRay;
	OutDistanceAlongRay = FVector::Dist(RayStart, ClosestOnRay);

	const float TotalHeight = FMath::Max(Frame.CapsuleHalfHeight * 2.f, KINDA_SMALL_NUMBER);
	const float HeightAboveFeet = FVector::DotProduct(ClosestOnRay - (Frame.Location - CapsuleUp * Frame.CapsuleHalfHeight), CapsuleUp);
	OutHeightFraction = FMath::Clamp(HeightAboveFeet / TotalHeight, 0.f, 1.f);

	return true;
}

bool ULagCompensationComponent::FindBestCharacterHit(
	UWorld* World,
	const FVector& TraceStart,
	const FVector& TraceEnd,
	float RewindTime,
	const AActor* IgnoredActor,
	AShooterCharacter*& OutHitCharacter,
	FVector& OutImpactPoint,
	bool& bOutHeadshot)
{
	OutHitCharacter = nullptr;
	bOutHeadshot = false;

	if (!World)
	{
		return false;
	}

	float ClosestDistance = TNumericLimits<float>::Max();

	for (TActorIterator<AShooterCharacter> It(World); It; ++It)
	{
		AShooterCharacter* Candidate = *It;
		if (!Candidate || Candidate == IgnoredActor || Candidate->IsDead())
		{
			continue;
		}

		ULagCompensationComponent* LagComp = Candidate->GetLagCompensation();
		if (!LagComp)
		{
			continue;
		}

		FShooterFramePackage Frame;
		if (!LagComp->GetFrameAtTime(RewindTime, Frame))
		{
			continue;
		}

		FVector Impact;
		float Distance;
		float HeightFraction;
		if (!RayIntersectsCapsule(TraceStart, TraceEnd, Frame, Impact, Distance, HeightFraction))
		{
			continue;
		}

		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			OutHitCharacter = Candidate;
			OutImpactPoint = Impact;
			bOutHeadshot = HeightFraction >= LagComp->HeadshotHeightFraction;
		}
	}

	return OutHitCharacter != nullptr;
}
