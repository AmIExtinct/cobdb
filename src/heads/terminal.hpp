#pragma once

#ifndef _WIN32
#include <termios.h>
#endif

namespace cob {

struct TerminalDimensions {
    int rows;
    int columns;
};

class Terminal {
public:
    enum Key { KeyEnter = 13, KeyUp = 1001, KeyDown = 1002, KeyLeft = 1003, KeyRight = 1004 };

    Terminal();
    ~Terminal();

    bool isInteractive() const;
    int readKey();
    void enterRawMode();
    void enterFullScreen();
    void restore();

    static TerminalDimensions dimensions();
    static void positionCursor(int row, int column);
    static void clear();
    static void moveToTop();

private:
    bool interactive_;
    bool modeSaved_;
    bool fullScreen_;
#ifdef _WIN32
    bool outputModeSaved_;
    unsigned long oldInputMode_;
    unsigned long oldOutputMode_;
#else
    struct termios oldMode_;
#endif
};

} // namespace cob