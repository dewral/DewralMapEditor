#include "creaturestore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QUuid>

#include <cstdlib>

namespace {
bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir sourceDirectory;
    if (!sourceDirectory.isValid()) return EXIT_FAILURE;

    const QString definitionPath = sourceDirectory.filePath(QStringLiteral("alice.xml"));
    const QString indexPath = sourceDirectory.filePath(QStringLiteral("npcs.xml"));
    if (!writeFile(definitionPath,
                   QByteArrayLiteral("<npc name=\"Alice\"><look type=\"128\" "
                                     "head=\"10\" body=\"20\" legs=\"30\" "
                                     "feet=\"40\"/></npc>"))
        || !writeFile(indexPath,
                      QByteArrayLiteral("<npcs><npc name=\"Alice\" "
                                        "file=\"alice.xml\"/></npcs>"))) {
        return EXIT_FAILURE;
    }

    const QString profile = QStringLiteral("creature-store-test-%1")
                                .arg(QUuid::createUuid().toString(QUuid::Id128));
    const QString profileDirectory = QDir(QCoreApplication::applicationDirPath())
                                         .filePath(QStringLiteral("data/%1").arg(profile));
    CreatureStore store;
    store.loadForDir(profile);
    const QVariantMap result = store.importOtFile(indexPath);
    const CreatureStore::CreatureType *npc = store.byNameAndType(
        QStringLiteral("Alice"), true);
    const bool passed = result.value(QStringLiteral("success")).toBool()
        && result.value(QStringLiteral("imported")).toInt() == 1
        && npc && npc->lookType == 128 && store.rowForCreature(
            QStringLiteral("Alice"), true) == 0;

    QDir(profileDirectory).removeRecursively();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
