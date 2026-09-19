#include <Debugging/Layout.hpp>

#include <cassert>

int main()
{
    using namespace Debugging::Layout;

    assert(performanceHeight(false) == PerformanceHeight);
    assert(performanceHeight(true) == PerformanceHeight + GiPerformanceHeight);

    constexpr float menu_height = 20.0f;
    const float without_performance = CameraPanelTop(false, false, menu_height);
    assert(without_performance == menu_height + PanelPadding);

    const float normal = CameraPanelTop(true, false, menu_height);
    assert(normal == menu_height + PanelPadding + PerformanceHeight + PanelSpacing);

    const float gi = CameraPanelTop(true, true, menu_height);
    assert(gi == menu_height + PanelPadding + PerformanceHeight + GiPerformanceHeight + PanelSpacing);

    return 0;
}
