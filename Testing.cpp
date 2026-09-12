#include "Tests/Harness/Testing.hpp"

int main(int argc, char **argv)
{
    Testing::Runner runner;
    Testing::registerAll(runner);
    return runner.run(argc, argv);
}
