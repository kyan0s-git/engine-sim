#include "../../include/ui/ui_widgets.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace ui {

namespace {

    constexpr float Pi = 3.14159265358979f;

    // Dial from 225 degrees (lower left) clockwise to -45 degrees
    constexpr float AngleMin = Pi * 0.75f;
    constexpr float AngleMax = Pi * 2.25f;

    float angleOf(float value, float min, float max) {
        const float s = (max > min) ? std::clamp((value - min) / (max - min), 0.0f, 1.0f) : 0.0f;
        return AngleMin + s * (AngleMax - AngleMin);
    }

    ImVec2 polar(const ImVec2 &center, float radius, float angle) {
        return ImVec2(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
    }

} /* namespace */

float Smooth(ImGuiID id, float target, float timeConstant) {
    ImGuiStorage *storage = ImGui::GetStateStorage();
    if (!std::isfinite(target)) target = 0.0f;

    // Exact first-order response: independent of the frame rate and can't
    // overshoot
    const float dt = std::min(ImGui::GetIO().DeltaTime, 0.25f);
    float *value = storage->GetFloatRef(id, target);
    const float s = 1.0f - std::exp(-dt / std::max(timeConstant, 1E-4f));
    *value += (target - *value) * s;

    return *value;
}

ImU32 AccentColor(float alpha) { return ImGui::GetColorU32(ImVec4(0.96f, 0.50f, 0.16f, alpha)); }
ImU32 WarningColor(float alpha) { return ImGui::GetColorU32(ImVec4(0.95f, 0.78f, 0.20f, alpha)); }
ImU32 DangerColor(float alpha) { return ImGui::GetColorU32(ImVec4(0.90f, 0.25f, 0.22f, alpha)); }
ImU32 OkColor(float alpha) { return ImGui::GetColorU32(ImVec4(0.30f, 0.78f, 0.45f, alpha)); }

void RoundGauge(const char *label, float value, const GaugeStyle &style, float size) {
    ImGui::PushID(label);
    const ImGuiID id = ImGui::GetID("##needle");
    const float shown = Smooth(id, value, 0.06f);

    ImDrawList *draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 center(origin.x + size * 0.5f, origin.y + size * 0.5f);
    const float radius = size * 0.46f;

    const ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 dim = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const ImU32 face = ImGui::GetColorU32(ImGuiCol_FrameBg);

    draw->AddCircleFilled(center, radius, face, 64);

    // Colored bands
    for (int i = 0; i < style.bandCount; ++i) {
        const GaugeBand &band = style.bands[i];
        draw->PathArcTo(
            center,
            radius * 0.90f,
            angleOf(band.start, style.min, style.max),
            angleOf(band.end, style.min, style.max),
            24);
        draw->PathStroke(band.color, 0, radius * 0.07f);
    }

    // Ticks and labels
    const float range = style.max - style.min;
    const float step = (style.majorStep > 0) ? style.majorStep : range / 10;
    const int majorCount = (step > 0) ? (int)std::floor(range / step + 1E-3f) : 0;
    const float fontSize = std::max(9.0f, size * 0.075f);
    for (int i = 0; i <= majorCount; ++i) {
        const float v = style.min + i * step;
        const float a = angleOf(v, style.min, style.max);
        draw->AddLine(polar(center, radius * 0.97f, a), polar(center, radius * 0.80f, a), text, 2.0f);

        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), style.format, v * style.labelScale);
        const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buffer);
        const ImVec2 p = polar(center, radius * 0.65f, a);
        draw->AddText(ImGui::GetFont(), fontSize, ImVec2(p.x - textSize.x * 0.5f, p.y - textSize.y * 0.5f), dim, buffer);

        if (i < majorCount) {
            for (int j = 1; j <= style.minorTicks; ++j) {
                const float vm = v + step * j / (style.minorTicks + 1);
                const float am = angleOf(vm, style.min, style.max);
                draw->AddLine(polar(center, radius * 0.97f, am), polar(center, radius * 0.88f, am), dim, 1.0f);
            }
        }
    }

    // Needle
    const float a = angleOf(shown, style.min, style.max);
    draw->AddLine(polar(center, -radius * 0.12f, a), polar(center, radius * 0.92f, a), AccentColor(), std::max(2.0f, size * 0.018f));
    draw->AddCircleFilled(center, size * 0.04f, AccentColor(), 24);

    // Label and value
    const float labelSize = std::max(9.0f, size * 0.07f);
    const ImVec2 labelExtent = ImGui::GetFont()->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, label);
    draw->AddText(ImGui::GetFont(), labelSize,
        ImVec2(center.x - labelExtent.x * 0.5f, center.y + radius * 0.20f), dim, label);

    char valueText[48];
    std::snprintf(valueText, sizeof(valueText), style.valueFormat, value);
    char full[80];
    std::snprintf(full, sizeof(full), "%s %s", valueText, style.unit);
    const float valueSize = std::max(11.0f, size * 0.11f);
    const ImVec2 valueExtent = ImGui::GetFont()->CalcTextSizeA(valueSize, FLT_MAX, 0.0f, full);
    draw->AddText(ImGui::GetFont(), valueSize,
        ImVec2(center.x - valueExtent.x * 0.5f, center.y + radius * 0.62f), text, full);

    ImGui::Dummy(ImVec2(size, size));
    ImGui::PopID();
}

void BarMeter(const char *label, float value, float min, float max, const char *format, ImU32 color) {
    ImGui::PushID(label);
    const float shown = Smooth(ImGui::GetID("##bar"), value, 0.08f);
    const float s = (max > min) ? std::clamp((shown - min) / (max - min), 0.0f, 1.0f) : 0.0f;

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), format, value);

    ImGui::TextUnformatted(label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.42f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color != 0 ? color : AccentColor());
    ImGui::ProgressBar(s, ImVec2(-FLT_MIN, 0), buffer);
    ImGui::PopStyleColor();
    ImGui::PopID();
}

void ValueRow(const char *label, const char *format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", label);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(buffer);
}

void HelpMarker(const char *text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("%s", text);
    }
}

} /* namespace ui */
