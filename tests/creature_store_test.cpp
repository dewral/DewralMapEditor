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
    const QString monsterPath = sourceDirectory.filePath(QStringLiteral("item-monster.xml"));
    const QString itemNpcPath = sourceDirectory.filePath(QStringLiteral("item-npc.xml"));
    if (!writeFile(definitionPath,
                   QByteArrayLiteral("<npc name=\"Alice\"><look type=\"128\" "
                                     "head=\"10\" body=\"20\" legs=\"30\" "
                                     "feet=\"40\"/></npc>"))
        || !writeFile(indexPath,
                      QByteArrayLiteral("<npcs><npc name=\"Alice\" "
                                        "file=\"alice.xml\"/></npcs>"))
        || !writeFile(monsterPath,
                      QByteArrayLiteral("<monster name=\"Item Monster\">"
                                        "<look typeex=\"5710\" corpse=\"8311\"/>"
                                        "</monster>"))
        || !writeFile(itemNpcPath,
                      QByteArrayLiteral("<npc name=\"Item NPC\">"
                                        "<look lookitem=\"5710\"/></npc>"))) {
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
    bool passed = result.value(QStringLiteral("success")).toBool()
        && result.value(QStringLiteral("imported")).toInt() == 1
        && npc && npc->lookType == 128 && store.rowForCreature(
            QStringLiteral("Alice"), true) == 0;

    const QVariantMap itemResult = store.importOtFiles({monsterPath, itemNpcPath});
    const CreatureStore::CreatureType *monster = store.byNameAndType(
        QStringLiteral("Item Monster"), false);
    const CreatureStore::CreatureType *itemNpc = store.byNameAndType(
        QStringLiteral("Item NPC"), true);
    passed = passed && itemResult.value(QStringLiteral("success")).toBool()
        && itemResult.value(QStringLiteral("imported")).toInt() == 2
        && monster && monster->lookType == 0 && monster->lookItem == 5710
        && itemNpc && itemNpc->lookType == 0 && itemNpc->lookItem == 5710;

    // Import, editing and persistence must retain the server ID, not the corpse
    // ID or a client ID tied to the currently loaded assets.
    passed = passed && store.saveCreature(
        QStringLiteral("Item Monster"), QStringLiteral("Item Monster"), false,
        0, 5710, 0, 0, 0, 0);
    CreatureStore reloaded;
    passed = passed && reloaded.loadForDir(profile);
    monster = reloaded.byNameAndType(QStringLiteral("Item Monster"), false);
    itemNpc = reloaded.byNameAndType(QStringLiteral("Item NPC"), true);
    passed = passed && monster && monster->lookType == 0 && monster->lookItem == 5710
        && itemNpc && itemNpc->lookItem == 5710 && reloaded.count() == 3;

    QDir(profileDirectory).removeRecursively();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
