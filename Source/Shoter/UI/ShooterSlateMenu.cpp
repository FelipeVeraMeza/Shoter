#include "UI/ShooterSlateMenu.h"
#include "UI/ShooterMenuSubsystem.h"
#include "Settings/ShooterSettingsSubsystem.h"
#include "Progression/ShooterProgressionSubsystem.h"
#include "Core/ShooterGameState.h"
#include "Core/ShooterPlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "Weapons/WeaponBase.h"
#include "Weapons/WeaponDataAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"

#define LOCTEXT_NAMESPACE "ShooterMenu"

namespace
{
	// Paleta: verde oliva oscuro y ámbar, la de un equipo militar.
	const FLinearColor PanelColor(0.025f, 0.03f, 0.025f, 0.9f);
	const FLinearColor ShadeColor(0.f, 0.f, 0.f, 0.35f);
	const FLinearColor PauseShadeColor(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor ButtonColor(0.12f, 0.13f, 0.11f, 0.95f);
	const FLinearColor AccentColor(0.86f, 0.6f, 0.18f, 1.f);
	const FLinearColor TextColor(0.93f, 0.92f, 0.88f, 1.f);
	const FLinearColor MutedColor(0.6f, 0.6f, 0.55f, 1.f);

	const FSlateBrush* WhiteBrush()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	FSlateFontInfo Font(const char* Style, int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}
}

void SShooterMenuScreen::Construct(const FArguments& InArgs)
{
	Menu = InArgs._Menu;
	Screen = InArgs._Screen;

	if (const UShooterSettingsSubsystem* Settings = GetSettings())
	{
		PendingSensitivity = Settings->GetMouseSensitivity();
		PendingVolume = Settings->GetMasterVolume();
	}

	TSharedRef<SWidget> Content = SNullWidget::NullWidget;
	switch (Screen)
	{
	case EShooterMenuScreen::Play:			Content = BuildPlay(); break;
	case EShooterMenuScreen::Options:		Content = BuildOptions(); break;
	case EShooterMenuScreen::Controls:		Content = BuildControls(); break;
	case EShooterMenuScreen::Loadout:		Content = BuildLoadout(); break;
	case EShooterMenuScreen::Arsenal:		Content = BuildLoadout(); break;
	case EShooterMenuScreen::Progression:	Content = BuildProgression(); break;
	case EShooterMenuScreen::Pause:			Content = BuildPause(); break;
	default:								Content = BuildMain(); break;
	}

	ChildSlot
	[
		Content
	];
}

FReply SShooterMenuScreen::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Escape retrocede una pantalla; en la pausa la cierra. Se consume aquí
	// para que la acción de pausa del juego no la reabra en el mismo pulso.
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		if (UShooterMenuSubsystem* MenuSubsystem = GetMenu())
		{
			if (Screen == EShooterMenuScreen::Main && !IsInMatch())
			{
				return FReply::Handled();
			}

			if (Screen == EShooterMenuScreen::Options)
			{
				if (UShooterSettingsSubsystem* Settings = GetSettings())
				{
					Settings->ApplyAndSave();
				}
			}
			MenuSubsystem->GoBack();
		}
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

UShooterSettingsSubsystem* SShooterMenuScreen::GetSettings() const
{
	const UShooterMenuSubsystem* MenuSubsystem = GetMenu();
	const UGameInstance* GameInstance = MenuSubsystem ? MenuSubsystem->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UShooterSettingsSubsystem>() : nullptr;
}

APlayerController* SShooterMenuScreen::GetPlayerController() const
{
	const UShooterMenuSubsystem* MenuSubsystem = GetMenu();
	const UGameInstance* GameInstance = MenuSubsystem ? MenuSubsystem->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
}

bool SShooterMenuScreen::IsInMatch() const
{
	const APlayerController* Controller = GetPlayerController();
	const UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	return World && World->GetGameState<AShooterGameState>() != nullptr;
}

// --- Piezas -------------------------------------------------------------------

TSharedRef<SWidget> SShooterMenuScreen::MakeFrame(const FText& Title, const FText& Subtitle, TSharedRef<SWidget> Content)
{
	// En partida el fondo oscurece todo para que el menú se lea sobre la
	// acción; en el menú principal deja ver la escena de fondo a la derecha.
	const FLinearColor Shade = IsInMatch() ? PauseShadeColor : ShadeColor;

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(Shade)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Fill)
		[
			SNew(SBox)
			.WidthOverride(620.f)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(PanelColor)
				.Padding(FMargin(64.f, 72.f, 48.f, 48.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(AccentColor)
						.Padding(FMargin(28.f, 2.f))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 18.f, 0.f, 4.f)
					[
						SNew(STextBlock)
						.Text(Title)
						.Font(Font("Bold", 40))
						.ColorAndOpacity(TextColor)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 28.f)
					[
						SNew(STextBlock)
						.Text(Subtitle)
						.Font(Font("Regular", 14))
						.ColorAndOpacity(MutedColor)
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							Content
						]
					]
				]
			]
		];
}

TSharedRef<SWidget> SShooterMenuScreen::MakeButton(const FText& Label, TFunction<void()> OnClicked, bool bPrimary)
{
	return SNew(SBox)
		.Padding(FMargin(0.f, 4.f))
		[
			SNew(SButton)
			.ButtonColorAndOpacity(bPrimary ? AccentColor : ButtonColor)
			.ContentPadding(FMargin(20.f, 12.f))
			.OnClicked_Lambda([OnClicked]()
			{
				OnClicked();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(Font("Bold", 17))
				.ColorAndOpacity(bPrimary ? FLinearColor(0.05f, 0.04f, 0.02f, 1.f) : TextColor)
			]
		];
}

TSharedRef<SWidget> SShooterMenuScreen::MakeToggleButton(const FText& Label, TFunction<bool()> IsSelected, TFunction<void()> OnClicked)
{
	return SNew(SBox)
		.Padding(FMargin(0.f, 3.f, 6.f, 3.f))
		[
			SNew(SButton)
			.ButtonColorAndOpacity_Lambda([IsSelected]()
			{
				return IsSelected() ? FSlateColor(AccentColor) : FSlateColor(ButtonColor);
			})
			.ContentPadding(FMargin(14.f, 9.f))
			.HAlign(HAlign_Center)
			.OnClicked_Lambda([OnClicked]()
			{
				OnClicked();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(Font("Bold", 14))
				.ColorAndOpacity_Lambda([IsSelected]()
				{
					return IsSelected() ? FSlateColor(FLinearColor(0.05f, 0.04f, 0.02f, 1.f)) : FSlateColor(TextColor);
				})
			]
		];
}

TSharedRef<SWidget> SShooterMenuScreen::MakeSectionLabel(const FText& Label)
{
	return SNew(SBox)
		.Padding(FMargin(0.f, 18.f, 0.f, 6.f))
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(Font("Bold", 12))
			.ColorAndOpacity(AccentColor)
		];
}

TSharedRef<SWidget> SShooterMenuScreen::MakeCheckRow(const FText& Label, TFunction<bool()> Get, TFunction<void(bool)> Set)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 6.f, 12.f, 6.f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([Get]() { return Get() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Set](ECheckBoxState State) { Set(State == ECheckBoxState::Checked); })
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(Font("Regular", 15))
			.ColorAndOpacity(TextColor)
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SShooterMenuScreen::MakeKeyRow(const FText& Action, const FText& Keys)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(0.55f)
		.Padding(0.f, 4.f)
		[
			SNew(STextBlock)
			.Text(Action)
			.Font(Font("Regular", 14))
			.ColorAndOpacity(TextColor)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.45f)
		.Padding(0.f, 4.f)
		[
			SNew(STextBlock)
			.Text(Keys)
			.Font(Font("Bold", 14))
			.ColorAndOpacity(AccentColor)
		];
}

// --- Pantallas ----------------------------------------------------------------

TSharedRef<SWidget> SShooterMenuScreen::BuildMain()
{
	TWeakObjectPtr<UShooterMenuSubsystem> WeakMenu = Menu;

	return MakeFrame(
		LOCTEXT("GameTitle", "SHOTER"),
		LOCTEXT("GameSubtitle", "Combate táctico urbano. Sin mira en pantalla, sin regeneración: cada bala cuenta."),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LOCTEXT("Play", "JUGAR"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->OpenScreen(EShooterMenuScreen::Play); } }, true)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LOCTEXT("Options", "OPCIONES"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->OpenScreen(EShooterMenuScreen::Options); } })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LOCTEXT("Controls", "CONTROLES"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->OpenScreen(EShooterMenuScreen::Controls); } })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LOCTEXT("Progression", "PROGRESIÓN"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->OpenScreen(EShooterMenuScreen::Progression); } })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 24.f, 0.f, 0.f)
		[
			MakeButton(LOCTEXT("Quit", "SALIR DEL JUEGO"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->QuitGame(); } })
		]);
}

TSharedRef<SWidget> SShooterMenuScreen::BuildPlay()
{
	TWeakObjectPtr<UShooterMenuSubsystem> WeakMenu = Menu;

	const auto ModeButton = [this](const FText& Label, const TCHAR* Mode)
	{
		const FString ModeString(Mode);
		return MakeToggleButton(Label,
			[this, ModeString]() { return SelectedMode == ModeString; },
			[this, ModeString]() { SelectedMode = ModeString; });
	};

	const auto CountButton = [WeakMenu, this](int32 Count)
	{
		return MakeToggleButton(FText::AsNumber(Count),
			[WeakMenu, Count]() { return WeakMenu.IsValid() && WeakMenu->MatchPlayerCount == Count; },
			[WeakMenu, Count]() { if (WeakMenu.IsValid()) { WeakMenu->MatchPlayerCount = Count; } });
	};

	return MakeFrame(
		LOCTEXT("PlayTitle", "DESPLIEGUE"),
		LOCTEXT("PlaySubtitle", "Distrito: barrio en ruinas de 160 × 120 m. Callejón al norte, avenida con plaza en el centro y patio industrial al sur."),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeSectionLabel(LOCTEXT("ModeSection", "MODO"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SUniformGridPanel)
			+ SUniformGridPanel::Slot(0, 0) [ ModeButton(LOCTEXT("ModeTeams", "EQUIPOS"), TEXT("equipos")) ]
			+ SUniformGridPanel::Slot(1, 0) [ ModeButton(LOCTEXT("ModeDomination", "DOMINACIÓN"), TEXT("dominacion")) ]
			+ SUniformGridPanel::Slot(2, 0) [ ModeButton(LOCTEXT("ModeFFA", "TODOS CONTRA TODOS"), TEXT("todos")) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Font(Font("Regular", 13))
			.ColorAndOpacity(MutedColor)
			.AutoWrapText(true)
			.Text_Lambda([this]()
			{
				if (SelectedMode == TEXT("dominacion"))
				{
					return LOCTEXT("ModeDominationHelp", "Tres zonas: A en el callejón, B en la plaza, C en la nave. Mantenerlas suma puntos.");
				}
				if (SelectedMode == TEXT("todos"))
				{
					return LOCTEXT("ModeFFAHelp", "Sin equipos: todos son enemigos. Gana quien más bajas consiga.");
				}
				return LOCTEXT("ModeTeamsHelp", "Alfa contra Bravo. Cada baja suma un punto al equipo; gana el primero en llegar a 60 o el que vaya por delante a los 15 minutos.");
			})
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeSectionLabel(LOCTEXT("PlayersSection", "JUGADORES (LOS HUECOS LOS OCUPAN BOTS)"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SUniformGridPanel)
			+ SUniformGridPanel::Slot(0, 0) [ CountButton(4) ]
			+ SUniformGridPanel::Slot(1, 0) [ CountButton(8) ]
			+ SUniformGridPanel::Slot(2, 0) [ CountButton(12) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 28.f, 0.f, 0.f)
		[
			MakeButton(LOCTEXT("Deploy", "DESPLEGAR EN DISTRITO"), [WeakMenu, this]()
			{
				if (WeakMenu.IsValid())
				{
					WeakMenu->StartMatchWithOptions(TEXT("/Game/Maps/Lvl_Distrito"),
						FString::Printf(TEXT("modo=%s?jugadores=%d"), *SelectedMode, WeakMenu->MatchPlayerCount));
				}
			}, true)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LOCTEXT("Training", "ZONA DE ENTRENAMIENTO"), [WeakMenu]()
			{
				if (WeakMenu.IsValid())
				{
					WeakMenu->StartMatch(TEXT("/Game/Maps/Lvl_Entrenamiento"));
				}
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
		[
			MakeButton(LOCTEXT("Back", "VOLVER"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->GoBack(); } })
		]);
}

TSharedRef<SWidget> SShooterMenuScreen::BuildOptions()
{
	TWeakObjectPtr<UShooterMenuSubsystem> WeakMenu = Menu;
	const TWeakObjectPtr<UShooterSettingsSubsystem> WeakSettings = GetSettings();

	const auto QualityButton = [this, WeakSettings](const FText& Label, int32 Preset)
	{
		return MakeToggleButton(Label,
			[WeakSettings, Preset]() { return WeakSettings.IsValid() && WeakSettings->GetQualityPreset() == Preset; },
			[WeakSettings, Preset]() { if (WeakSettings.IsValid()) { WeakSettings->SetQualityPreset(Preset); } });
	};

	const auto WindowButton = [this, WeakSettings](const FText& Label, EShooterWindowMode Mode)
	{
		return MakeToggleButton(Label,
			[WeakSettings, Mode]() { return WeakSettings.IsValid() && WeakSettings->GetWindowMode() == Mode; },
			[WeakSettings, Mode]() { if (WeakSettings.IsValid()) { WeakSettings->SetWindowMode(Mode); } });
	};

	const auto Commit = [WeakSettings]()
	{
		if (WeakSettings.IsValid())
		{
			WeakSettings->ApplyAndSave();
		}
	};

	return MakeFrame(
		LOCTEXT("OptionsTitle", "OPCIONES"),
		LOCTEXT("OptionsSubtitle", "Los cambios se guardan en tu perfil y se mantienen entre sesiones."),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeSectionLabel(LOCTEXT("ControlSection", "CONTROL"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("Sensitivity", "Sensibilidad del ratón")).Font(Font("Regular", 15)).ColorAndOpacity(TextColor)
			]
			+ SHorizontalBox::Slot().FillWidth(0.43f).VAlign(VAlign_Center)
			[
				SNew(SSlider)
				.MinValue(0.1f)
				.MaxValue(3.f)
				.Value_Lambda([this]() { return PendingSensitivity; })
				.OnValueChanged_Lambda([this](float Value) { PendingSensitivity = Value; })
				.OnMouseCaptureEnd_Lambda([this, WeakSettings]() { if (WeakSettings.IsValid()) { WeakSettings->SetMouseSensitivity(PendingSensitivity); } })
				.OnControllerCaptureEnd_Lambda([this, WeakSettings]() { if (WeakSettings.IsValid()) { WeakSettings->SetMouseSensitivity(PendingSensitivity); } })
			]
			+ SHorizontalBox::Slot().FillWidth(0.15f).VAlign(VAlign_Center).HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Font(Font("Bold", 14))
				.ColorAndOpacity(AccentColor)
				.Text_Lambda([this]() { return FText::AsNumber(FMath::RoundToFloat(PendingSensitivity * 100.f) / 100.f); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeCheckRow(LOCTEXT("InvertY", "Invertir eje vertical"),
				[WeakSettings]() { return WeakSettings.IsValid() && WeakSettings->GetInvertY(); },
				[WeakSettings](bool bValue) { if (WeakSettings.IsValid()) { WeakSettings->SetInvertY(bValue); } })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeSectionLabel(LOCTEXT("InterfaceSection", "INTERFAZ"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeCheckRow(LOCTEXT("RealisticHud", "HUD realista: sin mira, sin minimapa y munición solo al comprobar el cargador"),
				[WeakSettings]() { return WeakSettings.IsValid() && WeakSettings->GetRealisticHUD(); },
				[WeakSettings](bool bValue) { if (WeakSettings.IsValid()) { WeakSettings->SetRealisticHUD(bValue); } })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeSectionLabel(LOCTEXT("AudioSection", "AUDIO"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("MasterVolume", "Volumen general")).Font(Font("Regular", 15)).ColorAndOpacity(TextColor)
			]
			+ SHorizontalBox::Slot().FillWidth(0.43f).VAlign(VAlign_Center)
			[
				SNew(SSlider)
				.Value_Lambda([this]() { return PendingVolume; })
				.OnValueChanged_Lambda([this](float Value) { PendingVolume = Value; })
				.OnMouseCaptureEnd_Lambda([this, WeakSettings]() { if (WeakSettings.IsValid()) { WeakSettings->SetMasterVolume(PendingVolume); } })
				.OnControllerCaptureEnd_Lambda([this, WeakSettings]() { if (WeakSettings.IsValid()) { WeakSettings->SetMasterVolume(PendingVolume); } })
			]
			+ SHorizontalBox::Slot().FillWidth(0.15f).VAlign(VAlign_Center).HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Font(Font("Bold", 14))
				.ColorAndOpacity(AccentColor)
				.Text_Lambda([this]() { return FText::AsPercent(PendingVolume); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeSectionLabel(LOCTEXT("VideoSection", "VÍDEO"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SUniformGridPanel)
			+ SUniformGridPanel::Slot(0, 0) [ QualityButton(LOCTEXT("QualityLow", "BAJA"), 0) ]
			+ SUniformGridPanel::Slot(1, 0) [ QualityButton(LOCTEXT("QualityMedium", "MEDIA"), 1) ]
			+ SUniformGridPanel::Slot(2, 0) [ QualityButton(LOCTEXT("QualityHigh", "ALTA"), 2) ]
			+ SUniformGridPanel::Slot(3, 0) [ QualityButton(LOCTEXT("QualityEpic", "ÉPICA"), 3) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(SUniformGridPanel)
			+ SUniformGridPanel::Slot(0, 0) [ WindowButton(LOCTEXT("Fullscreen", "COMPLETA"), EShooterWindowMode::Fullscreen) ]
			+ SUniformGridPanel::Slot(1, 0) [ WindowButton(LOCTEXT("Borderless", "SIN BORDES"), EShooterWindowMode::WindowedFullscreen) ]
			+ SUniformGridPanel::Slot(2, 0) [ WindowButton(LOCTEXT("Windowed", "VENTANA"), EShooterWindowMode::Windowed) ]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeCheckRow(LOCTEXT("VSync", "Sincronización vertical"),
				[WeakSettings]() { return WeakSettings.IsValid() && WeakSettings->GetVSyncEnabled(); },
				[WeakSettings](bool bValue) { if (WeakSettings.IsValid()) { WeakSettings->SetVSyncEnabled(bValue); } })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 24.f, 0.f, 0.f)
		[
			MakeButton(LOCTEXT("ApplySave", "APLICAR Y GUARDAR"), Commit, true)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LOCTEXT("Back", "VOLVER"), [WeakMenu, Commit]()
			{
				Commit();
				if (WeakMenu.IsValid())
				{
					WeakMenu->GoBack();
				}
			})
		]);
}

TSharedRef<SWidget> SShooterMenuScreen::BuildControls()
{
	TWeakObjectPtr<UShooterMenuSubsystem> WeakMenu = Menu;

	// Las teclas reflejan IMC_Combate tal como lo genera build_realistic_map.py.
	return MakeFrame(
		LOCTEXT("ControlsTitle", "CONTROLES"),
		LOCTEXT("ControlsSubtitle", "Teclado y ratón. Con el mando: gatillos para apuntar y disparar, cruceta para asomarse, selector y vendas."),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeSectionLabel(LOCTEXT("MovementSection", "MOVIMIENTO")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyMove", "Moverse"), LOCTEXT("KeyMoveKeys", "W A S D")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeySprint", "Correr"), LOCTEXT("KeySprintKeys", "Mayús")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyCrouch", "Agacharse"), LOCTEXT("KeyCrouchKeys", "Ctrl / C")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyJump", "Saltar"), LOCTEXT("KeyJumpKeys", "Espacio")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyLean", "Asomarse izquierda / derecha"), LOCTEXT("KeyLeanKeys", "Q / E (mantener)")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeSectionLabel(LOCTEXT("CombatSection", "COMBATE")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyFire", "Disparar"), LOCTEXT("KeyFireKeys", "Clic izquierdo")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyAim", "Apuntar con la mira"), LOCTEXT("KeyAimKeys", "Clic derecho")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyBreath", "Contener la respiración"), LOCTEXT("KeyBreathKeys", "Mayús apuntando")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyReload", "Recargar / comprobar cargador"), LOCTEXT("KeyReloadKeys", "R")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyFireMode", "Selector de fuego"), LOCTEXT("KeyFireModeKeys", "B")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeySwitch", "Cambiar de arma"), LOCTEXT("KeySwitchKeys", "Rueda / 1")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyGrenade", "Granada"), LOCTEXT("KeyGrenadeKeys", "G")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyBandage", "Vendarse"), LOCTEXT("KeyBandageKeys", "H")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeSectionLabel(LOCTEXT("InterfaceKeysSection", "INTERFAZ")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyScore", "Tabla de posiciones"), LOCTEXT("KeyScoreKeys", "Tab (mantener)")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyLoadout", "Equipamiento"), LOCTEXT("KeyLoadoutKeys", "L")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyHelp", "Ayuda de controles"), LOCTEXT("KeyHelpKeys", "F1")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeKeyRow(LOCTEXT("KeyPause", "Pausa"), LOCTEXT("KeyPauseKeys", "Esc / P")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 24.f, 0.f, 0.f)
		[
			MakeButton(LOCTEXT("Back", "VOLVER"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->GoBack(); } })
		]);
}

TSharedRef<SWidget> SShooterMenuScreen::BuildLoadout()
{
	TWeakObjectPtr<UShooterMenuSubsystem> WeakMenu = Menu;
	TWeakObjectPtr<APlayerController> WeakController = GetPlayerController();

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);

	const UWorld* World = WeakController.IsValid() ? WeakController->GetWorld() : nullptr;
	const AShooterGameState* ShooterGameState = World ? World->GetGameState<AShooterGameState>() : nullptr;
	const TArray<TSubclassOf<AWeaponBase>> Options = ShooterGameState
		? ShooterGameState->GetPrimaryWeaponOptions() : TArray<TSubclassOf<AWeaponBase>>();

	List->AddSlot().AutoHeight()[ MakeSectionLabel(LOCTEXT("PrimarySection", "ARMA PRINCIPAL")) ];

	if (Options.Num() == 0)
	{
		List->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoLoadout", "El equipamiento se elige durante una partida."))
			.Font(Font("Regular", 15))
			.ColorAndOpacity(MutedColor)
		];
	}

	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		UClass* WeaponClass = Options[Index].Get();
		const AWeaponBase* Defaults = WeaponClass ? WeaponClass->GetDefaultObject<AWeaponBase>() : nullptr;
		const UWeaponDataAsset* Data = Defaults ? Defaults->WeaponData.Get() : nullptr;
		if (!Data)
		{
			continue;
		}

		const TWeakObjectPtr<UClass> WeakClass = WeaponClass;
		const bool bIsDefault = Index == 0;

		const auto IsSelected = [this, WeakController, WeakClass, bIsDefault]()
		{
			if (LocalPrimaryChoice.IsValid())
			{
				return LocalPrimaryChoice == WeakClass;
			}
			const AShooterPlayerState* ShooterState = WeakController.IsValid()
				? WeakController->GetPlayerState<AShooterPlayerState>() : nullptr;
			const UClass* Chosen = ShooterState ? ShooterState->GetPreferredPrimary().Get() : nullptr;
			return Chosen ? Chosen == WeakClass.Get() : bIsDefault;
		};

		FFormatNamedArguments Args;
		Args.Add(TEXT("Damage"), FText::AsNumber(FMath::RoundToInt(Data->Damage)));
		Args.Add(TEXT("Rate"), FText::AsNumber(FMath::RoundToInt(Data->RoundsPerMinute)));
		Args.Add(TEXT("Magazine"), FText::AsNumber(Data->MagazineSize));
		Args.Add(TEXT("Velocity"), FText::AsNumber(FMath::RoundToInt(Data->MuzzleVelocity / 100.f)));
		const FText Stats = FText::Format(
			LOCTEXT("WeaponStats", "Daño {Damage} · {Rate} disparos/min · cargador de {Magazine} · {Velocity} m/s"), Args);

		List->AddSlot().AutoHeight()
		[
			MakeToggleButton(Data->DisplayName, IsSelected, [this, WeakController, WeakClass]()
			{
				LocalPrimaryChoice = WeakClass;
				if (AShooterPlayerController* ShooterController = Cast<AShooterPlayerController>(WeakController.Get()))
				{
					ShooterController->RequestPrimaryWeapon(WeakClass.Get());
				}
			})
		];
		List->AddSlot().AutoHeight().Padding(4.f, 0.f, 0.f, 10.f)
		[
			SNew(STextBlock)
			.Text(Stats)
			.Font(Font("Regular", 12))
			.ColorAndOpacity(MutedColor)
		];
	}

	List->AddSlot().AutoHeight()[ MakeSectionLabel(LOCTEXT("KitSection", "EQUIPO FIJO")) ];
	List->AddSlot().AutoHeight()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("KitDescription", "Pistola 9 mm como secundaria, dos granadas de fragmentación y dos vendas."))
		.Font(Font("Regular", 14))
		.ColorAndOpacity(TextColor)
		.AutoWrapText(true)
	];
	List->AddSlot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("LoadoutApplies", "El cambio se aplica al reaparecer."))
		.Font(Font("Italic", 13))
		.ColorAndOpacity(AccentColor)
	];
	List->AddSlot().AutoHeight().Padding(0.f, 24.f, 0.f, 0.f)
	[
		MakeButton(LOCTEXT("Back", "VOLVER"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->GoBack(); } })
	];

	return MakeFrame(
		LOCTEXT("LoadoutTitle", "EQUIPAMIENTO"),
		LOCTEXT("LoadoutSubtitle", "El fusil de asalto domina a media distancia; el de tirador, con visor de aumentos, mata de dos impactos y atraviesa más."),
		List);
}

TSharedRef<SWidget> SShooterMenuScreen::BuildProgression()
{
	TWeakObjectPtr<UShooterMenuSubsystem> WeakMenu = Menu;

	const UShooterMenuSubsystem* MenuSubsystem = GetMenu();
	const UGameInstance* GameInstance = MenuSubsystem ? MenuSubsystem->GetGameInstance() : nullptr;
	const UShooterProgressionSubsystem* Progression = GameInstance
		? GameInstance->GetSubsystem<UShooterProgressionSubsystem>() : nullptr;

	FText Summary = LOCTEXT("NoProgress", "Sin datos de progresión todavía.");
	if (Progression)
	{
		int32 Current = 0;
		int32 Required = 0;
		Progression->GetLevelProgress(Current, Required);

		FFormatNamedArguments Args;
		Args.Add(TEXT("Level"), FText::AsNumber(Progression->GetLevel()));
		Args.Add(TEXT("Current"), FText::AsNumber(Current));
		Args.Add(TEXT("Required"), FText::AsNumber(Required));
		Args.Add(TEXT("KD"), FText::AsNumber(Progression->GetKillDeathRatio()));
		Args.Add(TEXT("Accuracy"), FText::AsPercent(Progression->GetAccuracy()));
		Summary = FText::Format(LOCTEXT("ProgressSummary",
			"Nivel {Level}\nExperiencia: {Current} / {Required}\nBajas por muerte: {KD}\nPrecisión: {Accuracy}"), Args);
	}

	return MakeFrame(
		LOCTEXT("ProgressionTitle", "PROGRESIÓN"),
		LOCTEXT("ProgressionSubtitle", "Se guarda en este equipo al terminar cada partida."),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(Summary)
			.Font(Font("Regular", 18))
			.ColorAndOpacity(TextColor)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 24.f, 0.f, 0.f)
		[
			MakeButton(LOCTEXT("Back", "VOLVER"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->GoBack(); } })
		]);
}

TSharedRef<SWidget> SShooterMenuScreen::BuildPause()
{
	TWeakObjectPtr<UShooterMenuSubsystem> WeakMenu = Menu;

	return MakeFrame(
		LOCTEXT("PauseTitle", "PAUSA"),
		LOCTEXT("PauseSubtitle", "La partida sigue en marcha: los demás no se detienen."),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LOCTEXT("Resume", "CONTINUAR"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->CloseMenu(); } }, true)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LOCTEXT("Loadout", "EQUIPAMIENTO"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->OpenScreen(EShooterMenuScreen::Loadout); } })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LOCTEXT("Options", "OPCIONES"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->OpenScreen(EShooterMenuScreen::Options); } })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LOCTEXT("Controls", "CONTROLES"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->OpenScreen(EShooterMenuScreen::Controls); } })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 24.f, 0.f, 0.f)
		[
			MakeButton(LOCTEXT("Leave", "ABANDONAR PARTIDA"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->LeaveMatch(); } })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeButton(LOCTEXT("Quit", "SALIR DEL JUEGO"), [WeakMenu]() { if (WeakMenu.IsValid()) { WeakMenu->QuitGame(); } })
		]);
}

#undef LOCTEXT_NAMESPACE
