#include "updateversion.h"

#include <cstdlib>
#include <iostream>

namespace {
using UpdateVersion::Decision;

void expect(Decision actual, Decision expected, const char *message)
{
    if (actual == expected)
        return;
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}
}

int main()
{
    expect(UpdateVersion::decide(QStringLiteral("1.1.0"), QStringLiteral("1.0.0")),
           Decision::Newer, "a greater stable version must update");
    expect(UpdateVersion::decide(QStringLiteral("0.9.0"), QStringLiteral("1.0.0")),
           Decision::Current, "an older stable version must not update");
    expect(UpdateVersion::decide(QStringLiteral("1.0.0"), QStringLiteral("1.0")),
           Decision::Current, "equivalent stable versions are current");
    expect(UpdateVersion::decide(QStringLiteral("v1.1.0"), QStringLiteral("1.0.0")),
           Decision::Newer, "a prefixed stable version must update");
    expect(UpdateVersion::decide(QStringLiteral("invalid"), QStringLiteral("1.0.0")),
           Decision::Current, "invalid release versions must not trigger an update");

    return EXIT_SUCCESS;
}
