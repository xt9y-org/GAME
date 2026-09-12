#ifndef GAME_TESTS_HARNESS_TESTING_HPP
#define GAME_TESTS_HARNESS_TESTING_HPP

#include "Ecs/Ecs.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Testing {

class RendererFixture;

enum class Kind {
    Functional,
    Visual,
};

struct Context {
    Context();
    ~Context();

    std::string renderer = "rasterizer";
    Ecs::World world;
    std::unique_ptr<RendererFixture> graphics;
};

class Case {
public:
    virtual ~Case() = default;

    virtual std::string_view name() const = 0;
    virtual Kind kind() const { return Kind::Functional; }
    virtual std::size_t frameCount() const { return kind() == Kind::Visual ? 8u : 0u; }

    virtual bool setup(Context&, std::string&) { return true; }
    virtual bool update(Context&, double, std::string&) { return true; }
    virtual bool verify(Context&, std::string&) { return true; }
    virtual void shutdown(Context&) {}
};

class Runner {
public:
    void add(std::unique_ptr<Case> test);

    template <typename T, typename... Args>
    T& add(Args&&... args)
    {
        auto test = std::make_unique<T>(std::forward<Args>(args)...);
        T& reference = *test;
        add(std::move(test));
        return reference;
    }

    int run(int argc, char **argv);

private:
    std::vector<std::unique_ptr<Case>> cases_;
};

void registerAll(Runner& runner);

bool near(float a, float b, float epsilon = 1.0e-4f);
bool near(double a, double b, double epsilon = 1.0e-8);
bool require(bool condition, std::string_view message, std::string& error);

} // namespace Testing

#endif
