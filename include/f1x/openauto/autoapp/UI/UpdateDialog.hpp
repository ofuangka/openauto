#pragma once

#include <QDialog>
#include <QFileSystemWatcher>
#include <QDir>
#include <QStringList>
#include <QTimer>
#include <QFileInfo>

namespace Ui {
class UpdateDialog;
}

namespace f1x
{
namespace openauto
{
namespace autoapp
{
namespace ui
{

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateDialog(QWidget *parent = nullptr);
    ~UpdateDialog() override;

    void updateCheck();
    void downloadCheck();
    void updateProgress();

private slots:
    void on_ButtonUpdateCsmt_clicked();
    void on_ButtonUpdateUdev_clicked();
    void on_ButtonUpdateOpenauto_clicked();
    void on_ButtonUpdateSystem_clicked();
    void on_ButtonUpdateCheck_clicked();
    void on_ButtonUpdateCancel_clicked();

private:
    Ui::UpdateDialog *ui_;
    QFileSystemWatcher* watcher_tmp;
    QFileSystemWatcher* watcher_download;
};

}
}
}
}
