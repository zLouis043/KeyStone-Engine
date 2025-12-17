#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#include <keystone.h>

int main(int argc, char** argv) {
    doctest::Context context;

    KS_PROFILE_BEGIN_SESSION("KeyStone Tests", "profiling/test_run.json");

    int res;
    {
        KS_PROFILE_SCOPE("RunAllTests");
        res = context.run();
    }

    context.applyCommandLine(argc, argv);

    KS_PROFILE_END_SESSION();

    if (context.shouldExit())
        return res;

    return res;
}