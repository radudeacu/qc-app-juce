#pragma once

#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

/** Minimal self-registering test harness.

    Hand-rolled rather than pulled from a package so the engine's test suite builds
    from a clean clone with no network access and no third-party dependency to
    justify. If the suite outgrows this, Catch2 is the obvious replacement.
*/
namespace qctest
{
    struct TestCase
    {
        std::string name;
        std::function<void()> body;
    };

    inline std::vector<TestCase>& registry()
    {
        static std::vector<TestCase> tests;
        return tests;
    }

    struct Registrar
    {
        Registrar (const std::string& name, std::function<void()> body)
        {
            registry().push_back ({ name, std::move (body) });
        }
    };

    struct Failure
    {
        std::string message;
    };

    inline void fail (const std::string& message)
    {
        throw Failure { message };
    }

    inline void check (bool condition, const std::string& message)
    {
        if (! condition)
            fail (message);
    }

    inline void checkClose (double actual, double expected, double tolerance, const std::string& what)
    {
        if (! (std::abs (actual - expected) <= tolerance))
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision (6)
                   << what << ": expected " << expected << " +/-" << tolerance
                   << ", got " << actual;
            fail (stream.str());
        }
    }

    inline int runAll()
    {
        int failures = 0;

        for (const auto& test : registry())
        {
            try
            {
                test.body();
                std::cout << "[  ok  ] " << test.name << "\n";
            }
            catch (const Failure& failure)
            {
                std::cout << "[ FAIL ] " << test.name << "\n           " << failure.message << "\n";
                ++failures;
            }
            catch (const std::exception& error)
            {
                std::cout << "[ FAIL ] " << test.name << "\n           unexpected exception: "
                          << error.what() << "\n";
                ++failures;
            }
        }

        std::cout << "\n" << (registry().size() - static_cast<std::size_t> (failures))
                  << " / " << registry().size() << " tests passed\n";

        return failures == 0 ? 0 : 1;
    }
}

#define QC_TEST(name)                                                                    \
    static void name();                                                                  \
    static const qctest::Registrar registrar_##name { #name, name };                     \
    static void name()
