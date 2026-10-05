#include "terminal.hpp"

#include <cstdio>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace cob {
namespace {
const int kKeyEscape = 27;
#ifndef _WIN32
bool readAvailableByte(unsigned char& byte, long timeoutMicroseconds) {
    fd_set inputSet;
    FD_ZERO(&inputSet);
    FD_SET(STDIN_FILENO, &inputSet);
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = timeoutMicroseconds;
    if (select(STDIN_FILENO + 1, &inputSet, 0, 0, &timeout) <= 0) return false;
    return read(STDIN_FILENO, &byte, 1) == 1;
}
#endif
}

Terminal::Terminal()
#ifdef _WIN32
    : interactive_(_isatty(_fileno(stdin)) != 0), modeSaved_(false), fullScreen_(false), outputModeSaved_(false), oldInputMode_(0), oldOutputMode_(0) {}
#else
    : interactive_(isatty(STDIN_FILENO) != 0), modeSaved_(false), fullScreen_(false) {}
#endif

Terminal::~Terminal() { restore(); }

bool Terminal::isInteractive() const { return interactive_; }

void Terminal::enterRawMode() {
    if (!interactive_) return;
#ifdef _WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(input, &oldInputMode_)) {
        modeSaved_ = true;
        SetConsoleMode(input, oldInputMode_ & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
    }
    if (GetConsoleMode(output, &oldOutputMode_)) {
        outputModeSaved_ = true;
        SetConsoleMode(output, oldOutputMode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#else
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &oldMode_) == 0) {
        raw = oldMode_;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        modeSaved_ = tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0;
    }
#endif
}

void Terminal::enterFullScreen() {
    if (!interactive_ || fullScreen_) return;
    std::fputs("\033[?1049h\033[?25l\033[2J\033[H", stdout);
    std::fflush(stdout);
    fullScreen_ = true;
}

void Terminal::restore() {
    if (!interactive_) return;
#ifdef _WIN32
    if (modeSaved_) SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), oldInputMode_);
    if (outputModeSaved_) SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), oldOutputMode_);
    modeSaved_ = false;
    outputModeSaved_ = false;
#else
    if (modeSaved_) tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldMode_);
    modeSaved_ = false;
#endif
    if (fullScreen_) {
        std::fputs("\033[?1049l", stdout);
        fullScreen_ = false;
    }
    std::fputs("\033[?25h", stdout);
    std::fflush(stdout);
}

int Terminal::readKey() {
#ifdef _WIN32
    int key = _getch();
    if (key == '\r' || key == '\n') return KeyEnter;
    if (key == 0 || key == 224) {
        const int code = _getch();
        if (code == 72) return KeyUp;
        if (code == 80) return KeyDown;
        if (code == 75) return KeyLeft;
        if (code == 77) return KeyRight;
        return 0;
    }
    return key;
#else
    unsigned char key = 0;
    if (read(STDIN_FILENO, &key, 1) != 1) return kKeyEscape;
    if (key == '\r' || key == '\n') return KeyEnter;
    if (key != 27) return key;
    unsigned char sequence[2] = {};
    if (!readAvailableByte(sequence[0], 30000) || sequence[0] != '[') return kKeyEscape;
    if (!readAvailableByte(sequence[1], 30000)) return kKeyEscape;
    if (sequence[1] == 'A') return KeyUp;
    if (sequence[1] == 'B') return KeyDown;
    if (sequence[1] == 'D') return KeyLeft;
    if (sequence[1] == 'C') return KeyRight;
    return kKeyEscape;
#endif
}

TerminalDimensions Terminal::dimensions() {
    TerminalDimensions dimensions = {24, 80};
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        dimensions.rows = info.srWindow.Bottom - info.srWindow.Top + 1;
        dimensions.columns = info.srWindow.Right - info.srWindow.Left + 1;
    }
#else
    struct winsize window;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0) {
        if (window.ws_row > 0) dimensions.rows = window.ws_row;
        if (window.ws_col > 0) dimensions.columns = window.ws_col;
    }
#endif
    return dimensions;
}

void Terminal::positionCursor(int row, int column) {
    std::printf("\033[%d;%dH", row, column);
}

void Terminal::clear() { std::fputs("\033[2J\033[H", stdout); }
void Terminal::moveToTop() { std::fputs("\033[H", stdout); }

} // namespace cob