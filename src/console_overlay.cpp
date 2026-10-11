#include "../include/console_overlay.h"

#include "../include/engine_sim_application.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace {
    std::vector<std::string> splitLines(const std::string &text) {
        std::vector<std::string> lines;
        std::stringstream ss(text);
        std::string line;
        while (std::getline(ss, line)) {
            // Tabs aren't part of the font
            std::replace(line.begin(), line.end(), '\t', ' ');
            lines.push_back(line);
        }

        return lines;
    }
} /* namespace */

ConsoleOverlay::ConsoleOverlay() {
    m_scrollOffset = 0;
}

ConsoleOverlay::~ConsoleOverlay() {
    /* void */
}

void ConsoleOverlay::scroll(int lines) {
    m_scrollOffset = std::max(0, m_scrollOffset + lines);
}

void ConsoleOverlay::drawLine(const std::string &line, const Bounds &bounds, float y, float height) {
    // Clip lines that don't fit instead of letting them run off the panel
    std::string s = line;
    const float maxWidth = bounds.width();
    dbasic::TextRenderer *text = m_app->getTextRenderer();
    while (!s.empty() && text->CalculateWidth(s, height) > maxWidth) {
        s.pop_back();
    }

    drawAlignedText(
        s,
        Bounds(bounds.width(), height, { bounds.left(), y }, Bounds::tl),
        height,
        Bounds::tl,
        Bounds::tl);
}

void ConsoleOverlay::render() {
    namespace fs = std::filesystem;

    drawFrame(m_bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    const Bounds body = m_bounds.inset(20.0f);
    const float titleHeight = 32.0f;
    const float textHeight = 18.0f;
    const float lineSpacing = textHeight * 1.4f;

    float y = body.top();
    drawLine("CONSOLE", body, y, titleHeight);
    y -= titleHeight * 1.5f;

    const auto relative = [&](const std::string &path) -> std::string {
        if (path.empty()) return "<NONE>";

        std::error_code ec;
        const fs::path rel = fs::relative(path, m_app->getDataRoot(), ec);
        return (ec || rel.empty() || *rel.begin() == "..")
            ? path
            : rel.generic_string();
    };

    std::vector<std::string> header = {
        "ENGINE: " + relative(m_app->getEngineScriptPath()),
        "THEME:  " + relative(m_app->getThemeScriptPath()),
        "[F5] CLOSE   [ENTER] RELOAD SCRIPTS   [UP]/[DOWN] SCROLL",
        "[PGUP]/[PGDN] PREVIOUS/NEXT ENGINE   [[]/[]] PREVIOUS/NEXT THEME",
        ""
    };

    for (const std::string &line : header) {
        drawLine(line, body, y, textHeight);
        y -= lineSpacing;
    }

    std::vector<std::string> log = splitLines(m_app->getScriptLog());
    if (log.empty()) {
        log.push_back("NO MESSAGES");
    }

    const int visibleLines =
        std::max(1, static_cast<int>((y - body.bottom()) / lineSpacing));
    const int maxOffset = std::max(0, static_cast<int>(log.size()) - visibleLines);
    m_scrollOffset = std::min(m_scrollOffset, maxOffset);

    const int first = m_scrollOffset;
    const int last = std::min(static_cast<int>(log.size()), first + visibleLines);
    for (int i = first; i < last; ++i) {
        drawLine(log[i], body, y, textHeight);
        y -= lineSpacing;
    }

    UiElement::render();
}
