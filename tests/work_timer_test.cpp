#include "worktimerservice.h"
#include "documentmanager.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QUuid>

static bool require(bool value, const char *message)
{
    if (!value) qCritical() << message;
    return value;
}
class WorkTimerServiceTest {
public:
 static bool clocks(DocumentManager &documents,const QString &folder) {
    qint64 monotonic=1000,utc=QDateTime::currentMSecsSinceEpoch();
    WorkTimerService timer(&documents,nullptr,folder,{{[&]{return monotonic;}},{[&]{return utc;}}});
    timer.setIdlePauseMinutes(0);
    if(!timer.startSession("Clock test"))return false;
    monotonic+=5000;utc+=3600000;
    if(!require(timer.elapsedSeconds()==5,"Forward clock change added fake working time"))return false;
    timer.pauseSession();if(!timer.resumeSession())return false;
    monotonic+=3000;utc-=7200000;
    if(!require(timer.elapsedSeconds()==8,"Backward clock change stopped elapsed time"))return false;
    timer.pauseSession();timer.finishSession();
    if(!require(timer.history().first().toMap().value("durationSeconds").toLongLong()==8,"Persisted duration uses wall time"))return false;
    timer.setIdlePauseMinutes(1);if(!timer.startSession("Idle clock"))return false;
    utc+=86400000;timer.tick();
    if(!require(timer.running(),"Wall clock triggered AFK"))return false;
    monotonic+=65000;timer.tick();
    if(!require(!timer.running()&&timer.elapsedSeconds()==60,"AFK threshold did not use monotonic elapsed time"))return false;
    timer.finishSession();timer.setIdlePauseMinutes(0);timer.setPomodoroEnabled(true);
    timer.setPomodoroWorkMinutes(1);timer.setPomodoroBreakMinutes(1);
    if(!timer.startSession("Pomodoro clock"))return false;
    monotonic+=60000;timer.tick();
    if(!require(timer.pomodoroBreakActive()&&timer.pomodoroBreakRemaining()==60,"Pomodoro failed to start a monotonic break"))return false;
    utc-=172800000;monotonic+=30000;
    if(!require(timer.pomodoroBreakRemaining()==30,"Wall clock changed the Pomodoro break"))return false;
    monotonic+=30000;timer.tick();
    return require(!timer.pomodoroBreakActive(),"Monotonic Pomodoro break did not finish");
 }
};
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("DME-Tests");
    QCoreApplication::setApplicationName("Timer-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create test directory")) return 1;
    qputenv("DME_RECOVERY_DIR", directory.filePath("recovery").toUtf8());
    DocumentManager documents;
    if (!require(documents.current()->newMap(1024,1024,860,2,22), "Could not create timer map")) return 1;
    QString imported;
    {
        WorkTimerService timer(&documents, nullptr, directory.path());
        if (!require(timer.startSession("Blackridge Mine") && timer.taskIcon() == "mapping", "Legacy task must have default icon")) return 1;
        timer.pauseSession();
        if (!require(timer.setTaskIcon("Blackridge Mine", "mine") && timer.taskIcon() == "mine", "Preset icon was not applied")) return 1;
        if (!require(!timer.setTaskIcon("missing task", "town") && !timer.setTaskIcon("Blackridge Mine", "invalid"), "Invalid icon request was accepted")) return 1;
        QImage image(512,256,QImage::Format_ARGB32); image.fill(Qt::cyan);
        const QString source=directory.filePath("source.png");
        if (!require(image.save(source) && timer.importTaskIcon("Blackridge Mine",QUrl::fromLocalFile(source)), "Custom image import failed")) return 1;
        imported=QUrl(timer.taskIcon()).toLocalFile();
        const QImage stored(imported);
        if (!require(stored.size()==QSize(128,64) && stored.pixelColor(2,2)==QColor(Qt::cyan), "Imported icon lost its aspect ratio or pixels")) return 1;
        QFile::remove(source);
        if (!require(QFileInfo::exists(imported), "Icon must remain after source removal")) return 1;
        QFile bad(directory.filePath("broken.png"));
        if (!require(bad.open(QIODevice::WriteOnly), "Could not create invalid image fixture")) return 1;
        bad.write("invalid image"); bad.close();
        if (!require(!timer.importTaskIcon("Blackridge Mine",QUrl::fromLocalFile(bad.fileName())) && QUrl(timer.taskIcon()).toLocalFile()==imported, "Failed import replaced the existing icon")) return 1;
        if (!require(timer.finishSession() && timer.startSession("Temple exterior") && timer.setTaskIcon("Temple exterior", "town"), "Could not create a second task")) return 1;
        timer.pauseSession();
    }
    {
        WorkTimerService timer(&documents, nullptr, directory.path());
        if (!require(timer.taskIcon()=="town" && timer.taskTimers().size()==2, "Task icon did not survive restart")) return 1;
        if (!require(timer.taskIconFor("Blackridge Mine")==QUrl::fromLocalFile(imported).toString(), "Custom icon was not persisted")) return 1;
        if (!require(timer.switchTask("Blackridge Mine") && QUrl(timer.taskIcon()).toLocalFile()==imported, "Switching tasks lost its icon")) return 1;
        timer.pauseSession();
        if (!require(timer.setTaskIcon("Blackridge Mine", "dungeon") && !QFileInfo::exists(imported), "Replacing a custom icon did not clean up its managed copy")) return 1;
        if (!require(timer.removeTaskTimer("Blackridge Mine") && timer.taskIconFor("Blackridge Mine")=="mapping" && timer.taskIconFor("Temple exterior")=="town", "Deleting a task damaged other task icons")) return 1;
    }
    {
        WorkTimerService timer(&documents, nullptr, directory.path());
        if (!require(timer.taskTimers().size()==1 && timer.taskIconFor("Temple exterior")=="town", "Task removal was not persisted")) return 1;
    }
    if (!WorkTimerServiceTest::clocks(documents,directory.filePath("clock-tests"))) return 1;
    qInfo() << "Task icons: presets, import, restart, switching, invalid images and cleanup passed";
    return 0;
}
