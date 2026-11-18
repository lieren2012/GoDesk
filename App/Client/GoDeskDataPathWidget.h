#pragma once

#include <QWidget>

class QLabel;

class GoDeskDataPathWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GoDeskDataPathWidget(QWidget* parent = nullptr);

private slots:
    void slotOpenDataDirectory();
    void slotMigrateDataDirectory();
    void slotRefresh();

private:
    void addPathRow(class QFormLayout* layout, const QString& title,
                    QLabel*& valueLabel);
    void refreshLabels();

    QLabel* m_statusLabel = nullptr;
    QLabel* m_dataRootLabel = nullptr;
    QLabel* m_configRootLabel = nullptr;
    QLabel* m_dataStoreLabel = nullptr;
    QLabel* m_pluginsLabel = nullptr;
    QLabel* m_bundledPluginsLabel = nullptr;
    QLabel* m_logLabel = nullptr;
    QLabel* m_cacheLabel = nullptr;
    QLabel* m_updateLabel = nullptr;
};

