#include "Core/ControlZone.h"
#include "Core/ShooterGameState.h"
#include "Core/ShooterPlayerState.h"
#include "Player/ShooterCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/** Color del marcador según el equipo que controla la zona. */
	FLinearColor TeamColor(EShooterTeam Team)
	{
		switch (Team)
		{
		case EShooterTeam::Alpha: return FLinearColor(0.15f, 0.5f, 1.f, 1.f);
		case EShooterTeam::Bravo: return FLinearColor(1.f, 0.3f, 0.15f, 1.f);
		default: return FLinearColor(0.6f, 0.6f, 0.6f, 1.f);
		}
	}
}

AControlZone::AControlZone()
{
	PrimaryActorTick.bCanEverTick = true;

	// Solo el servidor calcula la captura; el cliente recibe el resultado.
	bReplicates = true;

	CaptureVolume = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CaptureVolume"));
	RootComponent = CaptureVolume;
	CaptureVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CaptureVolume->SetCollisionResponseToAllChannels(ECR_Overlap);

	// El volumen no debe frenar balas ni jugadores: solo detecta presencia.
	CaptureVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	CaptureVolume->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	ZoneMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMarker"));
	ZoneMarker->SetupAttachment(RootComponent);
	ZoneMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Cilindro del motor: la zona es visible antes de tener arte propio.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		ZoneMarker->SetStaticMesh(CylinderAsset.Object);
	}
}

void AControlZone::BeginPlay()
{
	Super::BeginPlay();

	CaptureVolume->SetCapsuleRadius(ZoneRadius);
	CaptureVolume->SetCapsuleHalfHeight(ZoneHeight * 0.5f);

	// El cilindro del motor mide 100 uu de radio y 100 de alto por defecto.
	const float MarkerScale = ZoneRadius / 50.f;
	ZoneMarker->SetWorldScale3D(FVector(MarkerScale, MarkerScale, 0.05f));
	ZoneMarker->SetRelativeLocation(FVector(0.f, 0.f, -ZoneHeight * 0.5f));

	RefreshMarkerColor();
}

void AControlZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AControlZone, OwningTeam);
	DOREPLIFETIME(AControlZone, ZoneState);
	DOREPLIFETIME(AControlZone, CaptureProgress);
	DOREPLIFETIME(AControlZone, CapturingTeam);
}

void AControlZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	// La captura solo corre con la partida en marcha: sin esto las zonas
	// seguirian sumando puntos tras el final.
	const AShooterGameState* GameState = GetWorld()->GetGameState<AShooterGameState>();
	if (!GameState || GameState->GetMatchPhase() != EMatchPhase::InProgress)
	{
		return;
	}

	int32 AlphaCount = 0;
	int32 BravoCount = 0;
	CountOccupants(AlphaCount, BravoCount);

	UpdateCapture(DeltaSeconds, AlphaCount, BravoCount);
	AwardPoints(DeltaSeconds);
}

void AControlZone::CountOccupants(int32& OutAlpha, int32& OutBravo) const
{
	OutAlpha = 0;
	OutBravo = 0;

	TArray<AActor*> Overlapping;
	CaptureVolume->GetOverlappingActors(Overlapping, AShooterCharacter::StaticClass());

	for (const AActor* Actor : Overlapping)
	{
		const AShooterCharacter* Character = Cast<AShooterCharacter>(Actor);

		// Un cadáver dentro de la zona no debe contar para la captura.
		if (!Character || Character->IsDead())
		{
			continue;
		}

		const AShooterPlayerState* State = Character->GetPlayerState<AShooterPlayerState>();
		if (!State)
		{
			continue;
		}

		switch (State->GetTeam())
		{
		case EShooterTeam::Alpha: ++OutAlpha; break;
		case EShooterTeam::Bravo: ++OutBravo; break;
		default: break;
		}
	}
}

void AControlZone::UpdateCapture(float DeltaSeconds, int32 AlphaCount, int32 BravoCount)
{
	// Zona disputada: el progreso se congela. Revertirlo haría que dos
	// jugadores enfrentados se anularan sin que ninguno avanzara nunca.
	if (AlphaCount > 0 && BravoCount > 0)
	{
		ZoneState = EZoneState::Contested;
		return;
	}

	if (AlphaCount == 0 && BravoCount == 0)
	{
		// Sin nadie dentro, una captura a medias se mantiene: obliga a volver
		// a la zona en vez de premiar con una reversión automática.
		ZoneState = (OwningTeam == EShooterTeam::None)
			? EZoneState::Neutral : EZoneState::Captured;
		return;
	}

	const EShooterTeam Attacker = (AlphaCount > 0) ? EShooterTeam::Alpha : EShooterTeam::Bravo;
	const int32 Occupants = (AlphaCount > 0) ? AlphaCount : BravoCount;

	// El primer ocupante aporta el ritmo base; cada uno extra, una fracción
	// decreciente. Sin esto un grupo capturaría en un parpadeo.
	const float Rate = (1.f + (Occupants - 1) * ExtraOccupantBonus) / CaptureTime;

	if (OwningTeam == Attacker)
	{
		// Ya es suya: solo consolidar lo que falte del progreso.
		CaptureProgress = FMath::Min(CaptureProgress + Rate * DeltaSeconds, 1.f);
		ZoneState = EZoneState::Captured;
		return;
	}

	if (CapturingTeam != Attacker)
	{
		// Cambió quién intenta capturar: el progreso del anterior no sirve.
		CapturingTeam = Attacker;
		CaptureProgress = 0.f;
	}

	CaptureProgress += Rate * DeltaSeconds;
	ZoneState = EZoneState::Capturing;

	if (CaptureProgress >= 1.f)
	{
		CaptureProgress = 1.f;
		OwningTeam = Attacker;
		ZoneState = EZoneState::Captured;
		RefreshMarkerColor();
	}
}

void AControlZone::AwardPoints(float DeltaSeconds)
{
	if (OwningTeam == EShooterTeam::None || PointsPerSecond <= 0.f)
	{
		return;
	}

	AShooterGameState* GameState = GetWorld()->GetGameState<AShooterGameState>();
	if (!GameState)
	{
		return;
	}

	// El marcador es entero, pero el ritmo es por segundo: acumular la parte
	// fraccionaria evita perderla al truncar en cada tick.
	PointAccumulator += PointsPerSecond * DeltaSeconds;

	const int32 WholePoints = FMath::FloorToInt(PointAccumulator);
	if (WholePoints > 0)
	{
		PointAccumulator -= WholePoints;
		GameState->AddTeamScore(OwningTeam, WholePoints);
	}
}

void AControlZone::OnRep_OwningTeam()
{
	RefreshMarkerColor();
}

void AControlZone::RefreshMarkerColor()
{
	if (!ZoneMarker)
	{
		return;
	}

	UMaterialInstanceDynamic* Material = ZoneMarker->CreateAndSetMaterialInstanceDynamic(0);
	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Color"), TeamColor(OwningTeam));
		Material->SetVectorParameterValue(TEXT("BaseColor"), TeamColor(OwningTeam));
	}
}
