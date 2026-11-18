#include "GoDeskDataPathWidget.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "GoDeskDataPaths.h"

GoDeskDataPathWidget::GoDeskDataPathWidget(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(tr("数据目录"));

    auto* rootLayout = new QVBoxLayout(this);
    auto* description = new QLabel(
        tr("GoDesk 会优先把可写数据保存到安装目录下的 data 文件夹；安装目录不可写时自动使用用户目录。"),
        this);
    description->setWordWrap(true);
    rootLayout->addWidget(description);

    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    addPathRow(form, tr("状态"), m_statusLabel);
    addPathRow(form, tr("数据根目录"), m_dataRootLabel);
    addPathRow(form, tr("配置目录"), m_configRootLabel);
    addPathRow(form, tr("连接/数据库目录"), m_dataStoreLabel);
    addPathRow(form, tr("用户插件目录"), m_pluginsLabel);
    addPathRow(form, tr("内置插件目录"), m_bundledPluginsLabel);
    addPathRow(form, tr("日志目录"), m_logLabel);
    addPathRow(form, tr("缓存目录"), m_cacheLabel);
    addPathRow(form, tr("更新下载目录"), m_updateLabel);
    rootLayout->addLayout(form);

    auto* buttons = new QHBoxLayout;
    auto* open = new QPushButton(tr("打开数据目录"), this);
    auto* migrate = new QPushButton(tr("迁移数据目录"), this);
    auto* refresh = new QPushButton(tr("刷新"), this);
    buttons->addWidget(open);
    buttons->addWidget(migrate);
    buttons->addStretch();
    buttons->addWidget(refresh);
    rootLayout->addLayout(buttons);

    connect(open, &QPushButton::clicked, this,
            &GoDeskDataPathWidget::slotOpenDataDirectory);
    connect(migrate, &QPushButton::clicked, this,
            &GoDeskDataPathWidget::slotMigrateDataDirectory);
    connect(refresh, &QPushButton::clicked, this,
            &GoDeskDataPathWidget::slotRefresh);
    refreshLabels();
}

void GoDeskDataPathWidget::addPathRow(QFormLayout* layout,
                                      const QString& title,
                                      QLabel*& valueLabel)
{
    valueLabel = new QLabel(this);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse
                                        | Qt::TextSelectableByKeyboard);
    valueLabel->setWordWrap(true);
    layout->addRow(title, valueLabel);
}

void GoDeskDataPathWidget::refreshLabels()
{
    const GoDeskDataPaths& paths = GoDeskDataPaths::instance();
    m_statusLabel->setText(paths.usingFallback()
                               ? tr("已回退到用户目录（安装目录不可写）")
                               : tr("正在使用安装目录下的 data"));
    m_dataRootLabel->setText(QDir::toNativeSeparators(paths.dataRoot()));
    m_configRootLabel->setText(QDir::toNativeSeparators(paths.configRoot()));
    m_dataStoreLabel->setText(QDir::toNativeSeparators(paths.dataStoreRoot()));
    m_pluginsLabel->setText(QDir::toNativeSeparators(paths.pluginsRoot()));
    m_bundledPluginsLabel->setText(
        QDir::toNativeSeparators(paths.bundledPluginsRoot()));
    m_logLabel->setText(QDir::toNativeSeparators(paths.logRoot()));
    m_cacheLabel->setText(QDir::toNativeSeparators(paths.cacheRoot()));
    m_updateLabel->setText(QDir::toNativeSeparators(paths.updateRoot()));
}

void GoDeskDataPathWidget::slotOpenDataDirectory()
{
    const QString path = GoDeskDataPaths::instance().dataRoot();
    if (!QDir(path).exists())
        QDir().mkpath(path);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        QMessageBox::warning(this, tr("打开目录失败"), path);
}

void GoDeskDataPathWidget::slotMigrateDataDirectory()
{
    const QString target = QFileDialog::getExistingDirectory(
        this, tr("选择新的数据目录"),
        GoDeskDataPaths::instance().dataRoot(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (target.isEmpty())
        return;

    QString error;
    if (!GoDeskDataPaths::instance().migrateTo(target, &error)) {
        QMessageBox::critical(this, tr("迁移失败"), error);
        return;
    }

    refreshLabels();
    QMessageBox::information(
        this, tr("迁移完成"),
        tr("数据已复制到：%1\n请重启 GoDesk，使所有插件和数据库使用新目录。")
            .arg(QDir::toNativeSeparators(target)));
}

void GoDeskDataPathWidget::slotRefresh()
{
    refreshLabels();
}

