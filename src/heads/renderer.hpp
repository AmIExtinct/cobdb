#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace cob {

class Renderer {
public:
    void draw(const std::string& title, const std::string& subtitle,
              const std::vector<std::string>& rows, std::size_t selected,
              const std::string& emptyMessage, const std::string& status) const;
    void drawInput(const std::string& title, const std::string& label,
                   const std::string& value, const std::string& status) const;
    void drawPairInput(const std::string& title, const std::string& key,
                       const std::string& value, int activeField,
                       std::size_t cursorPosition,
                       const std::string& status) const;
    void drawConfirmation(const std::string& question, const std::string& status) const;
};

} // namespace cob