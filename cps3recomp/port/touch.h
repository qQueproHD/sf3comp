// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp); from cps2recomp.
//
// Controls on a touch screen (a phone): a stick at the left, the game's buttons at the right (six, or the game's
// own: game.cfg's buttons), and Coin, Start, the menu and (where the game zooms) the zoom at the top; player 1's,
// beside a gamepad's and the keys.
// They go where the window has room beside the picture (a phone's screen is wider than it), over its edges where
// it has not. They are shown on Android until a gamepad or a key is used, and anywhere once the screen is touched.
// With no finger on the screen for five seconds they are no longer drawn (the picture whole again); a touch brings
// them back, and is a press of what is under it, as when they are drawn.
// In the menu the stick moves through it and the first two buttons choose and go back.
// Touch movement (the feature's first choice): the stick is where the finger goes down, anywhere in the left half
// of the screen away from the buttons, as most phone fighting games have it; or fixed where it is drawn.
// Touch controls Off (the feature): never shown, never pressed. Fingers on a touch screen only (a laptop's or a Mac's
// touchpad sends them too: not these).
#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace port {

class TouchControls {
public:
    TouchControls();
    // The window's size in its pixels, and where the picture is in it
    void Layout(int width, int height, const SDL_FRect &picture);
    // An event of the window's: a finger's is the controls' (true); a gamepad's or a key's hides them
    bool Event(const SDL_Event &event);
    bool Shown() const {
        return m_shown && m_allowed;
    }
    // Off (the Touch controls feature): never shown, never pressed
    void Allow(bool allowed);
    // The stick where the finger goes down (on), or fixed where it is drawn (the Touch movement feature)
    void Floating(bool on);
    // A finger asked first, shown or not, allowed or not: true takes it from the controls (the open in-game menu's
    // taps, frontend_sdl.cpp, so that it answers a finger even with the controls off). (x, y) in the window's pixels
    std::function<bool(const SDL_Event &event, float x, float y)> before;
    // One of the controls' buttons drawn under (x, y), in the window's pixels
    bool OnButton(float x, float y) const {
        return Shown() && !m_idle && Hit(x, y) >= 0;
    }
    // (checks: a finger put down, moved and lifted, in the window's pixels, as a touch screen's would be)
    void Scripted(SDL_EventType type, float x, float y);
    // Player 1's controls held (cps3recomp::Button bits), read once a frame: a press lasts a few frames however short
    // the tap; and the menu's choose and back (recomp::kMenu* bits) as last read. With the menu open, the stick held
    // in one way goes on through its rows, as a held key does in a list.
    uint16_t Read(bool menuOpen);
    unsigned Menu() const;
    // The menu button, or the zoom's (the camera on both fighters, or off: zoom.h; online, the chat's), was pressed
    // since this was last asked
    bool TakeMenuPress();
    bool TakeZoomPress();
    // Online play (no zoom): the zoom's button is the chat's, which opens the system's keyboard
    void Chat(bool chat) {
        if (chat != m_chat) {
            m_chat = chat;
            m_width = 0; // (laid out again)
        }
    }
    // Their picture, when it changed since this was last asked (and for nothing shown, an empty one): the
    // presenter's overlay (present.h, SetOverlay), half the window's size
    bool TakePicture(std::vector<uint32_t> &pixels, int &width, int &height);

private:
    struct Control {
        float x, y, radius; // in the window's pixels
        uint16_t bits;      // what it presses (none for the menu button)
        std::string label;
        bool menu = false, zoom = false;
    };
    int Hit(float x, float y) const;
    bool InStick(float x, float y) const;
    void Changed() {
        m_changed = true;
    }
    void Draw();

    bool m_shown = false, m_changed = true, m_menuPressed = false, m_zoomPressed = false;
    bool m_idle = false;    // (shown, but no finger for a while: not drawn)
    bool m_chat = false;
    bool m_allowed = true, m_floating = false;
    uint64_t m_touched = 0; // (when a finger was last on the screen, SDL_GetTicks)
    int m_width = 0, m_height = 0;
    SDL_FRect m_picture{};
    float m_stickX = 0, m_stickY = 0, m_stickRadius = 0; // the stick's middle and reach
    float m_homeX = 0, m_homeY = 0;                      // its middle with no finger on it (floating: where it is drawn)
    float m_knobX = 0, m_knobY = 0;                      // where its finger is
    std::vector<Control> m_controls;
    std::vector<std::string> m_names;                    // the game's buttons'
    struct Finger {
        int control; // the stick (-1) or a control
        int reads;   // frames it has been read in
    };
    std::map<SDL_FingerID, Finger> m_fingers;            // each finger down
    std::vector<std::pair<uint16_t, int>> m_lingering;   // a short tap's: its bits, and the frames still to give them
    uint16_t m_stick = 0, m_read = 0, m_stickBefore = 0;
    int m_stickHeld = 0; // the frames the stick has been held the same way
    std::vector<uint32_t> m_pixels;
    int m_pictureWidth = 0, m_pictureHeight = 0;
};

} // namespace port
