#include "Telemetry/ShooterTelemetrySubsystem.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/PlatformFileManager.h"

void UShooterTelemetrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Telemetry"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	// Un archivo por día: así se pueden rotar o subir por lotes.
	LogFilePath = FPaths::Combine(Directory,
		FString::Printf(TEXT("events_%s.jsonl"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d"))));

	WriteEvent(TEXT("session_start"), {});
}

void UShooterTelemetrySubsystem::Deinitialize()
{
	WriteEvent(TEXT("session_end"), {});

	Super::Deinitialize();
}

void UShooterTelemetrySubsystem::WriteEvent(const FString& EventType, const TMap<FString, FString>& Fields)
{
	if (!bEnabled || LogFilePath.IsEmpty())
	{
		return;
	}

	// JSON Lines: un objeto por línea, apto para ingestión por lotes.
	FString Line = FString::Printf(
		TEXT("{\"ts\":\"%s\",\"session\":\"%s\",\"event\":\"%s\""),
		*FDateTime::UtcNow().ToIso8601(), *SessionId, *EventType);

	for (const TPair<FString, FString>& Field : Fields)
	{
		// Las comillas del valor romperían el JSON.
		FString Value = Field.Value;
		Value.ReplaceInline(TEXT("\""), TEXT("'"));

		Line += FString::Printf(TEXT(",\"%s\":\"%s\""), *Field.Key, *Value);
	}

	Line += TEXT("}\n");

	FFileHelper::SaveStringToFile(Line, *LogFilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(), EFileWrite::FILEWRITE_Append);
}

void UShooterTelemetrySubsystem::RecordMatchStart(const FString& MapName, const FString& ModeName)
{
	WriteEvent(TEXT("match_start"), {
		{ TEXT("map"), MapName },
		{ TEXT("mode"), ModeName },
	});
}

void UShooterTelemetrySubsystem::RecordMatchEnd(float DurationSeconds, int32 Kills, int32 Deaths)
{
	WriteEvent(TEXT("match_end"), {
		{ TEXT("duration"), FString::Printf(TEXT("%.1f"), DurationSeconds) },
		{ TEXT("kills"), FString::FromInt(Kills) },
		{ TEXT("deaths"), FString::FromInt(Deaths) },
	});
}

void UShooterTelemetrySubsystem::RecordKill(const FString& WeaponName, float Distance, bool bHeadshot)
{
	WriteEvent(TEXT("kill"), {
		{ TEXT("weapon"), WeaponName },
		{ TEXT("distance"), FString::Printf(TEXT("%.0f"), Distance) },
		{ TEXT("headshot"), bHeadshot ? TEXT("true") : TEXT("false") },
	});
}

void UShooterTelemetrySubsystem::RecordWeaponUsage(const FString& WeaponName, int32 ShotsFired, int32 ShotsHit)
{
	WriteEvent(TEXT("weapon_usage"), {
		{ TEXT("weapon"), WeaponName },
		{ TEXT("fired"), FString::FromInt(ShotsFired) },
		{ TEXT("hit"), FString::FromInt(ShotsHit) },
	});
}

void UShooterTelemetrySubsystem::RecordError(const FString& Context, const FString& Message)
{
	WriteEvent(TEXT("error"), {
		{ TEXT("context"), Context },
		{ TEXT("message"), Message },
	});
}
