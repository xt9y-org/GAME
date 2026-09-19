#include <Debugging/Layout.hpp>

#include <cassert>

int main()
{
    using namespace Debugging::Layout;

    assert(performanceHeight(false) == PerformanceHeight);
    assert(performanceHeight(true) == PerformanceHeight + GlobalIlluminationPerformanceHeight);

    constexpr float menu_height = 24.0f;
    assert(CameraPanelTop(false, false, menu_height) == menu_height + PanelPadding);
    assert(CameraPanelTop(false, true, menu_height) == menu_height + PanelPadding);
    assert(
        CameraPanelTop(true, false, menu_height) ==
        menu_height + PanelPadding + PerformanceHeight + PanelSpacing
    );
    assert(
        CameraPanelTop(true, true, menu_height) ==
        menu_height + PanelPadding + PerformanceHeight +
            GlobalIlluminationPerformanceHeight + PanelSpacing
    );

    return 0;
}
