#pragma once
#include "framework/App.h"   // InputEvent

class M5EPD_Canvas;           // global namespace fwd-decl (as in framework/App.h)

namespace paperos {

// A game inside GamesApp. GamesApp owns the canvas, draws the header (title())
// and pushes; the game renders its content region (y >= kContentTopY) and its
// own action bar, and maps discrete input events (TouchUp / Nav). After routing
// an event GamesApp re-renders and pushes Partial — unless consumeFullRefresh()
// returns true (set by reset()/new-game), in which case it pushes Full.
class IGame {
public:
    virtual ~IGame() = default;
    virtual void reset() = 0;                       // start a fresh game (wrapper seeds RNG)
    virtual void onInput(const InputEvent& e) = 0;
    virtual void render(M5EPD_Canvas& c) = 0;
    virtual const char* title() const = 0;
    virtual const char* iconId() const = 0;         // IconKit::app id for the menu row
    virtual bool consumeFullRefresh() = 0;          // true once after reset/new-game
};

} // namespace paperos
