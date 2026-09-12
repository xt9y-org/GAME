#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Models/GaussianSplat.hpp"

#include <cmath>

namespace Tests {
namespace {

class GaussianShCase final : public Testing::Case {
public:
    std::string_view name() const override { return "models/gaussian-splat-sh"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Models::GaussianSplat::Splat splat;
        splat.spherical_harmonics.resize(16u);
        for (std::size_t index = 0u; index < splat.spherical_harmonics.size(); ++index) {
            const float value = static_cast<float>(index + 1u) * 0.01f;
            splat.spherical_harmonics[index] = {value, value * 0.5f, value * 0.25f};
        }

        Models::Vec3 previous{};
        for (std::uint32_t degree = 0u; degree <= 3u; ++degree) {
            const Models::Vec3 color = Models::GaussianSplat::evaluateSphericalHarmonics(
                splat, degree, {0.3f, 0.4f, 0.5f});
            if (!Testing::require(
                    std::isfinite(color.x) && std::isfinite(color.y) && std::isfinite(color.z),
                    "Gaussian SH produced non-finite color", error))
                return false;
            if (degree > 0u && !Testing::require(
                    color.x != previous.x || color.y != previous.y || color.z != previous.z,
                    "Gaussian SH degree did not affect output", error))
                return false;
            previous = color;
        }

        const Models::Vec3 rotated = Models::GaussianSplat::evaluateSphericalHarmonics(
            splat, 3u, {-0.4f, 0.3f, 0.5f});
        return Testing::require(
            rotated.x != previous.x || rotated.y != previous.y || rotated.z != previous.z,
            "Gaussian SH direction did not affect output", error);
    }
};

} // namespace

void registerGaussianSplat(Testing::Runner& runner)
{
    runner.add<GaussianShCase>();
}

} // namespace Tests
