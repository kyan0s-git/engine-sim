#ifndef ATG_ENGINE_SIM_CONSOLE_OVERLAY_H
#define ATG_ENGINE_SIM_CONSOLE_OVERLAY_H

#include "ui_element.h"

#include <string>
#include <vector>

// Full window panel (toggled with F5) that shows the scripts that are
// loaded, the script compiler's messages and the keyboard shortcuts for
// switching engines and themes
class ConsoleOverlay : public UiElement {
    public:
        ConsoleOverlay();
        virtual ~ConsoleOverlay() override;

        virtual void render() override;

        void scroll(int lines);

    protected:
        void drawLine(const std::string &line, const Bounds &bounds, float y, float height);

        int m_scrollOffset;
};

#endif /* ATG_ENGINE_SIM_CONSOLE_OVERLAY_H */
