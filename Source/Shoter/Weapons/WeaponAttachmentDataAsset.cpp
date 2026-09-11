#include "Weapons/WeaponAttachmentDataAsset.h"

namespace
{
	/**
	 * Multiplica el modificador de las tres ranuras.
	 *
	 * Multiplicar en vez de sumar mantiene el efecto proporcional: dos
	 * accesorios que reducen la dispersión un 20 % dejan un 64 %, no un 60 %,
	 * y ninguna combinación puede llegar a cero por acumulación.
	 */
	template <typename TGetter>
	float CombineSlots(const UWeaponAttachmentDataAsset* Optic,
		const UWeaponAttachmentDataAsset* Barrel,
		const UWeaponAttachmentDataAsset* Magazine,
		TGetter&& Getter)
	{
		float Result = 1.f;

		if (Optic)
		{
			Result *= Getter(Optic);
		}
		if (Barrel)
		{
			Result *= Getter(Barrel);
		}
		if (Magazine)
		{
			Result *= Getter(Magazine);
		}

		return Result;
	}
}

float FWeaponAttachmentSet::GetDamageMultiplier() const
{
	return CombineSlots(Optic, Barrel, Magazine,
		[](const UWeaponAttachmentDataAsset* A) { return A->DamageMultiplier; });
}

float FWeaponAttachmentSet::GetSpreadMultiplier() const
{
	return CombineSlots(Optic, Barrel, Magazine,
		[](const UWeaponAttachmentDataAsset* A) { return A->SpreadMultiplier; });
}

float FWeaponAttachmentSet::GetRecoilMultiplier() const
{
	return CombineSlots(Optic, Barrel, Magazine,
		[](const UWeaponAttachmentDataAsset* A) { return A->RecoilMultiplier; });
}

float FWeaponAttachmentSet::GetMagazineMultiplier() const
{
	return CombineSlots(Optic, Barrel, Magazine,
		[](const UWeaponAttachmentDataAsset* A) { return A->MagazineMultiplier; });
}

float FWeaponAttachmentSet::GetReloadTimeMultiplier() const
{
	return CombineSlots(Optic, Barrel, Magazine,
		[](const UWeaponAttachmentDataAsset* A) { return A->ReloadTimeMultiplier; });
}

float FWeaponAttachmentSet::GetADSTimeMultiplier() const
{
	return CombineSlots(Optic, Barrel, Magazine,
		[](const UWeaponAttachmentDataAsset* A) { return A->ADSTimeMultiplier; });
}

float FWeaponAttachmentSet::GetEffectiveRangeMultiplier() const
{
	return CombineSlots(Optic, Barrel, Magazine,
		[](const UWeaponAttachmentDataAsset* A) { return A->EffectiveRangeMultiplier; });
}
