#pragma once

#include <QString>
#include "plugin_export.h"

/**
 * Resolves GoDesk's writable Windows paths before RabbitCommon is initialized.
 *
 * The installed program is read-only by default.  We therefore prefer a
 * `data` directory beside the selected installation directory and fall back to
 * the user's local application data directory when that location is not
 * writable.  RabbitCommon's user document root is set to this resolved path so
 * its existing etc/share/db consumers keep their behavior.
 */
class PLUGIN_EXPORT GoDeskDataPaths
{
public:
    static GoDeskDataPaths& instance();

    /** Resolve paths and inject the user document root into RabbitCommon. */
    bool initialize(QString* error = nullptr);

    QString installRoot() const { return m_installRoot; }
    QString dataRoot() const { return m_dataRoot; }
    QString configRoot() const;
    QString dataStoreRoot() const;
    QString databaseRoot() const;
    QString pluginsRoot() const;
    QString bundledPluginsRoot() const;
    QString logRoot() const;
    QString cacheRoot() const;
    QString updateRoot() const;
    QString legacyRoot() const { return m_legacyRoot; }
    bool usingFallback() const { return m_usingFallback; }

    /** Copy existing data to a new writable root and make it active. */
    bool migrateTo(const QString& targetRoot, QString* error = nullptr);

private:
    GoDeskDataPaths() = default;
    GoDeskDataPaths(const GoDeskDataPaths&) = delete;
    GoDeskDataPaths& operator=(const GoDeskDataPaths&) = delete;

    bool isWritable(const QString& path) const;
    bool copyDirectory(const QString& source, const QString& target,
                       QString* error) const;
    QString markerFile() const;
    QString configuredRoot() const;
    bool writeConfiguredRoot(const QString& root, QString* error) const;
    void ensureLogConfiguration() const;

    QString m_installRoot;
    QString m_dataRoot;
    QString m_legacyRoot;
    bool m_usingFallback = false;
};

