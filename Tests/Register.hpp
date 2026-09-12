#ifndef GAME_TESTS_REGISTER_HPP
#define GAME_TESTS_REGISTER_HPP

namespace Testing { class Runner; }

namespace Tests {
void registerCore(Testing::Runner&);
void registerInput(Testing::Runner&);
void registerCamera(Testing::Runner&);
void registerPhysics(Testing::Runner&);
void registerAudio(Testing::Runner&);
void registerAnimation(Testing::Runner&);
void registerFont(Testing::Runner&);
void registerInteractivity(Testing::Runner&);
void registerModels(Testing::Runner&);
void registerGaussianSplat(Testing::Runner&);
void registerHierarchy(Testing::Runner&);
void registerVisibility(Testing::Runner&);
void registerSettings(Testing::Runner&);
void registerScenes(Testing::Runner&);
void registerUi(Testing::Runner&);
} // namespace Tests

#endif
