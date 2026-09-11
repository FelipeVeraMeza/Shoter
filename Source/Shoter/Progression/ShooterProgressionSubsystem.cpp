#include "Progression/ShooterProgressionSubsystem.h"
#include "Progression/ShooterSaveGame.h"
#include "Weapons/WeaponAttachmentDataAsset.h"
#include "Engine/AssetManager.h"
#include "AssetRegistry/AssetData.h"
#include "Kismet/GameplayStatics.h"

const FString UShooterProgressionSubsystem::SlotName = TEXT("ShooterProfile");

namespace
{
	/** Armas que se desbloquean al alcanzar cada nivel (RF-9.3). */
	const TMap<int32, FName>& GetUnlockTable()
	{
		static const TMap<int32, FName> UnlockTable = {
			{ 1,  TEXT("Rifle") },
			{ 3,  TEXT("Pistola") },
			{ 6,  TEXT("Escopeta") },
			{ 10, TEXT("Subfusil") },
			{ 15, TEXT("Francotirador") },
			{ 20, TEXT("Lanzagranadas") },
		};
		return UnlockTable;
	}
}

void UShooterProgressionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Load();
}

void UShooterProgressionSubsystem::Deinitialize()
{
	Save();

	Super::Deinitialize();
}

void UShooterProgressionSubsystem::Load()
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		SaveData = Cast<UShooterSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	}

	// Primer arranque, o partida guardada corrupta: empezar de cero (RF-1.2).
	if (!SaveData)
	{
		SaveData = Cast<UShooterSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UShooterSaveGame::StaticClass()));
	}

	RefreshUnlocks();
}

void UShooterProgressionSubsystem::Save()
{
	if (SaveData)
	{
		UGameplayStatics::SaveGameToSlot(SaveData, SlotName, 0);
	}
}

int32 UShooterProgressionSubsystem::GetLevel() const
{
	return SaveData ? SaveData->Level : 1;
}

int32 UShooterProgressionSubsystem::GetExperience() const
{
	return SaveData ? SaveData->ExperiencePoints : 0;
}

int32 UShooterProgressionSubsystem::GetExperienceForLevel(int32 Level) const
{
	// Cada nivel cuesta un 15 % más que el anterior: la curva se abre sin
	// volverse inalcanzable.
	if (Level <= 1)
	{
		return 0;
	}

	float Total = 0.f;
	float LevelCost = static_cast<float>(BaseExperiencePerLevel);

	for (int32 Index = 1; Index < Level; ++Index)
	{
		Total += LevelCost;
		LevelCost *= 1.15f;
	}

	return FMath::RoundToInt(Total);
}

void UShooterProgressionSubsystem::GetLevelProgress(int32& OutCurrent, int32& OutRequired) const
{
	const int32 Level = GetLevel();
	const int32 Floor = GetExperienceForLevel(Level);
	const int32 Ceiling = GetExperienceForLevel(Level + 1);

	OutCurrent = GetExperience() - Floor;
	OutRequired = FMath::Max(Ceiling - Floor, 1);
}

void UShooterProgressionSubsystem::AddExperience(int32 Amount)
{
	if (!SaveData || Amount <= 0)
	{
		return;
	}

	SaveData->ExperiencePoints += Amount;
	OnExperienceGained.Broadcast(Amount, SaveData->ExperiencePoints);

	// Un solo golpe de XP puede cruzar varios niveles.
	while (SaveData->ExperiencePoints >= GetExperienceForLevel(SaveData->Level + 1))
	{
		++SaveData->Level;
		RefreshUnlocks();
		OnLevelUp.Broadcast(SaveData->Level);
	}
}

void UShooterProgressionSubsystem::RecordMatchResult(int32 Kills, int32 Deaths, int32 Assists)
{
	if (!SaveData)
	{
		return;
	}

	SaveData->TotalKills += FMath::Max(Kills, 0);
	SaveData->TotalDeaths += FMath::Max(Deaths, 0);
	++SaveData->MatchesPlayed;

	AddExperience(
		Kills * ExperiencePerKill +
		Assists * ExperiencePerAssist +
		ExperiencePerMatch);

	Save();
}

void UShooterProgressionSubsystem::RecordShot(bool bHit)
{
	if (!SaveData)
	{
		return;
	}

	++SaveData->ShotsFired;
	if (bHit)
	{
		++SaveData->ShotsHit;
	}
}

float UShooterProgressionSubsystem::GetAccuracy() const
{
	if (!SaveData || SaveData->ShotsFired <= 0)
	{
		return 0.f;
	}

	return static_cast<float>(SaveData->ShotsHit) / SaveData->ShotsFired;
}

float UShooterProgressionSubsystem::GetKillDeathRatio() const
{
	if (!SaveData)
	{
		return 0.f;
	}

	// Sin muertes, el ratio es el número de bajas: dividir por cero no sirve.
	return SaveData->TotalDeaths > 0
		? static_cast<float>(SaveData->TotalKills) / SaveData->TotalDeaths
		: static_cast<float>(SaveData->TotalKills);
}

bool UShooterProgressionSubsystem::IsWeaponUnlocked(FName WeaponId) const
{
	return SaveData && SaveData->UnlockedWeapons.Contains(WeaponId);
}

void UShooterProgressionSubsystem::UnlockWeapon(FName WeaponId)
{
	if (SaveData && !SaveData->UnlockedWeapons.Contains(WeaponId))
	{
		SaveData->UnlockedWeapons.Add(WeaponId);
	}
}

void UShooterProgressionSubsystem::RecordWeaponShot(FName WeaponId)
{
	if (!SaveData || WeaponId.IsNone())
	{
		return;
	}

	int32& Count = SaveData->WeaponShotCounts.FindOrAdd(WeaponId);
	++Count;

	// Recorrer el registro de assets en cada bala sería caro. Los umbrales de
	// desbloqueo son múltiplos redondos, así que basta comprobar de vez en
	// cuando: retrasa un desbloqueo unos disparos, nunca lo pierde.
	constexpr int32 UnlockCheckInterval = 25;
	if (Count % UnlockCheckInterval == 0)
	{
		RefreshAttachmentUnlocks(WeaponId, Count);
	}
}

void UShooterProgressionSubsystem::RefreshAttachmentUnlocks(FName WeaponId, int32 ShotCount)
{
	// Los accesorios se buscan por el AssetManager en vez de una tabla fija:
	// añadir uno nuevo es crear el asset, sin tocar código (RNF-9.3).
	const UAssetManager* Manager = UAssetManager::GetIfInitialized();
	if (!Manager)
	{
		return;
	}

	TArray<FAssetData> Assets;
	Manager->GetAssetRegistry().GetAssetsByClass(
		UWeaponAttachmentDataAsset::StaticClass()->GetClassPathName(), Assets);

	for (const FAssetData& Asset : Assets)
	{
		const FName AssetName = Asset.AssetName;
		if (SaveData->UnlockedAttachments.Contains(AssetName))
		{
			continue;
		}

		// Cargar solo los que aún faltan: los ya desbloqueados no se tocan.
		const UWeaponAttachmentDataAsset* Attachment =
			Cast<UWeaponAttachmentDataAsset>(Asset.GetAsset());

		if (Attachment && Attachment->ShotsToUnlock > 0
			&& ShotCount >= Attachment->ShotsToUnlock)
		{
			SaveData->UnlockedAttachments.Add(AssetName);
		}
	}
}

int32 UShooterProgressionSubsystem::GetWeaponShotCount(FName WeaponId) const
{
	if (!SaveData)
	{
		return 0;
	}

	const int32* Found = SaveData->WeaponShotCounts.Find(WeaponId);
	return Found ? *Found : 0;
}

bool UShooterProgressionSubsystem::IsAttachmentUnlocked(FName AttachmentId) const
{
	return SaveData && SaveData->UnlockedAttachments.Contains(AttachmentId);
}

void UShooterProgressionSubsystem::UnlockAttachment(FName AttachmentId)
{
	if (SaveData && !SaveData->UnlockedAttachments.Contains(AttachmentId))
	{
		SaveData->UnlockedAttachments.Add(AttachmentId);
	}
}

void UShooterProgressionSubsystem::RefreshUnlocks()
{
	if (!SaveData)
	{
		return;
	}

	for (const TPair<int32, FName>& Entry : GetUnlockTable())
	{
		if (SaveData->Level >= Entry.Key)
		{
			UnlockWeapon(Entry.Value);
		}
	}
}

FString UShooterProgressionSubsystem::GetPlayerName() const
{
	return SaveData ? SaveData->PlayerName : TEXT("Jugador");
}

void UShooterProgressionSubsystem::SetPlayerName(const FString& NewName)
{
	if (!SaveData || NewName.IsEmpty())
	{
		return;
	}

	SaveData->PlayerName = NewName.Left(24);
	Save();
}
