#include <f1x/openauto/autoapp/UI/UpdateDialog.hpp>
#include "ui_updatedialog.h"
#include <QFileInfo>
#include <QTextStream>
#include <QStorageInfo>
#include <fstream>
#include <cstdio>

namespace f1x
{
namespace openauto
{
namespace autoapp
{
namespace ui
{

UpdateDialog::UpdateDialog(QWidget *parent)
    : QDialog(parent)
    , ui_(new Ui::UpdateDialog)
{
    ui_->setupUi(this);
    connect(ui_->ButtonUpdateCsmt, &QPushButton::clicked, this, &UpdateDialog::on_ButtonUpdateCsmt_clicked);
    connect(ui_->ButtonUpdateUdev, &QPushButton::clicked, this, &UpdateDialog::on_ButtonUpdateUdev_clicked);
    connect(ui_->ButtonUpdateOpenauto, &QPushButton::clicked, this, &UpdateDialog::on_ButtonUpdateOpenauto_clicked);
    connect(ui_->ButtonUpdateSystem, &QPushButton::clicked, this, &UpdateDialog::on_ButtonUpdateSystem_clicked);
    connect(ui_->ButtonUpdateCheck, &QPushButton::clicked, this, &UpdateDialog::on_ButtonUpdateCheck_clicked);
    connect(ui_->ButtonUpdateCancel, &QPushButton::clicked, this, &UpdateDialog::on_ButtonUpdateCancel_clicked);
    connect(ui_->ButtonClose, &QPushButton::clicked, this, &UpdateDialog::close);

    ui_->progressBarCsmt->hide();
    ui_->progressBarUdev->hide();
    ui_->progressBarOpenauto->hide();
    ui_->progressBarSystem->hide();
    ui_->labelSystemReadyInstall->hide();
    ui_->labelUpdateChecking->hide();
    ui_->ButtonUpdateCancel->hide();
    updateCheck();

    watcher_tmp = new QFileSystemWatcher(this);
    watcher_tmp->addPath("/tmp");
    connect(watcher_tmp, &QFileSystemWatcher::directoryChanged, this, &UpdateDialog::updateCheck);

    watcher_download = new QFileSystemWatcher(this);
    watcher_download->addPath("/media/USBDRIVES/CSSTORAGE");
    connect(watcher_download, &QFileSystemWatcher::directoryChanged, this, &UpdateDialog::downloadCheck);

    QStorageInfo storage("/media/USBDRIVES/CSSTORAGE");
    storage.refresh();
    if (storage.isValid() && storage.isReady()) {
        ui_->system->show();
        ui_->labelNoStorage->hide();
    } else {
        ui_->labelNoStorage->show();
        ui_->system->hide();
    }
}

UpdateDialog::~UpdateDialog()
{
    delete ui_;
}

void f1x::openauto::autoapp::ui::UpdateDialog::on_ButtonUpdateCsmt_clicked()
{
    ui_->ButtonUpdateCsmt->hide();
    ui_->progressBarCsmt->show();
    qApp->processEvents();
    system("crankshaft update csmt &");
}

void f1x::openauto::autoapp::ui::UpdateDialog::on_ButtonUpdateUdev_clicked()
{
    ui_->ButtonUpdateUdev->hide();
    ui_->progressBarUdev->show();
    qApp->processEvents();
    system("crankshaft update udev &");
}

void f1x::openauto::autoapp::ui::UpdateDialog::on_ButtonUpdateOpenauto_clicked()
{
    ui_->ButtonUpdateOpenauto->hide();
    ui_->progressBarOpenauto->show();
    qApp->processEvents();
    system("crankshaft update openauto &");
}

void f1x::openauto::autoapp::ui::UpdateDialog::on_ButtonUpdateSystem_clicked()
{
    ui_->ButtonUpdateSystem->hide();
    ui_->progressBarSystem->show();
    ui_->progressBarSystem->setValue(0);
    qApp->processEvents();
    system("crankshaft update system &");
}

void f1x::openauto::autoapp::ui::UpdateDialog::on_ButtonUpdateCheck_clicked()
{
    ui_->ButtonUpdateCheck->hide();
    ui_->labelUpdateChecking->show();
    qApp->processEvents();
    system("/usr/local/bin/crankshaft update check");
    updateCheck();
    ui_->labelUpdateChecking->hide();
    ui_->ButtonUpdateCheck->show();
}

void f1x::openauto::autoapp::ui::UpdateDialog::on_ButtonUpdateCancel_clicked()
{
    ui_->ButtonUpdateCancel->hide();
    system("crankshaft update cancel &");
}

void f1x::openauto::autoapp::ui::UpdateDialog::downloadCheck()
{
    QDir directory("/media/USBDRIVES/CSSTORAGE");
    QStringList files = directory.entryList(QStringList() << "*.zip", QDir::AllEntries, QDir::Name);
    foreach(QString filename, files) {
        if (filename != "") {
            ui_->labelDownload->setText(filename);
        }
    }
}

void f1x::openauto::autoapp::ui::UpdateDialog::updateCheck()
{
    if (!std::ifstream("/tmp/csmt_updating")) {
        if (std::ifstream("/tmp/csmt_update_available")) {
            ui_->labelCsmtOK->hide();
            ui_->ButtonUpdateCsmt->show();
        } else {
            ui_->ButtonUpdateCsmt->hide();
            ui_->progressBarCsmt->hide();
            ui_->labelCsmtOK->show();
        }
    }

    if (!std::ifstream("/tmp/udev_updating")) {
        if (std::ifstream("/tmp/udev_update_available")) {
            ui_->labelUdevOK->hide();
            ui_->ButtonUpdateUdev->show();
        } else {
            ui_->ButtonUpdateUdev->hide();
            ui_->progressBarUdev->hide();
            ui_->labelUdevOK->show();
        }
    }

    if (!std::ifstream("/tmp/openauto_updating")) {
        if (std::ifstream("/tmp/openauto_update_available")) {
            ui_->labelOpenautoOK->hide();
            ui_->ButtonUpdateOpenauto->show();
        } else {
            ui_->ButtonUpdateOpenauto->hide();
            ui_->progressBarOpenauto->hide();
            ui_->labelOpenautoOK->show();
        }
    } else {
        ui_->labelOpenautoOK->hide();
        ui_->ButtonUpdateOpenauto->hide();
    }

    if (std::ifstream("/tmp/system_update_ready")) {
        ui_->labelSystemOK->hide();
        ui_->ButtonUpdateSystem->hide();
        ui_->progressBarSystem->hide();
        ui_->labelSystemReadyInstall->show();
        ui_->ButtonUpdateCancel->hide();
    } else {
        ui_->labelSystemReadyInstall->hide();
        if (std::ifstream("/tmp/system_update_available")) {
            ui_->labelSystemOK->hide();
            ui_->progressBarSystem->hide();
            ui_->ButtonUpdateSystem->show();
        }
        if (std::ifstream("/tmp/system_update_downloading")) {
            ui_->labelSystemOK->hide();
            ui_->ButtonUpdateSystem->hide();
            ui_->ButtonUpdateCheck->hide();
            ui_->progressBarSystem->show();
            ui_->ButtonUpdateCancel->show();

            QFileInfo downloadfile = "/media/USBDRIVES/CSSTORAGE/" + ui_->labelDownload->text();
            if (downloadfile.exists()) {
                qint64 size = downloadfile.size();
                size = size/1024/1024;
                ui_->progressBarSystem->setValue(size);
            }
        } else {
            if (ui_->ButtonUpdateCheck->isVisible() == false) {
                ui_->ButtonUpdateCheck->show();
                ui_->labelDownload->setText("");
                ui_->ButtonUpdateCancel->hide();
            }
        }

        if (!std::ifstream("/tmp/system_update_available") && !std::ifstream("/tmp/system_update_downloading")) {
            ui_->progressBarSystem->hide();
            ui_->labelSystemOK->show();
            ui_->ButtonUpdateSystem->hide();
        }
    }
}

}
}
}
}

