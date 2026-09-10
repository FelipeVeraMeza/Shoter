#include "ShooterGameMode.h"
#include "ShooterCharacter.h"

AShooterGameMode::AShooterGameMode()
{
	DefaultPawnClass = AShooterCharacter::StaticClass();
}
