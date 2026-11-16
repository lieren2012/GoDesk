#ifndef GODESKUPDATEDIALOG_H
#define GODESKUPDATEDIALOG_H

#include <QDialog>
#include <QFile>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QUrl>

class QLabel;
class QPushButton;
class QProgressBar;
class QNetworkReply;

/**
 * Windows-only GoDesk updater.
 *
 * The updater talks to the formal GoDesk GitHub Releases API, ignores draft and
 * prerelease entries, and shows the exact release, package, destination and
 * SHA-256 before launching the installer.
 */
class GoDeskUpdateDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit GoDeskUpdateDialog(QWidget* parent = nullptr,
                                bool showIfUpToDate = true);
    ~GoDeskUpdateDialog() override;

    void StartCheck();

private Q_SLOTS:
    void slotCheckFinished();
    void slotDownloadReadyRead();
    void slotDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void slotDownloadFinished();
    void slotDownloadError(QNetworkReply::NetworkError error);
    void slotDownloadClicked();

private:
    static QString NormalizeVersion(const QString& version);
    static int CompareVersions(const QString& left, const QString& right);

    void SetStatus(const QString& text);
    void SetError(const QString& text);
    void ShowResult(bool showIfUpToDate);
    bool SelectWindowsAsset(const QJsonObject& release);
    bool VerifyDownloadedFile(QString& actualSha256);
    void UpdateDetails();
    void CloseReply();

    QNetworkAccessManager m_Network;
    QPointer<QNetworkReply> m_CheckReply;
    QPointer<QNetworkReply> m_DownloadReply;
    QFile m_DownloadFile;

    QUrl m_ApiUrl;
    QUrl m_ReleaseUrl;
    QUrl m_DownloadUrl;
    QString m_CurrentVersion;
    QString m_LatestVersion;
    QString m_DownloadFileName;
    QString m_DownloadPath;
    QString m_ExpectedSha256;
    QString m_ActualSha256;
    bool m_ShowIfUpToDate;

    QLabel* m_CurrentVersionLabel;
    QLabel* m_LatestVersionLabel;
    QLabel* m_SourceLabel;
    QLabel* m_FileLabel;
    QLabel* m_PathLabel;
    QLabel* m_Sha256Label;
    QLabel* m_BehaviorLabel;
    QLabel* m_StatusLabel;
    QProgressBar* m_ProgressBar;
    QPushButton* m_DownloadButton;
    QPushButton* m_CloseButton;
};

#endif // GODESKUPDATEDIALOG_H
