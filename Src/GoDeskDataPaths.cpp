#include "GoDeskDataPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QObject>

#include "RabbitCommonDir.h"

namespace {
const QString kMarkerGroup = QStringLiteral("GoDesk");
const QString kMarkerKey = QStringLiteral("DataRoot");
}

GoDeskDataPaths& GoDeskDataPaths::instance()
{
    static GoDeskDataPaths paths;
    return paths;
}

QString GoDeskDataPaths::configRoot() const
{
    return QDir(m_dataRoot).filePath(QStringLiteral("etc"));
}

QString GoDeskDataPaths::dataStoreRoot() const
{
    return QDir(m_dataRoot).filePath(QStringLiteral("share"));
}

QString GoDeskDataPaths::databaseRoot() const
{
    return QDir(dataStoreRoot()).filePath(QStringLiteral("db"));
}

QString GoDeskDataPaths::pluginsRoot() const
{
    return QDir(m_dataRoot).filePath(QStringLiteral("plugins"));
}

QString GoDeskDataPaths::bundledPluginsRoot() const
{
    return QDir(m_installRoot).filePath(QStringLiteral("plugins"));
}

QString GoDeskDataPaths::logRoot() const
{
    return QDir(m_dataRoot).filePath(QStringLiteral("log"));
}

QString GoDeskDataPaths::cacheRoot() const
{
    return QDir(m_dataRoot).filePath(QStringLiteral("cache"));
}

QString GoDeskDataPaths::updateRoot() const
{
    return QDir(m_dataRoot).filePath(QStringLiteral("update"));
}

QString GoDeskDataPaths::markerFile() const
{
    return QDir(QStandardPaths::writableLocation(
                    QStandardPaths::AppConfigLocation))
        .filePath(QStringLiteral("paths.ini"));
}

QString GoDeskDataPaths::configuredRoot() const
{
    const QString file = markerFile();
    if (!QFileInfo::exists(file))
        return QString();

    QSettings settings(file, QSettings::IniFormat);
    settings.beginGroup(kMarkerGroup);
    const QString root = QFileInfo(settings.value(kMarkerKey).toString())
                             .absoluteFilePath();
    settings.endGroup();
    return root;
}

bool GoDeskDataPaths::writeConfiguredRoot(const QString& root,
                                          QString* error) const
{
    const QString file = markerFile();
    QDir parent(QFileInfo(file).absolutePath());
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        if (error)
            *error = QObject::tr("无法创建数据目录配置位置：%1").arg(parent.absolutePath());
        return false;
    }

    QSettings settings(file, QSettings::IniFormat);
    settings.beginGroup(kMarkerGroup);
    settings.setValue(kMarkerKey, QDir(root).absolutePath());
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (error)
            *error = QObject::tr("无法保存数据目录配置：%1").arg(file);
        return false;
    }
    return true;
}

bool GoDeskDataPaths::isWritable(const QString& path) const
{
    QDir directory(path);
    if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
        return false;

    const QString probe = directory.filePath(QStringLiteral(".godesk-write-test"));
    QFile file(probe);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write("GoDesk");
    file.close();
    QFile::remove(probe);
    return true;
}

bool GoDeskDataPaths::copyDirectory(const QString& source,
                                    const QString& target,
                                    QString* error) const
{
    QDir sourceDir(source);
    if (!sourceDir.exists())
        return true;

    QDir targetDir(target);
    if (!targetDir.exists() && !targetDir.mkpath(QStringLiteral("."))) {
        if (error)
            *error = QObject::tr("无法创建迁移目录：%1").arg(target);
        return false;
    }

    const QFileInfoList entries = sourceDir.entryInfoList(
        QDir::NoDotAndDotDot | QDir::AllEntries,
        QDir::DirsFirst | QDir::Name);
    for (const QFileInfo& entry : entries) {
        const QString destination = targetDir.filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectory(entry.absoluteFilePath(), destination, error))
                return false;
            continue;
        }

        if (QFileInfo::exists(destination))
            continue;
        if (!QFile::copy(entry.absoluteFilePath(), destination)) {
            if (error)
                *error = QObject::tr("无法迁移文件：%1").arg(entry.absoluteFilePath());
            return false;
        }
    }
    return true;
}

void GoDeskDataPaths::ensureLogConfiguration() const
{
    const QString file = QDir(configRoot()).filePath(
        QCoreApplication::applicationName() + QStringLiteral("_logqt.ini"));
    if (!QFileInfo::exists(file)) {
        QDir(configRoot()).mkpath(QStringLiteral("."));
        QSaveFile save(file);
        if (save.open(QIODevice::WriteOnly | QIODevice::Text)) {
            const QByteArray content = QByteArrayLiteral("[Log]\nPath=")
                + QDir::toNativeSeparators(logRoot()).toUtf8()
                + QByteArrayLiteral("\n");
            save.write(content);
            save.commit();
        }
    }

    // RabbitCommon looks for the log configuration below the read-only
    // application root.  Point it to the writable user configuration before
    // CTools::Init() creates CLog.
    const QString userConfigure = QDir(m_dataRoot).filePath(
        QCoreApplication::applicationName() + QStringLiteral(".conf"));
    QSettings settings(userConfigure, QSettings::IniFormat);
    const QString configured = settings.value(QStringLiteral("Log/ConfigFile"))
                                   .toString();
    if (configured.isEmpty() || !QFileInfo::exists(configured)) {
        settings.setValue(QStringLiteral("Log/ConfigFile"), file);
        settings.sync();
    }
}

bool GoDeskDataPaths::initialize(QString* error)
{
    QDir applicationDir(QCoreApplication::applicationDirPath());
    m_installRoot = applicationDir.absolutePath();
    if (applicationDir.dirName().compare(QStringLiteral("bin"), Qt::CaseInsensitive) == 0)
        applicationDir.cdUp();
    m_installRoot = applicationDir.absolutePath();

    m_legacyRoot = QDir(QStandardPaths::writableLocation(
                            QStandardPaths::DocumentsLocation))
                       .filePath(QStringLiteral("Rabbit/GoDesk"));

    const QString configured = configuredRoot();
    if (!configured.isEmpty() && isWritable(configured)) {
        m_dataRoot = QDir(configured).absolutePath();
        m_usingFallback = !QDir(m_dataRoot).absolutePath().startsWith(
            QDir(m_installRoot).absolutePath(), Qt::CaseInsensitive);
    } else {
        const QString installData = QDir(m_installRoot).filePath(QStringLiteral("data"));
        if (isWritable(installData)) {
            m_dataRoot = QDir(installData).absolutePath();
            m_usingFallback = false;
        } else {
            m_dataRoot = QDir(QStandardPaths::writableLocation(
                                  QStandardPaths::AppLocalDataLocation))
                              .filePath(QStringLiteral("data"));
            if (!isWritable(m_dataRoot)) {
                if (error)
                    *error = QObject::tr("安装目录和用户数据目录均不可写：%1").arg(m_dataRoot);
                return false;
            }
            m_dataRoot = QDir(m_dataRoot).absolutePath();
            m_usingFallback = true;
        }

        const QDir data(m_dataRoot);
        if (data.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty()
            && QDir(m_legacyRoot).exists()
            && QDir(m_legacyRoot).absolutePath() != data.absolutePath()) {
            QString migrationError;
            if (!copyDirectory(m_legacyRoot, m_dataRoot, &migrationError) && error)
                *error = migrationError;
        }
        if (!writeConfiguredRoot(m_dataRoot, error))
            return false;
    }

    // RabbitCommon appends etc/share/db below this root.  Inject it before
    // CTools::Init() creates CDir and the logging subsystem.
    RabbitCommon::CDir::Instance()->SetDirUserDocument(m_dataRoot);
    QDir().mkpath(pluginsRoot());
    QDir().mkpath(cacheRoot());
    QDir().mkpath(updateRoot());
    QDir().mkpath(logRoot());
    ensureLogConfiguration();
    return true;
}

bool GoDeskDataPaths::migrateTo(const QString& targetRoot, QString* error)
{
    const QString target = QDir::cleanPath(QDir(targetRoot).absolutePath());
    const QString current = QDir::cleanPath(QDir(m_dataRoot).absolutePath());
    if (target.isEmpty()
        || target.compare(current, Qt::CaseInsensitive) == 0)
        return true;

    const QString sourcePrefix = current + QDir::separator();
    const QString targetPrefix = target + QDir::separator();
    if (target.startsWith(sourcePrefix, Qt::CaseInsensitive)
        || current.startsWith(targetPrefix, Qt::CaseInsensitive)) {
        if (error)
            *error = QObject::tr("新数据目录不能位于当前数据目录内部或反向包含当前目录。");
        return false;
    }
    if (!isWritable(target)) {
        if (error)
            *error = QObject::tr("数据目录不可写：%1").arg(target);
        return false;
    }
    const QFileInfoList existing = QDir(target).entryInfoList(
        QDir::NoDotAndDotDot | QDir::AllEntries);
    if (!existing.isEmpty()) {
        if (error)
            *error = QObject::tr("新数据目录必须为空，请选择一个专用文件夹：%1")
                         .arg(target);
        return false;
    }
    if (!copyDirectory(m_dataRoot, target, error))
        return false;
    if (!writeConfiguredRoot(target, error))
        return false;

    m_dataRoot = target;
    m_usingFallback = !target.startsWith(QDir(m_installRoot).absolutePath(),
                                         Qt::CaseInsensitive);
    QDir().mkpath(pluginsRoot());
    QDir().mkpath(cacheRoot());
    QDir().mkpath(updateRoot());
    QDir().mkpath(logRoot());
    ensureLogConfiguration();
    return true;
}

