#ifndef ATG_ENGINE_SIM_UI_WIDGETS_H
#define ATG_ENGINE_SIM_UI_WIDGETS_H

#include <imgui.h>

// Instrument widgets drawn with Dear ImGui's draw lists
namespace ui {

    struct GaugeBand {
        float start;
        float end;
        ImU32 color;
    };

    struct GaugeStyle {
        float min = 0;
        float max = 1;
        float majorStep = 0.1f;
        int minorTicks = 4;
        const char *format = "%.0f";     // tick labels
        float labelScale = 1.0f;          // tick label = value * labelScale
        const char *valueFormat = "%.0f";
        const char *unit = "";
        const GaugeBand *bands = nullptr;
        int bandCount = 0;
    };

    // Round dial with a needle. The needle follows the value with a critically
    // damped response that doesn't depend on the frame rate.
    void RoundGauge(const char *label, float value, const GaugeStyle &style, float size);

    // Horizontal bar with a label and the value
    void BarMeter(
        const char *label,
        float value,
        float min,
        float max,
        const char *format,
        ImU32 color = 0);

    // Value with a label in a fixed width table-like row
    void ValueRow(const char *label, const char *format, ...);

    // Frame-rate independent smoothing of a displayed value; `id` identifies
    // the value between frames
    float Smooth(ImGuiID id, float target, float timeConstant = 0.08f);

    // Accent colors
    ImU32 AccentColor(float alpha = 1.0f);
    ImU32 WarningColor(float alpha = 1.0f);
    ImU32 DangerColor(float alpha = 1.0f);
    ImU32 OkColor(float alpha = 1.0f);

    // Tooltip on the previous item
    void HelpMarker(const char *text);

} /* namespace ui */

#endif /* ATG_ENGINE_SIM_UI_WIDGETS_H */
