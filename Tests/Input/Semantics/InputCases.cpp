#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Input.hpp"
#include <lwcgl/lwcgl.h>

namespace Tests {
namespace {

class KeyboardCase final : public Testing::Case {
public:
    std::string_view name() const override { return "input/keyboard"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Input::reset();
        return Testing::require(Input::key(nullptr) == Input::InvalidKey, "null key name accepted", error) &&
            Testing::require(Input::key("W") == Keyboard.KEY_W, "W key mapping mismatch", error) &&
            Testing::require(!Input::keyDown(Input::InvalidKey), "invalid key reported down", error) &&
            Testing::require(!Input::keyPressed(Input::InvalidKey), "invalid key reported pressed", error) &&
            Testing::require(!Input::keyReleased(Input::InvalidKey), "invalid key reported released", error);
    }
};

class MouseCase final : public Testing::Case {
public:
    std::string_view name() const override { return "input/mouse"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Input::reset();
        return Testing::require(Input::button(nullptr) == Input::InvalidButton, "null button name accepted", error) &&
            Testing::require(!Input::buttonDown(Input::InvalidButton), "invalid button reported down", error) &&
            Testing::require(!Input::buttonPressed(Input::InvalidButton), "invalid button reported pressed", error) &&
            Testing::require(!Input::buttonReleased(Input::InvalidButton), "invalid button reported released", error);
    }
};

class PointerCase final : public Testing::Case {
public:
    std::string_view name() const override { return "input/pointer"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Input::reset();
        const Input::Pointer pointer = Input::pointer();
        return Testing::require(pointer.x == 0 && pointer.y == 0 && pointer.dx == 0 && pointer.dy == 0,
                                "reset pointer is not zero", error) &&
            Testing::require(pointer.wheel == 0 && !pointer.captured, "reset pointer flags are not zero", error);
    }
};

} // namespace

void registerInput(Testing::Runner& runner)
{
    runner.add<KeyboardCase>();
    runner.add<MouseCase>();
    runner.add<PointerCase>();
}

} // namespace Tests
