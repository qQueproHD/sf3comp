// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp); from cps2recomp.
//
// The in-game menu (menu.h): the shared menu told what a CPS-2 game's is made of.
#include "menu.h"

#include <cstring>

#include "../recomp-common/menu_layout.h"
#include "../runtime/system.h"
#include "../runtime/game.h"

#include <algorithm>
#include <iterator>

namespace port {

namespace {
    // The first page's own rows
    enum { kCoin = 1, kStart2, kTest, kLeave };
} // namespace

void Menu::OpenOnline(Online online, bool keep) {
    m_menu.Close();
    recomp::MenuConfig config;
    config.groups = {{{recomp::MenuRow::Page, 0, kOnlinePage}},
                     {{recomp::MenuRow::Mute}, {recomp::MenuRow::Resume}},
                     {{recomp::MenuRow::Action, kLeave, online.leave, online.leaveDescription}}};
    config.pages.push_back({kOnlinePage, "", "The match's modes and your own picks for them.", false});
    config.features = std::move(online.features);
    config.commands = std::move(online.commands);
    config.value = std::move(online.value);
    config.setValue = std::move(online.setValue);
    config.muted = [this] { return muted; };
    config.setMuted = [this](bool to) { muted = to; };
    config.confirmName = "Enter or A";
    config.closeName = "Esc";
    if (keep && m_online && m_onlineMenu.IsOpen()) {
        m_onlineMenu.Configure(std::move(config));
        return;
    }
    m_onlineMenu = recomp::Menu();
    m_onlineMenu.SetCompact(true); // (drawn on the game's own picture, at the board's size)
    m_onlineMenu.Configure(std::move(config));
    m_onlineMenu.Open();
    m_onlineMenu.Buttons(recomp::kMenuConfirm); // (into the Online match page, its first row: the first page's first)
    m_online = true;
}

void Menu::Open(bool withStates) {
    m_online = false;
    m_onlineMenu.Close();
    if (onOpen) {
        onOpen();
    }
    const cps3recomp::Game &game = cps3recomp::TheGame();
    recomp::MenuConfig config;
    // The standard layout (recomp-common/menu_layout.h, cps2recomp's): Online, Display, Game, Replays and Stage first;
    // Alternative modes (Training mode under it, Dummy under that); the save states; the other pages and the board's
    // own rows; Reset all options, Reset game, Quit game, Resume. Under Game: the speed, Mute, Board, Cheats, Sound.
    // (the game's pages first, so that their words are what the rows say: Standard keeps them and places the pages)
    std::vector<std::string> modes;
    for (const cps3recomp::Page &page : game.pages) {
        // (such pages are ways of running the game: their rows do not count features that are on)
        config.pages.push_back({page.name, page.parent, page.description, false});
        const std::string name = page.name;
        if (page.top && !*page.parent && name != recomp::layout::kStage && name != recomp::layout::kModes) {
            modes.push_back(name); // (another of the game's ways to play: beside Alternative modes)
        }
    }
    recomp::layout::Rows rows;
    if (replaysPage) {
        rows.heads.push_back("Replays");
        config.pages.push_back({"Replays", "", "Your matches, kept as you play them, to watch again and to play on from.", false});
    }
    rows.heads.push_back(recomp::layout::kStage);
    rows.modes = std::move(modes);
    rows.states = withStates;
    config.pages.push_back({"Arcade monitor", recomp::layout::kDisplay, "A CRT at the board's own resolution: set on the launcher."});
    rows.actions = {
        {recomp::MenuRow::Action, kCoin, recomp::layout::kInsertCoin, "Puts a coin in the first slot."},
        {recomp::MenuRow::Action, kStart2, recomp::layout::kPlayer2Joins,
         "Presses player 2's Start: with a coin in (or free play), a second player to practise against."},
        {recomp::MenuRow::Action, kTest, recomp::layout::kTestMenu,
         "The game's own operator menu (the board's test switch, F2): its settings, such as difficulty and rounds. "
         "Its Exit row goes back."},
    };
    rows.resetOptionsDescription = "Puts every option back as it first was (the launcher's Enhanced preset), and the speed to full. The "
                                   "game's own settings (its Test menu) and the controls stay as they are.";
    rows.resetDescription = "Starts the board again, as if switched off and on.";
    recomp::layout::Standard(config, rows);
    for (const cps3recomp::Feature &feature : game.features) {
        config.features.push_back({feature.category, feature.title, feature.description, {feature.choices.begin(), feature.choices.end()}});
    }
    for (const cps3recomp::Command &command : game.commands) {
        if (hosting && !strcmp(command.name, "online_host")) {
            config.commands.push_back({command.category, "Stop hosting",
                                       "Others can no longer join: the match's code stops working. Your game goes on as it is."});
            continue;
        }
        if (recordingClip && !strcmp(command.name, "scene_clip")) {
            config.commands.push_back({command.category, "Stop recording", "Ends the clip being recorded here; its frames so far are kept."});
            continue;
        }
        if (recordingVideo && !strcmp(command.name, "video_record")) {
            config.commands.push_back({command.category, "Stop recording video", "Ends the video being recorded and saves it."});
            continue;
        }
        config.commands.push_back({command.category, command.title, command.description});
    }
    // (the replays' rows: commands after the game's, Press tells them by their place)
    for (const ReplayRow &row : replays) {
        config.commands.push_back({"Replays", row.title, row.detail});
    }
    config.value = [](size_t feature) { return cps3recomp::FeatureSetting(cps3recomp::TheGame().features[feature].name); };
    config.setValue = [](size_t feature, int value) { cps3recomp::SetFeatureValue(cps3recomp::TheGame().features[feature].name, value); };
    // (of a long choice, those the launcher leaves in the menu: settings.ini's [menu], recomp-common/settings.h)
    {
        recomp::Ini settings(cps3recomp::SettingsPath());
        settings.load();
        std::vector<std::vector<bool>> offered;
        for (const recomp::FeatureInfo &info : cps3recomp::FeatureList()) {
            offered.push_back(recomp::MenuChoices(settings, info));
            if (info.flags & cps3recomp::kFeatureLauncherOnly) {
                // (none but the one chosen and off, which the menu always offers: the row can only be put back to off)
                offered.back().assign(std::max<size_t>(info.choices.size(), 2), false);
            }
            if (choiceAvailable) {
                for (size_t choice = 1; choice < offered.back().size(); choice++) {
                    if (offered.back()[choice] && !choiceAvailable(info.name, static_cast<int>(choice))) {
                        offered.back()[choice] = false;
                    }
                }
            }
        }
        config.offered = [offered](size_t feature, int choice) {
            return feature >= offered.size() || choice < 0 || choice >= static_cast<int>(offered[feature].size()) || offered[feature][static_cast<size_t>(choice)];
        };
    }
    recomp::layout::Speeds(config);
    config.speedDescription = "Left/Right: slow motion, or faster than the board. While playing, P (or a click of the right stick) stops "
                              "the game and starts it again; O (or, once stopped, the left trigger) goes on by one frame; - and = change "
                              "the speed.";
    config.muted = [this] { return muted; };
    config.setMuted = [this](bool to) { muted = to; };
    config.confirmName = "Enter or A";
    config.closeName = "Esc";
    m_menu.Configure(std::move(config));
    m_menu.Open();
    if (!m_waiting.empty()) {
        m_menu.Note(m_waiting);
        m_waiting.clear();
    }
}

Menu::Menu() {
    // (so that - and = work before the menu is first opened)
    recomp::MenuConfig config;
    recomp::layout::Speeds(config);
    m_menu.Configure(std::move(config));
}

Menu::Rate Menu::GameRate() const {
    const recomp::layout::Speed speed = recomp::layout::SpeedOf(m_menu.Speed());
    // (the slower of the player's choice and what the game's sources ask for: runtime/game.h)
    const int slowBy = cps3recomp::SlowMotion();
    if (slowBy > 1 && speed.frames * slowBy > speed.ticks) {
        return {1, slowBy};
    }
    return {speed.frames, speed.ticks};
}

void Menu::SetFullSpeed() {
    m_menu.SetSpeed(recomp::layout::kFull);
}

void Menu::Faster(int by) {
    m_menu.SetSpeed(m_menu.Speed() + by);
}

Menu::Action Menu::Buttons(uint16_t pressed) {
    unsigned buttons = 0;
    buttons |= (pressed & cps3recomp::kUp) ? recomp::kMenuUp : 0;
    buttons |= (pressed & cps3recomp::kDown) ? recomp::kMenuDown : 0;
    buttons |= (pressed & cps3recomp::kLeft) ? recomp::kMenuLeft : 0;
    buttons |= (pressed & cps3recomp::kRight) ? recomp::kMenuRight : 0;
    if (pressed & (cps3recomp::kPunch1 | cps3recomp::kPunch2 | cps3recomp::kPunch3 | cps3recomp::kKick1 | cps3recomp::kKick2 |
                   cps3recomp::kKick3 | cps3recomp::kStart)) {
        buttons |= recomp::kMenuConfirm;
    }
    // (a press of something else, a coin button say, still takes the note away, as it always did)
    if (pressed && !buttons && IsOpen()) {
        Shown().Note("");
    }
    return Press(buttons);
}

Menu::Action Menu::Press(unsigned buttons) {
    return Take(Shown().Buttons(buttons));
}

Menu::Action Menu::Take(const recomp::Menu::Chosen &chosen) {
    switch (chosen.kind) {
    case recomp::Menu::Chosen::Save: return Action::Save;
    case recomp::Menu::Chosen::Load: return Action::Load;
    case recomp::Menu::Chosen::Quit: return Action::Quit;
    case recomp::Menu::Chosen::Reset: return Action::Reset;
    case recomp::Menu::Chosen::ResetOptions: return Action::ResetOptions;
    case recomp::Menu::Chosen::Command:
        m_command = static_cast<size_t>(chosen.id);
        if (m_online) {
            return Action::OnlineCommand;
        }
        if (m_command >= cps3recomp::TheGame().commands.size()) {
            m_replay = m_command - cps3recomp::TheGame().commands.size();
            return Action::Replay;
        }
        return Action::Command;
    case recomp::Menu::Chosen::Action:
        return chosen.id == kLeave ? Action::Leave : chosen.id == kCoin ? Action::Coin : chosen.id == kStart2 ? Action::Start2 : chosen.id == kTest ? Action::Test : Action::None;
    default: return Action::None;
    }
}

} // namespace port
