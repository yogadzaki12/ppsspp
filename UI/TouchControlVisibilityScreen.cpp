// Copyright (c) 2013- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include "Common/System/Display.h"
#include "Common/Render/TextureAtlas.h"
#include "Common/Data/Text/I18n.h"
#include "Common/StringUtils.h"
#include "Common/UI/TabHolder.h"
#include "Common/UI/PopupScreens.h"

#include "Core/Config.h"
#include "Core/KeyMap.h"

#include "UI/TouchControlVisibilityScreen.h"
#include "UI/CustomButtonMappingScreen.h"
#include "UI/GamepadEmu.h"

static const int leftColumnWidth = 140;

class CheckBoxChoice : public UI::Choice {
public:
	CheckBoxChoice(std::string_view text, UI::CheckBox *checkbox, UI::LayoutParams *lp)
		: Choice(text, lp), checkbox_(checkbox) {
		OnClick.Handle(this, &CheckBoxChoice::HandleClick);
	}
	CheckBoxChoice(ImageID imgID, UI::CheckBox *checkbox, UI::LayoutParams *lp)
		: Choice(imgID, lp), checkbox_(checkbox) {
		OnClick.Handle(this, &CheckBoxChoice::HandleClick);
	}

private:
	void HandleClick(UI::EventParams &e);

	UI::CheckBox *checkbox_;
};

class EmotionBindingMappingScreen : public UISimpleBaseDialogScreen {
public:
	EmotionBindingMappingScreen(const Path &gamePath, int id) : UISimpleBaseDialogScreen(gamePath, SimpleDialogFlags::ContentsCanScroll), id_(id) {}

	void CreateDialogViews(UI::ViewGroup *parent) override {
		using namespace UI;
		using namespace CustomKeyData;
		auto mc = GetI18NCategory(I18NCat::MAPPABLECONTROLS);

		if (id_ < 0 || id_ >= Config::EMOTION_BUTTON_ITEM_COUNT) {
			return;
		}

		for (int i = 0; i < ARRAY_SIZE(g_customKeyList); ++i) {
			array_[i] = (g_Config.EmotionButton[id_] & (1ULL << i)) != 0;
		}

		const bool portrait = GetDeviceOrientation() == DeviceOrientation::Portrait;
		const int cellSize = portrait ? std::min((g_display.dp_xres / 2 - 10), 290) : 380;
		GridLayoutSettings gridsettings(cellSize, 64, 5);
		gridsettings.fillCells = true;
		GridLayout *grid = parent->Add(new GridLayout(gridsettings, new LayoutParams(FILL_PARENT, WRAP_CONTENT)));

		for (int i = 0; i < ARRAY_SIZE(g_customKeyList); ++i) {
			LinearLayout *row = new LinearLayout(ORIENT_HORIZONTAL, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT));
			row->SetSpacing(0);

			CheckBox *checkbox = new CheckBox(&array_[i], "", "", new LinearLayoutParams(50, WRAP_CONTENT));
			row->Add(checkbox);

			Choice *choice;
			if (g_customKeyList[i].i.isValid()) {
				choice = new Choice(g_customKeyList[i].i, new LinearLayoutParams(1.0f));
			} else {
				choice = new Choice(mc->T(KeyMap::GetPspButtonNameCharPointer(g_customKeyList[i].c)), new LinearLayoutParams(1.0f));
			}

			choice->SetCentered(true);
			choice->OnClick.Add([checkbox](EventParams &) {
				checkbox->Toggle();
			});

			row->Add(choice);
			grid->Add(row);
		}
	}

	void onFinish(DialogResult result) override {
		if (id_ < 0 || id_ >= Config::EMOTION_BUTTON_ITEM_COUNT) {
			return;
		}

		uint64_t value = 0;
		for (int i = ARRAY_SIZE(array_) - 1; i >= 0; --i) {
			value |= array_[i] ? 1ULL : 0ULL;
			if (i > 0) {
				value <<= 1;
			}
		}
		g_Config.EmotionButton[id_] = value;
	}

	std::string_view GetTitle() const override {
		auto mc = GetI18NCategory(I18NCat::MAPPABLECONTROLS);
		char temp[32];
		snprintf(temp, sizeof(temp), "Emotion %d", id_ + 1);
		title_ = std::string(mc->T(temp)) + " binding";
		return title_;
	}

	const char *tag() const override { return "EmotionBindingMapping"; }

private:
	bool array_[ARRAY_SIZE(CustomKeyData::g_customKeyList)]{};
	int id_ = 0;
	mutable std::string title_;
};

std::string_view TouchControlVisibilityScreen::GetTitle() const {
	auto co = GetI18NCategory(I18NCat::CONTROLS);
	return co->T("Touch Control Visibility");
}

void TouchControlVisibilityScreen::CreateContextMenu(UI::ViewGroup *parent) {
	using namespace UI;

	auto di = GetI18NCategory(I18NCat::DIALOG);

	Choice *toggleAll = parent->Add(new Choice(di->T("Toggle All")));
	toggleAll->OnClick.Add([this](UI::EventParams &e) {
		// TODO: Is this a meaningful operation to support?
		for (auto toggle : toggles_) {
			*toggle.show = nextToggleAll_;
		}
		nextToggleAll_ = !nextToggleAll_;
	});
}

void TouchControlVisibilityScreen::CreateDialogViews(UI::ViewGroup *parent) {
	using namespace UI;
	using namespace CustomKeyData;

	auto di = GetI18NCategory(I18NCat::DIALOG);
	auto co = GetI18NCategory(I18NCat::CONTROLS);
	auto mc = GetI18NCategory(I18NCat::MAPPABLECONTROLS);

	const bool portrait = GetDeviceOrientation() == DeviceOrientation::Portrait;

	const int cellSize = portrait ? std::min((g_display.dp_xres / 2 - 10), 290) : 380;
	UI::GridLayoutSettings gridsettings(cellSize, 64, 5);
	gridsettings.fillCells = true;
	GridLayout *grid = parent->Add(new GridLayoutList(gridsettings, new LayoutParams(FILL_PARENT, WRAP_CONTENT)));

	TouchControlConfig &touch = g_Config.GetTouchControlsConfig(GetDeviceOrientation());

	toggles_.clear();
	toggles_.push_back({ "Circle", &touch.bShowTouchCircle, ImageID("I_CIRCLE"), nullptr });
	toggles_.push_back({ "Cross", &touch.bShowTouchCross, ImageID("I_CROSS"), nullptr });
	toggles_.push_back({ "Square", &touch.bShowTouchSquare, ImageID("I_SQUARE"), nullptr });
	toggles_.push_back({ "Triangle", &touch.bShowTouchTriangle, ImageID("I_TRIANGLE"), nullptr });
	toggles_.push_back({ "L", &touch.touchLKey.show, ImageID("I_L"), nullptr });
	toggles_.push_back({ "R", &touch.touchRKey.show, ImageID("I_R"), nullptr });
	toggles_.push_back({ "Start", &touch.touchStartKey.show, ImageID("I_START"), nullptr });
	toggles_.push_back({ "Select", &touch.touchSelectKey.show, ImageID("I_SELECT"), nullptr });
	toggles_.push_back({ "Dpad", &touch.touchDpad.show, ImageID::invalid(), nullptr });
	toggles_.push_back({ "Analog Stick", &touch.touchAnalogStick.show, ImageID::invalid(), nullptr });
	toggles_.push_back({ "Right Analog Stick", &touch.touchRightAnalogStick.show, ImageID::invalid(), [=](EventParams &e) {
		screenManager()->push(new RightAnalogMappingScreen(gamePath_));
	}});
	toggles_.push_back({ "Fast-forward", &touch.touchFastForwardKey.show, ImageID::invalid(), nullptr});
	toggles_.push_back({ "Emotion", &touch.touchEmotionKey.show, ImageID("I_THREE_DOTS"), [=](EventParams &e) {
		std::vector<std::string> items;
		for (int i = 0; i < Config::EMOTION_BUTTON_ITEM_COUNT; ++i) {
			char temp[32];
			snprintf(temp, sizeof(temp), "Emotion %d", i + 1);
				items.push_back(std::string(mc->T(temp)));
		}

		auto *popup = new ListPopupScreen(mc->T("Emotion"), items, -1, [=](int index) {
			if (index >= 0 && index < Config::EMOTION_BUTTON_ITEM_COUNT) {
				screenManager()->push(new EmotionBindingMappingScreen(gamePath_, index));
			}
		});
		if (e.v) {
			popup->SetPopupOrigin(e.v);
		}
		screenManager()->push(popup);
	}});
	toggles_.push_back({ "Pause", &touch.touchPauseKey.show, ImageID("I_HAMBURGER"), nullptr});

	for (int i = 0; i < TouchControlConfig::CUSTOM_BUTTON_COUNT; i++) {
		char temp[256];
		snprintf(temp, sizeof(temp), "Custom %d", i + 1);
		toggles_.push_back({ temp, &touch.touchCustom[i].show, ImageID::invalid(), [=](EventParams &e) {
			screenManager()->push(new CustomButtonMappingScreen(GetDeviceOrientation(), gamePath_, i));
		} });
	}

	for (auto toggle : toggles_) {
		LinearLayout *row = new LinearLayout(ORIENT_HORIZONTAL, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT));
		row->SetSpacing(0);

		CheckBox *checkbox = new CheckBox(toggle.show, "", "", new LinearLayoutParams(50, WRAP_CONTENT));
		row->Add(checkbox);

		Choice *choice;
		if (toggle.handle) {
			// Handle custom button strings differently, and hackily. But will extend to arbitrary button counts.
			char translated[256];
			int i = 0;
			if (sscanf(toggle.key.c_str(), "Custom %d", &i) == 1) {
				snprintf(translated, sizeof(translated), mc->T_cstr("Custom %d"), i);
			} else {
				truncate_cpy(translated, sizeof(translated), mc->T(toggle.key));
			}
			choice = new Choice(std::string(translated) + " (" + std::string(mc->T("tap to customize")) + ")", "", new LinearLayoutParams(1.0f));
			choice->OnClick.Add(toggle.handle);
		} else if (toggle.img.isValid()) {
			choice = new CheckBoxChoice(toggle.img, checkbox, new LinearLayoutParams(1.0f));
		} else {
			choice = new CheckBoxChoice(mc->T(toggle.key), checkbox, new LinearLayoutParams(1.0f));
		}

		// Cannot hide the back button if the system doesn't have a built-in one.
		if (toggle.key == "Pause" && !System_GetPropertyBool(SYSPROP_HAS_BACK_BUTTON)) {
			checkbox->SetEnabled(false);
			choice->SetEnabled(false);
		}

		choice->SetCentered(true);
		row->Add(choice);
		grid->Add(row);
	}
}

void TouchControlVisibilityScreen::onFinish(DialogResult result) {
	g_Config.Save("TouchControlVisibilityScreen::onFinish");
}

std::string_view RightAnalogMappingScreen::GetTitle() const {
	auto mc = GetI18NCategory(I18NCat::MAPPABLECONTROLS);
	return mc->T("Right Analog Stick");
}

void RightAnalogMappingScreen::CreateDialogViews(UI::ViewGroup *parent) {
	using namespace UI;

	auto di = GetI18NCategory(I18NCat::DIALOG);
	auto co = GetI18NCategory(I18NCat::CONTROLS);
	auto mc = GetI18NCategory(I18NCat::MAPPABLECONTROLS);

	TouchControlConfig &touch = g_Config.GetTouchControlsConfig(GetDeviceOrientation());

	static const char *rightAnalogButton[] = {"None", "L", "R", "Square", "Triangle", "Circle", "Cross", "D-pad up", "D-pad down", "D-pad left", "D-pad right", "Start", "Select", "RightAn.Up", "RightAn.Down", "RightAn.Left", "RightAn.Right", "An.Up", "An.Down", "An.Left", "An.Right"};

	parent->Add(new ItemHeader(co->T("Analog Style")));
	parent->Add(new CheckBox(&touch.touchRightAnalogStick.show, co->T("Visible")));
	parent->Add(new CheckBox(&g_Config.bRightAnalogCustom, co->T("Use custom right analog")));
	parent->Add(new CheckBox(&g_Config.bRightAnalogDisableDiagonal, co->T("Disable diagonal input")))->SetEnabledPtr(&g_Config.bRightAnalogCustom);

	parent->Add(new ItemHeader(co->T("Analog Binding")));
	parent->Add(new PopupMultiChoice(&g_Config.iRightAnalogUp, mc->T("RightAn.Up"), rightAnalogButton, 0, ARRAY_SIZE(rightAnalogButton), I18NCat::MAPPABLECONTROLS, screenManager()))->SetEnabledPtr(&g_Config.bRightAnalogCustom);
	parent->Add(new PopupMultiChoice(&g_Config.iRightAnalogDown, mc->T("RightAn.Down"), rightAnalogButton, 0, ARRAY_SIZE(rightAnalogButton), I18NCat::MAPPABLECONTROLS, screenManager()))->SetEnabledPtr(&g_Config.bRightAnalogCustom);
	parent->Add(new PopupMultiChoice(&g_Config.iRightAnalogLeft, mc->T("RightAn.Left"), rightAnalogButton, 0, ARRAY_SIZE(rightAnalogButton), I18NCat::MAPPABLECONTROLS, screenManager()))->SetEnabledPtr(&g_Config.bRightAnalogCustom);
	parent->Add(new PopupMultiChoice(&g_Config.iRightAnalogRight, mc->T("RightAn.Right"), rightAnalogButton, 0, ARRAY_SIZE(rightAnalogButton), I18NCat::MAPPABLECONTROLS, screenManager()))->SetEnabledPtr(&g_Config.bRightAnalogCustom);
	parent->Add(new PopupMultiChoice(&g_Config.iRightAnalogPress, co->T("Keep this button pressed when right analog is pressed"), rightAnalogButton, 0, ARRAY_SIZE(rightAnalogButton) - 8, I18NCat::MAPPABLECONTROLS, screenManager()))->SetEnabledPtr(&g_Config.bRightAnalogCustom);
}

void CheckBoxChoice::HandleClick(UI::EventParams &e) {
	checkbox_->Toggle();
};
