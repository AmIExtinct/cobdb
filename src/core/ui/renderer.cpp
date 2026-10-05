#include "renderer.hpp"

#include "terminal.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace cob {
namespace {

std::string safeText(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (std::string::const_iterator character = value.begin(); character != value.end(); ++character) {
        const unsigned char byte = static_cast<unsigned char>(*character);
        result += byte < 32 || byte == 127 ? '?' : static_cast<char>(byte);
    }
    return result;
}

std::size_t characterLength(const std::string& value) {
    std::size_t length = 0;
    for (std::string::const_iterator character = value.begin(); character != value.end(); ++character) {
        if ((static_cast<unsigned char>(*character) & 0xc0) != 0x80) ++length;
    }
    return length;
}

std::string fit(const std::string& value, std::size_t width) {
    std::string result = safeText(value);
    std::size_t byteIndex = 0;
    std::size_t characters = 0;
    while (byteIndex < result.size() && characters < width) {
        const unsigned char byte = static_cast<unsigned char>(result[byteIndex]);
        const std::size_t sequenceLength = byte < 0x80 ? 1 :
            (byte & 0xe0) == 0xc0 ? 2 : (byte & 0xf0) == 0xe0 ? 3 : (byte & 0xf8) == 0xf0 ? 4 : 1;
        if (byteIndex + sequenceLength > result.size()) break;
        byteIndex += sequenceLength;
        ++characters;
    }
    return result.substr(0, byteIndex);
}

std::string pad(const std::string& value, std::size_t width) {
    std::string result = fit(value, width);
    const std::size_t used = characterLength(result);
    if (used < width) result.append(width - used, ' ');
    return result;
}

void putAt(std::ostringstream& screen, int row, int column, const std::string& value) {
    screen << "\033[" << row << ';' << column << 'H' << value;
}

void appendStatus(std::ostringstream& screen, const std::string& text, int row, int width) {
    screen << "\033[" << row << ";1H\033[2K\033[1;30;46m ";
    screen << pad(text, static_cast<std::size_t>(std::max(0, width - 2))) << ' ';
    screen << "\033[0m";
}

} // namespace

void Renderer::draw(const std::string& title, const std::string& subtitle,
                    const std::vector<std::string>& rows, std::size_t selected,
                    const std::string& emptyMessage, const std::string& status) const {
    const TerminalDimensions size = Terminal::dimensions();
    const int contentHeight = std::max(1, size.rows - 4);
    const std::size_t pageSize = static_cast<std::size_t>(contentHeight);
    const std::size_t focused = rows.empty() ? 0 : std::min(selected, rows.size() - 1);
    const std::size_t first = focused >= pageSize ? focused - pageSize + 1 : 0;
    const bool splitPane = size.columns >= 68;
    const int leftWidth = splitPane ? std::min(size.columns - 27, std::max(40, size.columns * 62 / 100)) : size.columns;
    const int rightStart = leftWidth + 2;
    const int rightWidth = std::max(1, size.columns - rightStart + 1);

    std::ostringstream screen;
    screen << "\033[?25l\033[2J\033[H";
    putAt(screen, 1, 1, "\033[1;36m " + fit(title, size.columns - 2) + "\033[0m");
    putAt(screen, 2, 1, " \033[2m" + fit(subtitle, size.columns - 2) + "\033[0m");
    putAt(screen, 3, 1, "\033[2m" + std::string(static_cast<std::size_t>(std::max(1, size.columns)), '-') + "\033[0m");

    if (splitPane) {
        std::vector<std::string> detailLines;
        detailLines.push_back("DETAILS");
        detailLines.push_back(std::string(static_cast<std::size_t>(std::max(1, rightWidth - 1)), '-'));
        if (rows.empty()) {
            detailLines.push_back("Nothing selected");
            detailLines.push_back("");
            detailLines.push_back(emptyMessage);
        } else {
            detailLines.push_back("SELECTED ITEM");
            detailLines.push_back(rows[focused]);
            detailLines.push_back("");
            detailLines.push_back("Items: " + std::to_string(rows.size()));
            detailLines.push_back("Position: " + std::to_string(focused + 1) + " / " + std::to_string(rows.size()));
        }
        detailLines.push_back("");
        detailLines.push_back("j/k  navigate");
        detailLines.push_back("Enter  open/select");

        for (int line = 0; line < contentHeight; ++line) {
            const int screenRow = line + 4;
            const std::size_t rowIndex = first + static_cast<std::size_t>(line);
            std::string listLine;
            bool isSelected = false;
            if (rows.empty() && line == 0) listLine = emptyMessage;
            else if (rowIndex < rows.size()) {
                listLine = rows[rowIndex];
                isSelected = rowIndex == focused;
            }
            const std::string clipped = pad(isSelected ? "> " + listLine : "  " + listLine,
                                            static_cast<std::size_t>(std::max(1, leftWidth - 1)));
            putAt(screen, screenRow, 1, isSelected ? "\033[1;30;43m" + clipped + "\033[0m" : clipped);
            putAt(screen, screenRow, leftWidth, "\033[2m|\033[0m");
            if (line < static_cast<int>(detailLines.size())) {
                putAt(screen, screenRow, rightStart, " " + fit(detailLines[static_cast<std::size_t>(line)],
                                                               static_cast<std::size_t>(std::max(0, rightWidth - 1))));
            }
        }
    } else {
        const std::size_t end = std::min(rows.size(), first + pageSize);
        for (int line = 0; line < contentHeight; ++line) {
            const std::size_t rowIndex = first + static_cast<std::size_t>(line);
            std::string lineText;
            bool isSelected = false;
            if (rows.empty() && line == 0) lineText = emptyMessage;
            else if (rowIndex < end) {
                lineText = rows[rowIndex];
                isSelected = rowIndex == focused;
            }
            const std::string clipped = pad(isSelected ? "> " + lineText : "  " + lineText,
                                            static_cast<std::size_t>(std::max(1, size.columns)));
            putAt(screen, line + 4, 1, isSelected ? "\033[1;30;43m" + clipped + "\033[0m" : clipped);
        }
    }
    appendStatus(screen, status, size.rows, size.columns);
    screen << "\033[?25l\033[0m";
    const std::string output = screen.str();
    std::fwrite(output.data(), 1, output.size(), stdout);
    std::fflush(stdout);
}

void Renderer::drawInput(const std::string& title, const std::string& label,
                         const std::string& value, const std::string& status) const {
    const TerminalDimensions size = Terminal::dimensions();
    const std::size_t fieldWidth = static_cast<std::size_t>(std::max(12, size.columns - 8));
    const int fieldTop = std::max(5, size.rows / 2 - 2);
    std::ostringstream screen;
    screen << "\033[?25l\033[2J\033[H";
    putAt(screen, 1, 1, "\033[1;36m " + fit(title, size.columns - 2) + "\033[0m");
    putAt(screen, fieldTop, 4, "\033[2m" + fit(label, fieldWidth) + "\033[0m");
    putAt(screen, fieldTop + 1, 4, "+" + std::string(fieldWidth, '-') + "+");
    putAt(screen, fieldTop + 2, 4, "| > " + pad(value, fieldWidth - 4) + " |");
    putAt(screen, fieldTop + 3, 4, "+" + std::string(fieldWidth, '-') + "+");
    appendStatus(screen, status, size.rows, size.columns);
    const std::string output = screen.str();
    std::fwrite(output.data(), 1, output.size(), stdout);

    const std::size_t cursorOffset = std::min(characterLength(safeText(value)), fieldWidth - 4);
    std::printf("\033[?25h");
    Terminal::positionCursor(fieldTop + 2, static_cast<int>(cursorOffset + 8));
    std::fflush(stdout);
}

void Renderer::drawPairInput(const std::string& title, const std::string& key,
                             const std::string& value, int activeField,
                             std::size_t cursorPosition,
                             const std::string& status) const {
    const TerminalDimensions size = Terminal::dimensions();
    const std::size_t fieldWidth = static_cast<std::size_t>(std::max(12, size.columns - 8));
    const std::string border(fieldWidth, '-');
    const int formTop = std::max(4, size.rows / 2 - 5);
    std::ostringstream screen;
    screen << "\033[?25l\033[2J\033[H";
    putAt(screen, 1, 1, "\033[1;36m " + fit(title, size.columns - 2) + "\033[0m");
    putAt(screen, formTop, 4, "\033[2mKey\033[0m");
    putAt(screen, formTop + 1, 4, "+" + border + "+");
    putAt(screen, formTop + 2, 4, "| > " + pad(key, fieldWidth - 4) + " |");
    putAt(screen, formTop + 3, 4, "+" + border + "+");
    putAt(screen, formTop + 5, 4, "\033[2mValue\033[0m");
    putAt(screen, formTop + 6, 4, "+" + border + "+");
    putAt(screen, formTop + 7, 4, "| > " + pad(value, fieldWidth - 4) + " |");
    putAt(screen, formTop + 8, 4, "+" + border + "+");
    appendStatus(screen, status, size.rows, size.columns);
    const std::string output = screen.str();
    std::fwrite(output.data(), 1, output.size(), stdout);

    const std::string& activeValue = activeField == 0 ? key : value;
    const std::size_t safeCursor = std::min(cursorPosition, activeValue.size());
    const std::size_t cursorOffset = std::min(characterLength(safeText(activeValue.substr(0, safeCursor))), fieldWidth - 4);
    std::printf("\033[?25h");
    Terminal::positionCursor(formTop + (activeField == 0 ? 2 : 7), static_cast<int>(cursorOffset + 8));
    std::fflush(stdout);
}

void Renderer::drawConfirmation(const std::string& question, const std::string& status) const {
    draw("CONFIRM", question, std::vector<std::string>(1, "Press y to confirm, any other key to cancel"),
         0, std::string(), status);
}

} // namespace cob