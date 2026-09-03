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
    const QString oldCommit = QStringLiteral("1111111111111111111111111111111111111111");
    const QString newCommit = QStringLiteral("2222222222222222222222222222222222222222");

    expect(UpdateVersion::decide(QStringLiteral("stable"), QStringLiteral("1.1.0"),
                                 newCommit, QStringLiteral("1.0.0"), oldCommit),
           Decision::Newer, "a greater stable version must update");
    expect(UpdateVersion::decide(QStringLiteral("stable"), QStringLiteral("0.9.0"),
                                 newCommit, QStringLiteral("1.0.0"), oldCommit),
           Decision::Current, "an older stable version must not update");
    expect(UpdateVersion::decide(QStringLiteral("stable"), QStringLiteral("1.0.0"),
                                 oldCommit.left(12), QStringLiteral("1.0"), oldCommit),
           Decision::Current, "matching abbreviated commits are current");
    expect(UpdateVersion::decide(QStringLiteral("stable"), QStringLiteral("1.0.0"),
                                 newCommit, QStringLiteral("1.0.0"), oldCommit),
           Decision::CompareCommits,
           "a republished stable version must compare commit ancestry");
    expect(UpdateVersion::decide(QStringLiteral("stable"), QStringLiteral("1.0.0"),
                                 newCommit, QStringLiteral("1.0.0"), QStringLiteral("unknown")),
           Decision::Current, "unknown local commits must not trigger a same-version update");
    expect(UpdateVersion::decide(QStringLiteral("development"),
                                 QStringLiteral("1.0.0-dev.20"), newCommit,
                                 QStringLiteral("1.0.0"), oldCommit),
           Decision::CompareCommits, "nightly commits must be ordered, not merely differ");

    return EXIT_SUCCESS;
}
