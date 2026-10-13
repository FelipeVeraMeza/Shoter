#include "Weapons/WeaponDataAsset.h"
#include "Curves/CurveFloat.h"

float UWeaponDataAsset::GetSecondsBetweenShots() const
{
	return 60.f / FMath::Max(RoundsPerMinute, 1.f);
}

float UWeaponDataAsset::GetDamageAtDistance(float Distance) const
{
	if (Distance <= EffectiveRange || Range <= EffectiveRange)
	{
		return Damage;
	}

	const float Alpha = FMath::Clamp((Distance - EffectiveRange) / (Range - EffectiveRange), 0.f, 1.f);
	const float Multiplier = DamageFalloff ? DamageFalloff->GetFloatValue(Alpha) : FMath::Lerp(1.f, 0.5f, Alpha);

	return Damage * FMath::Max(Multiplier, 0.f);
}

float UWeaponDataAsset::GetZoneMultiplier(EShooterHitZone Zone) const
{
	switch (Zone)
	{
	case EShooterHitZone::Head:		return HeadshotMultiplier;
	case EShooterHitZone::Stomach:	return StomachMultiplier;
	case EShooterHitZone::Arm:
	case EShooterHitZone::Leg:		return LimbMultiplier;
	default:						return 1.f;
	}
}

float UWeaponDataAsset::GetEmptyReloadTime() const
{
	return EmptyReloadTime > 0.f ? EmptyReloadTime : ReloadTime * 1.3f;
}

TArray<EWeaponFireMode> UWeaponDataAsset::GetFireModes() const
{
	if (AvailableFireModes.Num() > 0)
	{
		return AvailableFireModes;
	}
	return { FireMode };
}
