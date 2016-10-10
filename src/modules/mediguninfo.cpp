/*
 *  mediguninfo.cpp
 *  StatusSpec project
 *  
 *  Copyright (c) 2014-2015 Forward Command Post
 *  BSD 2-Clause License
 *  http://opensource.org/licenses/BSD-2-Clause
 *
 */

#include "mediguninfo.h"

#include <vector>

#include "cbase.h"
#include "convar.h"
#include "iclientmode.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui_controls/EditablePanel.h"

#include "../common.h"
#include "../entities.h"
#include "../funcs.h"
#include "../ifaces.h"
#include "../player.h"

class MedigunInfo::MainPanel : public vgui::EditablePanel {
	DECLARE_CLASS_SIMPLE(MainPanel, vgui::EditablePanel);

public:
	MainPanel(vgui::Panel *parent, const char *panelName);

	virtual void ApplySettings(KeyValues *inResourceData);
	virtual void LoadControlSettings(const char *dialogResourceName, const char *pathID = NULL, KeyValues *pPreloadedKeyValues = NULL, KeyValues *pConditions = NULL);
	virtual void OnTick();

private:
	int bluBaseX;
	int bluBaseY;
	int bluOffsetX;
	int bluOffsetY;
	int redBaseX;
	int redBaseY;
	int redOffsetX;
	int redOffsetY;

	std::vector<MedigunPanel *> bluMedigunPanels;
	std::vector<MedigunPanel *> redMedigunPanels;
};

class MedigunInfo::MedigunPanel : public vgui::EditablePanel {
	DECLARE_CLASS_SIMPLE(MedigunPanel, vgui::EditablePanel);

public:
	MedigunPanel(vgui::Panel *parent, const char *panelName);

private:
	MESSAGE_FUNC_PARAMS(OnMedigunInfoUpdate, "MedigunInfo", attributes);
	MESSAGE_FUNC_PARAMS(OnReloadControlSettings, "ReloadControlSettings", data);

	bool alive;
	int charges;
	float level;
	TFMedigun medigun;
	bool released;
	TFResistType resistType;
	TFTeam team;
};

MedigunInfo::MedigunInfo() {
	mainPanel = nullptr;

	enabled = new ConVar("statusspec_mediguninfo_enabled", "0", FCVAR_NONE, "enable medigun info", [](IConVar *var, const char *pOldValue, float flOldValue) { g_ModuleManager->GetModule<MedigunInfo>()->ToggleEnabled(var, pOldValue, flOldValue); });
	reload_settings = new ConCommand("statusspec_mediguninfo_reload_settings", []() { g_ModuleManager->GetModule<MedigunInfo>()->ReloadSettings(); }, "reload settings for the medigun info HUD from the resource file", FCVAR_NONE);
}

bool MedigunInfo::CheckDependencies() {
	bool ready = true;

	if (!g_pVGuiSchemeManager) {
		PRINT_TAG();
		Warning("Required interface vgui::ISchemeManager for module %s not available!\n", g_ModuleManager->GetModuleName<MedigunInfo>().c_str());

		ready = false;
	}

	if (!g_pVGuiSurface) {
		PRINT_TAG();
		Warning("Required interface vgui::ISurface for module %s not available!\n", g_ModuleManager->GetModuleName<MedigunInfo>().c_str());

		ready = false;
	}

	if (!Interfaces::vguiLibrariesAvailable) {
		PRINT_TAG();
		Warning("Required VGUI library for module %s not available!\n", g_ModuleManager->GetModuleName<MedigunInfo>().c_str());

		ready = false;
	}

	if (!Entities::RetrieveClassPropOffset("CWeaponMedigun", { "m_iItemDefinitionIndex" })) {
		PRINT_TAG();
		Warning("Required property m_iItemDefinitionIndex for CWeaponMedigun for module %s not available!\n", g_ModuleManager->GetModuleName<MedigunInfo>().c_str());

		ready = false;
	}

	if (!Entities::RetrieveClassPropOffset("CWeaponMedigun", { "m_bChargeRelease" })) {
		PRINT_TAG();
		Warning("Required property m_bChargeRelease for CWeaponMedigun for module %s not available!\n", g_ModuleManager->GetModuleName<MedigunInfo>().c_str());

		ready = false;
	}

	if (!Entities::RetrieveClassPropOffset("CWeaponMedigun", { "m_nChargeResistType" })) {
		PRINT_TAG();
		Warning("Required property m_nChargeResistType for CWeaponMedigun for module %s not available!\n", g_ModuleManager->GetModuleName<MedigunInfo>().c_str());

		ready = false;
	}

	if (!Entities::RetrieveClassPropOffset("CWeaponMedigun", { "m_flChargeLevel" })) {
		PRINT_TAG();
		Warning("Required property m_flChargeLevel for CWeaponMedigun for module %s not available!\n", g_ModuleManager->GetModuleName<MedigunInfo>().c_str());

		ready = false;
	}

	if (!Player::CheckDependencies()) {
		PRINT_TAG();
		Warning("Required player helper class for module %s not available!\n", g_ModuleManager->GetModuleName<MedigunInfo>().c_str());

		ready = false;
	}

	try {
		Interfaces::GetClientMode();
	}
	catch (bad_pointer) {
		PRINT_TAG();
		Warning("Module %s requires IClientMode, which cannot be verified at this time!\n", g_ModuleManager->GetModuleName<MedigunInfo>().c_str());
	}

	return ready;
}

void MedigunInfo::ReloadSettings() {
	mainPanel->LoadControlSettings("Resource/UI/MedigunInfo.res");
}

void MedigunInfo::ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue) {
	if (enabled->GetBool()) {
		if (!mainPanel) {
			try {
				vgui::Panel *viewport = Interfaces::GetClientMode()->GetViewport();

				if (viewport) {
					mainPanel = new MainPanel(viewport, "MedigunInfo");
				}
				else {
					Warning("Could not initialize the panel!\n");
					mainPanel = nullptr;
					var->SetValue(0);
				}
			}
			catch (bad_pointer) {
				Warning("Could not initialize the panel!\n");
				mainPanel = nullptr;
				var->SetValue(0);
			}
		}

		if (mainPanel) {
			mainPanel->SetEnabled(true);
		}
	}
	else {
		if (mainPanel) {
			delete mainPanel;
			mainPanel = nullptr;
		}
	}
}

MedigunInfo::MainPanel::MainPanel(vgui::Panel *parent, const char *panelName) : vgui::EditablePanel(parent, panelName) {
	LoadControlSettings("Resource/UI/MedigunInfo.res");

	g_pVGui->AddTickSignal(GetVPanel());
}

void MedigunInfo::MainPanel::ApplySettings(KeyValues *inResourceData) {
	vgui::EditablePanel::ApplySettings(inResourceData);

	int alignScreenWide, alignScreenTall;
	g_pVGuiSurface->GetScreenSize(alignScreenWide, alignScreenTall);

	int wide, tall;
	GetSize(wide, tall);

	GetPos(redBaseX, redBaseY);
	ComputePos(inResourceData->GetString("red_base_x"), redBaseX, wide, alignScreenWide, true);
	ComputePos(inResourceData->GetString("red_base_y"), redBaseY, tall, alignScreenTall, false);

	GetPos(bluBaseX, bluBaseY);
	ComputePos(inResourceData->GetString("blu_base_x"), bluBaseX, wide, alignScreenWide, true);
	ComputePos(inResourceData->GetString("blu_base_y"), bluBaseY, tall, alignScreenTall, false);

	redOffsetX = g_pVGuiSchemeManager->GetProportionalScaledValue(inResourceData->GetInt("red_offset_x"));
	redOffsetY = g_pVGuiSchemeManager->GetProportionalScaledValue(inResourceData->GetInt("red_offset_y"));
	bluOffsetX = g_pVGuiSchemeManager->GetProportionalScaledValue(inResourceData->GetInt("blu_offset_x"));
	bluOffsetY = g_pVGuiSchemeManager->GetProportionalScaledValue(inResourceData->GetInt("blu_offset_y"));
}

void MedigunInfo::MainPanel::LoadControlSettings(const char *dialogResourceName, const char *pathID, KeyValues *pPreloadedKeyValues, KeyValues *pConditions) {
	vgui::EditablePanel::LoadControlSettings(dialogResourceName, pathID, pPreloadedKeyValues, pConditions);

	PostActionSignal(new KeyValues("ReloadControlSettings"));
}

void MedigunInfo::MainPanel::OnTick() {
	vgui::EditablePanel::OnTick();

	size_t bluMediguns = 0;
	size_t redMediguns = 0;

	for (Player player : Player::Iterable()) {
		if (player.GetClass() == TFClass_Medic && (player.GetTeam() == TFTeam_Blue || player.GetTeam() == TFTeam_Red)) {
			for (int weaponIndex = 0; weaponIndex < MAX_WEAPONS; weaponIndex++) {
				C_BaseCombatWeapon *weapon = player.GetWeapon(weaponIndex);

				if (weapon && Entities::CheckEntityBaseclass(weapon, "WeaponMedigun")) {
					MedigunPanel *medigunPanel;
					int x, y;

					if (player.GetTeam() == TFTeam_Red) {
						redMediguns++;

						if (redMediguns > redMedigunPanels.size()) {
							medigunPanel = new MedigunPanel(this, "MedigunPanel");
							AddActionSignalTarget(medigunPanel);
							redMedigunPanels.push_back(medigunPanel);
						}
						else {
							medigunPanel = redMedigunPanels.at(redMediguns - 1);
						}

						x = redBaseX + (redOffsetX * (redMediguns - 1));
						y = redBaseY + (redOffsetY * (redMediguns - 1));
					}
					else if (player.GetTeam() == TFTeam_Blue) {
						bluMediguns++;

						if (bluMediguns > bluMedigunPanels.size()) {
							medigunPanel = new MedigunPanel(this, "MedigunPanel");
							AddActionSignalTarget(medigunPanel);
							bluMedigunPanels.push_back(medigunPanel);
						}
						else {
							medigunPanel = bluMedigunPanels.at(bluMediguns - 1);
						}

						x = bluBaseX + (bluOffsetX * (bluMediguns - 1));
						y = bluBaseY + (bluOffsetY * (bluMediguns - 1));
					}

					KeyValues *medigunInfo = new KeyValues("MedigunInfo");

					int itemDefinitionIndex = *Entities::GetEntityProp<int *>(weapon, { "m_iItemDefinitionIndex" });
					TFMedigun type = TFMedigun_Unknown;
					if (itemDefinitionIndex == 29 || itemDefinitionIndex == 211 || itemDefinitionIndex == 663 || itemDefinitionIndex == 796 || itemDefinitionIndex == 805 || itemDefinitionIndex == 885 || itemDefinitionIndex == 894 || itemDefinitionIndex == 903 || itemDefinitionIndex == 912 || itemDefinitionIndex == 961 || itemDefinitionIndex == 970 || itemDefinitionIndex == 15008 || itemDefinitionIndex == 15010 || itemDefinitionIndex == 15025 || itemDefinitionIndex == 15039 || itemDefinitionIndex == 15050) {
						type = TFMedigun_MediGun;
					}
					else if (itemDefinitionIndex == 35) {
						type = TFMedigun_Kritzkrieg;
					}
					else if (itemDefinitionIndex == 411) {
						type = TFMedigun_QuickFix;
					}
					else if (itemDefinitionIndex == 998) {
						type = TFMedigun_Vaccinator;
					}

					float level = *Entities::GetEntityProp<float *>(weapon, { "m_flChargeLevel" });

					medigunInfo->SetBool("alive", player.IsAlive());
					medigunInfo->SetInt("charges", type == TFMedigun_Vaccinator ? int(floor(level * 4.0f)) : int(floor(level)));
					medigunInfo->SetFloat("level", level);
					medigunInfo->SetInt("medigun", type);
					medigunInfo->SetBool("released", *Entities::GetEntityProp<bool *>(weapon, { "m_bChargeRelease" }));
					medigunInfo->SetInt("resistType", *Entities::GetEntityProp<int *>(weapon, { "m_nChargeResistType" }));
					medigunInfo->SetInt("team", player.GetTeam());

					PostMessage(medigunPanel, medigunInfo);
					medigunPanel->SetPos(x, y);

					break;
				}
			}
		}
	}

	while (redMediguns < redMedigunPanels.size()) {
		delete redMedigunPanels.back();
		redMedigunPanels.pop_back();
	}
	
	while (bluMediguns < bluMedigunPanels.size()) {
		delete bluMedigunPanels.back();
		bluMedigunPanels.pop_back();
	}
}

MedigunInfo::MedigunPanel::MedigunPanel(vgui::Panel *parent, const char *panelName) : vgui::EditablePanel(parent, panelName) {}

void MedigunInfo::MedigunPanel::OnMedigunInfoUpdate(KeyValues *attributes) {
	bool reloadSettings = (alive != attributes->GetBool("alive") || charges != attributes->GetInt("charges") || medigun != attributes->GetInt("medigun") || released != attributes->GetBool("released") || resistType != attributes->GetInt("resistType") || team != attributes->GetInt("team"));

	alive = attributes->GetBool("alive");
	charges = attributes->GetInt("charges");
	level = attributes->GetFloat("level");
	medigun = (TFMedigun) attributes->GetInt("medigun");
	released = attributes->GetBool("released");
	resistType = (TFResistType) attributes->GetInt("resistType");
	team = (TFTeam) attributes->GetInt("team");

	if (reloadSettings) {
		KeyValues *conditions = new KeyValues("conditions");

		if (alive) {
			conditions->SetBool("player-alive", true);
		}
		else {
			conditions->SetBool("player-dead", true);
		}

		if (charges == 0) {
			conditions->SetBool("charges-0", true);
		}
		else if (charges == 1) {
			conditions->SetBool("charges-1", true);
		}
		else if (charges == 2) {
			conditions->SetBool("charges-2", true);
		}
		else if (charges == 3) {
			conditions->SetBool("charges-3", true);
		}
		else if (charges == 4) {
			conditions->SetBool("charges-4", true);
		}

		if (medigun == TFMedigun_MediGun) {
			conditions->SetBool("medigun-medigun", true);
		}
		else if (medigun == TFMedigun_Kritzkrieg) {
			conditions->SetBool("medigun-kritzkrieg", true);
		}
		else if (medigun == TFMedigun_QuickFix) {
			conditions->SetBool("medigun-quickfix", true);
		}
		else if (medigun == TFMedigun_Vaccinator) {
			conditions->SetBool("medigun-vaccinator", true);
		}

		if (released) {
			conditions->SetBool("status-released", true);
		}
		else {
			conditions->SetBool("status-building", true);
		}

		if (resistType == TFResistType_Bullet) {
			conditions->SetBool("resist-bullet", true);
		}
		else if (resistType == TFResistType_Explosive) {
			conditions->SetBool("resist-explosive", true);
		}
		else if (resistType == TFResistType_Fire) {
			conditions->SetBool("resist-fire", true);
		}

		if (team == TFTeam_Red) {
			conditions->SetBool("team-red", true);
		}
		else if (team == TFTeam_Blue) {
			conditions->SetBool("team-blu", true);
		}

		LoadControlSettings("Resource/UI/MedigunPanel.res", nullptr, nullptr, conditions);
	}

	SetDialogVariable("charge", int(floor(level * 100.0f)));
	SetDialogVariable("charges", int(floor(level * 4.0f)));
	SetDialogVariable("charge1", int(floor(level * 400.0f) - 0.0f) < 0 ? 0 : int(floor(level * 400.0f) - 0.0f));
	SetDialogVariable("charge2", int(floor(level * 400.0f) - 100.0f) < 0 ? 0 : int(floor(level * 400.0f) - 100.0f));
	SetDialogVariable("charge3", int(floor(level * 400.0f) - 200.0f) < 0 ? 0 : int(floor(level * 400.0f) - 200.0f));
	SetDialogVariable("charge4", int(floor(level * 400.0f) - 300.0f) < 0 ? 0 : int(floor(level * 400.0f) - 300.0f));
}

void MedigunInfo::MedigunPanel::OnReloadControlSettings(KeyValues *attributes) {
	KeyValues *conditions = new KeyValues("conditions");

	if (alive) {
		conditions->SetBool("player-alive", true);
	}
	else {
		conditions->SetBool("player-dead", true);
	}

	if (charges == 0) {
		conditions->SetBool("charges-0", true);
	}
	else if (charges == 1) {
		conditions->SetBool("charges-1", true);
	}
	else if (charges == 2) {
		conditions->SetBool("charges-2", true);
	}
	else if (charges == 3) {
		conditions->SetBool("charges-3", true);
	}
	else if (charges == 4) {
		conditions->SetBool("charges-4", true);
	}

	if (medigun == TFMedigun_MediGun) {
		conditions->SetBool("medigun-medigun", true);
	}
	else if (medigun == TFMedigun_Kritzkrieg) {
		conditions->SetBool("medigun-kritzkrieg", true);
	}
	else if (medigun == TFMedigun_QuickFix) {
		conditions->SetBool("medigun-quickfix", true);
	}
	else if (medigun == TFMedigun_Vaccinator) {
		conditions->SetBool("medigun-vaccinator", true);
	}

	if (released) {
		conditions->SetBool("status-released", true);
	}
	else {
		conditions->SetBool("status-building", true);
	}

	if (resistType == TFResistType_Bullet) {
		conditions->SetBool("resist-bullet", true);
	}
	else if (resistType == TFResistType_Explosive) {
		conditions->SetBool("resist-explosive", true);
	}
	else if (resistType == TFResistType_Fire) {
		conditions->SetBool("resist-fire", true);
	}

	if (team == TFTeam_Red) {
		conditions->SetBool("team-red", true);
	}
	else if (team == TFTeam_Blue) {
		conditions->SetBool("team-blu", true);
	}

	LoadControlSettings("Resource/UI/MedigunPanel.res", nullptr, nullptr, conditions);
}