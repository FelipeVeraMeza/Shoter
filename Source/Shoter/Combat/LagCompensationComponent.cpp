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
	Frame.LeanOffset = Character->GetLeanWorldOffset();

	History.Add(Frame);

	const float OldestAllowed = Frame.Time - MaxRecordTime;
	int32 FirstValid = 0;
	while (FirstValid < History.Num() && History[FirstValid].Time < OldestAllowed)
	{
		++FirstValid;
	}
	if (FirstValid > 0)
	{
		History.RemoveAt(0, FirstValid, EAllowShrinking::No);
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
			OutFrame.LeanOffset = FMath::Lerp(Older.LeanOffset, Newer.LeanOffset, Alpha);
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
	float& OutHeightFraction,
	float& OutAxisDistanceFraction)
{
	const float SegmentHalfLength = FMath::Max(Frame.CapsuleHalfHeight - Frame.CapsuleRadius, 0.f);
	const FVector CapsuleUp = Frame.Rotation.Quaternion().GetUpVector();

	const FVector AxisTop = Frame.Location + CapsuleUp * SegmentHalfLength;
	const FVector AxisBottom = Frame.Location - CapsuleUp * SegmentHalfLength;

	FVector ClosestOnRay, ClosestOnAxis;
	FMath::SegmentDistToSegmentSafe(RayStart, RayEnd, AxisBottom, AxisTop, ClosestOnRay, ClosestOnAxis);

	const float AxisDistance = FVector::Dist(ClosestOnRay, ClosestOnAxis);
	if (AxisDistance > Frame.CapsuleRadius)
	{
		return false;
	}

	OutAxisDistanceFraction = AxisDistance / FMath::Max(Frame.CapsuleRadius, KINDA_SMALL_NUMBER);

	OutImpactPoint = ClosestOnRay;
	OutDistanceAlongRay = FVector::Dist(RayStart, ClosestOnRay);

	const float TotalHeight = FMath::Max(Frame.CapsuleHalfHeight * 2.f, KINDA_SMALL_NUMBER);
	const float HeightAboveFeet = FVector::DotProduct(ClosestOnRay - (Frame.Location - CapsuleUp * Frame.CapsuleHalfHeight), CapsuleUp);
	OutHeightFraction = FMath::Clamp(HeightAboveFeet / TotalHeight, 0.f, 1.f);

	return true;
}

bool ULagCompensationComponent::TraceAgainstFrame(
	const FVector& RayStart,
	const FVector& RayEnd,
	const FShooterFramePackage& Frame,
	FVector& OutImpactPoint,
	float& OutDistanceAlongRay,
	EShooterHitZone& OutZone) const
{
	const FVector Up = Frame.Rotation.Quaternion().GetUpVector();

	// Cabeza: esfera en lo alto de la cápsula, desplazada si el personaje se
	// asoma. Se prueba primero porque es la zona que más importa acertar.
	const FVector HeadCenter = Frame.Location
		+ Up * (Frame.CapsuleHalfHeight - HeadRadius - 4.f)
		+ Frame.LeanOffset;

	const FVector ClosestToHead = FMath::ClosestPointOnSegment(HeadCenter, RayStart, RayEnd);
	if (FVector::DistSquared(ClosestToHead, HeadCenter) <= FMath::Square(HeadRadius))
	{
		OutImpactPoint = ClosestToHead;
		OutDistanceAlongRay = FVector::Dist(RayStart, ClosestToHead);
		OutZone = EShooterHitZone::Head;
		return true;
	}

	// Cuerpo: la mitad superior acompaña la inclinación, así que se desplaza
	// la cápsula entera la mitad del desplazamiento de la cabeza.
	FShooterFramePackage BodyFrame = Frame;
	BodyFrame.Location += Frame.LeanOffset * 0.5f;

	float HeightFraction = 0.f;
	float AxisFraction = 0.f;
	if (!RayIntersectsCapsule(RayStart, RayEnd, BodyFrame, OutImpactPoint, OutDistanceAlongRay,
		HeightFraction, AxisFraction))
	{
		return false;
	}

	// La cápsula redondeada sobresale por encima de los hombros: lo que pasa
	// por ahí sin tocar la esfera de la cabeza es un fallo, no un impacto.
	if (HeightFraction >= ShoulderHeightFraction)
	{
		return false;
	}

	if (HeightFraction < WaistHeightFraction)
	{
		OutZone = EShooterHitZone::Leg;
	}
	else if (AxisFraction > ArmRadiusFraction)
	{
		// Los bordes de la cápsula a la altura del tronco son los brazos.
		OutZone = EShooterHitZone::Arm;
	}
	else
	{
		OutZone = HeightFraction >= ChestHeightFraction ? EShooterHitZone::Torso : EShooterHitZone::Stomach;
	}

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
	EShooterHitZone& OutZone)
{
	OutHitCharacter = nullptr;
	OutZone = EShooterHitZone::None;

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
		EShooterHitZone Zone;
		if (!LagComp->TraceAgainstFrame(TraceStart, TraceEnd, Frame, Impact, Distance, Zone))
		{
			continue;
		}

		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			OutHitCharacter = Candidate;
			OutImpactPoint = Impact;
			OutZone = Zone;
		}
	}

	return OutHitCharacter != nullptr;
}
