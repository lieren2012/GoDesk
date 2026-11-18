#include "GoDeskUpdateDialog.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>

#include "GoDeskDataPaths.h"

namespace {
const QUrl kGoDeskReleaseApi(
    QStringLiteral("https://api.github.com/repos/lieren2012/GoDesk/releases?per_page=20"));
const QString kWindowsAssetMarker = QStringLiteral("Windows_Setup.exe");
}

GoDeskUpdateDialog::GoDeskUpdateDialog(QWidget* parent, bool showIfUpToDate)
    : QDialog(parent),
      m_ApiUrl(kGoDeskReleaseApi),
      m_ShowIfUpToDate(showIfUpToDate),
      m_CurrentVersionLabel(new QLabel(this)),
      m_LatestVersionLabel(new QLabel(this)),
      m_SourceLabel(new QLabel(this)),
      m_FileLabel(new QLabel(this)),
      m_PathLabel(new QLabel(this)),
      m_Sha256Label(new QLabel(this)),
      m_BehaviorLabel(new QLabel(this)),
      m_StatusLabel(new QLabel(this)),
      m_ProgressBar(new QProgressBar(this)),
      m_DownloadButton(new QPushButton(tr("下载并安装"), this)),
      m_CloseButton(new QPushButton(tr("关闭"), this))
{
    setWindowTitle(tr("GoDesk 更新"));
    setMinimumWidth(720);
    setAttribute(Qt::WA_DeleteOnClose);

    for (QLabel* label : {m_SourceLabel, m_FileLabel, m_PathLabel,
                          m_Sha256Label, m_BehaviorLabel, m_StatusLabel}) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }

    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(tr("当前版本"), m_CurrentVersionLabel);
    form->addRow(tr("最新版本"), m_LatestVersionLabel);
    form->addRow(tr("更新来源"), m_SourceLabel);
    form->addRow(tr("下载文件"), m_FileLabel);
    form->addRow(tr("保存路径"), m_PathLabel);
    form->addRow(tr("SHA-256"), m_Sha256Label);
    form->addRow(tr("完成行为"), m_BehaviorLabel);

    m_ProgressBar->setRange(0, 100);
    m_ProgressBar->hide();

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(m_DownloadButton);
    buttons->addWidget(m_CloseButton);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_ProgressBar);
    layout->addWidget(m_StatusLabel);
    layout->addLayout(buttons);

    connect(m_DownloadButton, &QPushButton::clicked,
            this, &GoDeskUpdateDialog::slotDownloadClicked);
    connect(m_CloseButton, &QPushButton::clicked, this, &QDialog::close);

    m_CurrentVersion = NormalizeVersion(QCoreApplication::applicationVersion());
    if (m_CurrentVersion.isEmpty())
        m_CurrentVersion = QStringLiteral("0.1.12");
    m_CurrentVersionLabel->setText(m_CurrentVersion);
    m_LatestVersionLabel->setText(tr("检查中..."));
    m_SourceLabel->setText(tr("GitHub Release API：%1").arg(m_ApiUrl.toString()));
    m_FileLabel->setText(tr("尚未选择安装包"));
    m_PathLabel->setText(tr("尚未下载"));
    m_Sha256Label->setText(tr("下载后计算"));
    m_BehaviorLabel->setText(
        tr("下载完成后自动启动 Windows 安装器；GoDesk 不直接替换当前程序文件。"));
    SetStatus(tr("准备检查正式 Release..."));
}

GoDeskUpdateDialog::~GoDeskUpdateDialog()
{
    CloseReply();
    if (m_DownloadFile.isOpen())
        m_DownloadFile.close();
}

QString GoDeskUpdateDialog::NormalizeVersion(const QString& version)
{
    QString normalized = version.trimmed();
    while (normalized.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        normalized.remove(0, 1);
    const int suffix = normalized.indexOf(QRegularExpression(QStringLiteral("[^0-9.]")));
    if (suffix >= 0)
        normalized.truncate(suffix);
    return normalized;
}

int GoDeskUpdateDialog::CompareVersions(const QString& left, const QString& right)
{
    const QStringList leftParts = NormalizeVersion(left).split(QLatin1Char('.'));
    const QStringList rightParts = NormalizeVersion(right).split(QLatin1Char('.'));
    const int count = qMax(leftParts.size(), rightParts.size());
    for (int i = 0; i < count; ++i) {
        bool leftOk = false;
        bool rightOk = false;
        const int leftValue = i < leftParts.size() ? leftParts.at(i).toInt(&leftOk) : 0;
        const int rightValue = i < rightParts.size() ? rightParts.at(i).toInt(&rightOk) : 0;
        if (!leftOk || !rightOk)
            return QString::compare(left, right, Qt::CaseInsensitive);
        if (leftValue != rightValue)
            return leftValue < rightValue ? -1 : 1;
    }
    return 0;
}

void GoDeskUpdateDialog::StartCheck()
{
    if (m_CheckReply || m_DownloadReply)
        return;

    m_CurrentVersion = NormalizeVersion(QCoreApplication::applicationVersion());
    m_CurrentVersionLabel->setText(m_CurrentVersion);
    SetStatus(tr("正在读取正式 GitHub Release..."));
    m_DownloadButton->setEnabled(false);
    if (m_ShowIfUpToDate)
        show();

    QNetworkRequest request(m_ApiUrl);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent",
                         QStringLiteral("GoDesk/%1").arg(m_CurrentVersion).toUtf8());
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
#endif
    m_CheckReply = m_Network.get(request);
    connect(m_CheckReply, &QNetworkReply::finished,
            this, &GoDeskUpdateDialog::slotCheckFinished);
}

void GoDeskUpdateDialog::slotCheckFinished()
{
    QNetworkReply* reply = m_CheckReply;
    m_CheckReply = nullptr;
    if (!reply)
        return;

    const QByteArray payload = reply->readAll();
    const auto error = reply->error();
    const QString errorString = reply->errorString();
    reply->deleteLater();
    if (error != QNetworkReply::NoError) {
        SetError(tr("读取 Release 失败：%1").arg(errorString));
        show();
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isArray()) {
        SetError(tr("GitHub 返回的 Release 数据格式无效。"));
        show();
        return;
    }

    QJsonObject bestRelease;
    QString bestVersion;
    for (const QJsonValue& value : document.array()) {
        const QJsonObject release = value.toObject();
        if (release.value(QStringLiteral("draft")).toBool()
            || release.value(QStringLiteral("prerelease")).toBool()) {
            continue;
        }
        const QString version = NormalizeVersion(
            release.value(QStringLiteral("tag_name")).toString());
        if (version.isEmpty() || !SelectWindowsAsset(release))
            continue;
        if (bestVersion.isEmpty() || CompareVersions(version, bestVersion) > 0) {
            bestVersion = version;
            bestRelease = release;
        }
    }

    if (bestRelease.isEmpty()) {
        SetError(tr("没有找到可用的 Windows 正式 Release 安装包。"));
        show();
        return;
    }

    m_LatestVersion = bestVersion;
    m_ReleaseUrl = QUrl(bestRelease.value(QStringLiteral("html_url")).toString());
    if (!SelectWindowsAsset(bestRelease)) {
        SetError(tr("最新 Release 没有 GoDesk Windows 安装包。"));
        show();
        return;
    }
    UpdateDetails();

    if (CompareVersions(m_LatestVersion, m_CurrentVersion) <= 0) {
        SetStatus(tr("当前已是最新版本。"));
        m_DownloadButton->setEnabled(false);
        if (m_ShowIfUpToDate)
            show();
        else
            close();
        return;
    }

    SetStatus(tr("发现新版本，可以下载并安装。"));
    m_DownloadButton->setEnabled(true);
    show();
}

bool GoDeskUpdateDialog::SelectWindowsAsset(const QJsonObject& release)
{
    const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
    QJsonObject selected;
    for (const QJsonValue& value : assets) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (name.endsWith(kWindowsAssetMarker, Qt::CaseInsensitive)
            && name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
            selected = asset;
            break;
        }
    }
    if (selected.isEmpty())
        return false;

    m_DownloadFileName = selected.value(QStringLiteral("name")).toString();
    m_DownloadUrl = QUrl(selected.value(QStringLiteral("browser_download_url")).toString());
    m_ExpectedSha256 = selected.value(QStringLiteral("digest")).toString();
    if (m_ExpectedSha256.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive))
        m_ExpectedSha256 = m_ExpectedSha256.mid(QStringLiteral("sha256:").size());
    m_ExpectedSha256 = m_ExpectedSha256.trimmed().toLower();
    return !m_DownloadFileName.isEmpty() && m_DownloadUrl.isValid();
}

void GoDeskUpdateDialog::UpdateDetails()
{
    m_CurrentVersionLabel->setText(m_CurrentVersion);
    m_LatestVersionLabel->setText(m_LatestVersion);
    QString source = tr("GitHub Release API：%1").arg(m_ApiUrl.toString());
    if (m_ReleaseUrl.isValid())
        source += tr("\nRelease：%1").arg(m_ReleaseUrl.toString());
    m_SourceLabel->setText(source);
    m_FileLabel->setText(m_DownloadFileName);
    const QString plannedPath = QDir(GoDeskDataPaths::instance().updateRoot())
        .filePath(m_DownloadFileName);
    m_PathLabel->setText(m_DownloadPath.isEmpty() ? plannedPath : m_DownloadPath);
    m_Sha256Label->setText(m_ExpectedSha256.isEmpty()
                               ? tr("下载后计算")
                               : tr("期望值：%1").arg(m_ExpectedSha256));
}

void GoDeskUpdateDialog::slotDownloadClicked()
{
    if (!m_DownloadUrl.isValid() || m_DownloadFileName.isEmpty())
        return;

    QDir directory(GoDeskDataPaths::instance().updateRoot());
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        SetError(tr("无法创建更新下载目录。"));
        return;
    }

    m_DownloadPath = directory.filePath(m_DownloadFileName);
    m_DownloadFile.setFileName(m_DownloadPath);
    if (!m_DownloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        SetError(tr("无法写入下载文件：%1").arg(m_DownloadPath));
        return;
    }

    UpdateDetails();
    SetStatus(tr("正在下载更新安装包..."));
    m_DownloadButton->setEnabled(false);
    m_ProgressBar->setValue(0);
    m_ProgressBar->show();

    QNetworkRequest request(m_DownloadUrl);
    request.setRawHeader("User-Agent",
                         QStringLiteral("GoDesk/%1").arg(m_CurrentVersion).toUtf8());
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
#endif
    m_DownloadReply = m_Network.get(request);
    connect(m_DownloadReply, &QNetworkReply::readyRead,
            this, &GoDeskUpdateDialog::slotDownloadReadyRead);
    connect(m_DownloadReply, &QNetworkReply::downloadProgress,
            this, &GoDeskUpdateDialog::slotDownloadProgress);
    connect(m_DownloadReply, &QNetworkReply::finished,
            this, &GoDeskUpdateDialog::slotDownloadFinished);
    connect(m_DownloadReply, &QNetworkReply::errorOccurred,
            this, &GoDeskUpdateDialog::slotDownloadError);
}

void GoDeskUpdateDialog::slotDownloadReadyRead()
{
    if (m_DownloadReply && m_DownloadFile.isOpen())
        m_DownloadFile.write(m_DownloadReply->readAll());
}

void GoDeskUpdateDialog::slotDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        m_ProgressBar->setRange(0, 100);
        m_ProgressBar->setValue(static_cast<int>((bytesReceived * 100) / bytesTotal));
    }
}

void GoDeskUpdateDialog::slotDownloadError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error);
    if (m_DownloadReply)
        SetError(tr("下载更新失败：%1").arg(m_DownloadReply->errorString()));
}

bool GoDeskUpdateDialog::VerifyDownloadedFile(QString& actualSha256)
{
    QFile file(m_DownloadPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return false;
    actualSha256 = hash.result().toHex().toLower();
    return m_ExpectedSha256.isEmpty() || actualSha256 == m_ExpectedSha256;
}

void GoDeskUpdateDialog::slotDownloadFinished()
{
    QNetworkReply* reply = m_DownloadReply;
    m_DownloadReply = nullptr;
    if (!reply)
        return;
    const auto error = reply->error();
    const QString errorString = reply->errorString();
    if (m_DownloadFile.isOpen())
        m_DownloadFile.write(reply->readAll());
    reply->deleteLater();
    if (m_DownloadFile.isOpen())
        m_DownloadFile.close();
    if (error != QNetworkReply::NoError) {
        SetError(tr("下载更新失败：%1").arg(errorString));
        return;
    }

    if (!VerifyDownloadedFile(m_ActualSha256)) {
        m_Sha256Label->setText(tr("实际值：%1\n期望值：%2")
                                   .arg(m_ActualSha256, m_ExpectedSha256));
        SetError(tr("SHA-256 校验失败，未启动安装器。"));
        return;
    }

    m_Sha256Label->setText(tr("实际值：%1").arg(m_ActualSha256));
    SetStatus(tr("下载完成，SHA-256 校验通过，正在启动安装器..."));
    if (QProcess::startDetached(m_DownloadPath)) {
        SetStatus(tr("安装器已启动：%1\n请按安装器提示完成升级；GoDesk 不会直接替换当前文件。")
                      .arg(m_DownloadPath));
        QTimer::singleShot(1000, QCoreApplication::instance(),
                           []() { QCoreApplication::quit(); });
    } else {
        SetError(tr("安装器启动失败，文件已保存到：%1").arg(m_DownloadPath));
        m_DownloadButton->setEnabled(false);
    }
}

void GoDeskUpdateDialog::SetStatus(const QString& text)
{
    m_StatusLabel->setStyleSheet(QString());
    m_StatusLabel->setText(text);
}

void GoDeskUpdateDialog::SetError(const QString& text)
{
    m_StatusLabel->setStyleSheet(QStringLiteral("color:#b42318;"));
    m_StatusLabel->setText(text);
    m_ProgressBar->hide();
}

void GoDeskUpdateDialog::CloseReply()
{
    if (m_CheckReply) {
        m_CheckReply->abort();
        m_CheckReply->deleteLater();
        m_CheckReply = nullptr;
    }
    if (m_DownloadReply) {
        m_DownloadReply->abort();
        m_DownloadReply->deleteLater();
        m_DownloadReply = nullptr;
    }
}
