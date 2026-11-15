// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_GAME_MENU_BUTTON_H_
#define REODE_LOADER_SRC_GAME_MENU_BUTTON_H_

namespace reode::game {

// Hooks MenuLayer::init so that the main menu gets a Reode button, which
// toggles an overlay listing every mod and its state.
//
// MenuLayer::init is located at runtime: it overrides cocos2d::CCLayer::init,
// so its address is the entry of MenuLayer's RTTI-located vtable at the slot
// that CCLayer::init occupies in libcocos2d's exported CCLayer vtable. No
// hard-coded game addresses are involved.
bool InstallMenuButton();

}  // namespace reode::game

#endif  // REODE_LOADER_SRC_GAME_MENU_BUTTON_H_
