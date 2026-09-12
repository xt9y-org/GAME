#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

namespace Testing {

void registerAll(Runner& runner)
{
    Tests::registerCore(runner);
    Tests::registerInput(runner);
    Tests::registerCamera(runner);
    Tests::registerPhysics(runner);
    Tests::registerAudio(runner);
    Tests::registerAnimation(runner);
    Tests::registerFont(runner);
    Tests::registerInteractivity(runner);
    Tests::registerModels(runner);
    Tests::registerGaussianSplat(runner);
    Tests::registerHierarchy(runner);
    Tests::registerVisibility(runner);
    Tests::registerSettings(runner);
    Tests::registerScenes(runner);
    Tests::registerUi(runner);
}

} // namespace Testing
