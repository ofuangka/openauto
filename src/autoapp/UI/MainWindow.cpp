/*
*  This file is part of openauto project.
*  Copyright (C) 2018 f1x.studio (Michal Szwaj)
*
*  openauto is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 3 of the License, or
*  (at your option) any later version.

*  openauto is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with openauto. If not, see <http://www.gnu.org/licenses/>.
*/

#include <unistd.h>

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QRect>
#include <QScreen>
#include <QStandardItemModel>
#include <QTextStream>
#include <QTimer>
#include <QVideoWidget>
#include <cstdio>
#include <f1x/openauto/Common/Log.hpp>
#include <f1x/openauto/autoapp/UI/MainWindow.hpp>
#include <fstream>
#include <iostream>

#include "ui_mainwindow.h"

namespace f1x {
namespace openauto {
namespace autoapp {
namespace ui {

MainWindow::MainWindow(configuration::IConfiguration::Pointer configuration,
                       QWidget *parent)
    : QMainWindow(parent),
      ui_(new Ui::MainWindow),
      localDevice(new QBluetoothLocalDevice) {
  // set default bg color to black
  this->setStyleSheet("QMainWindow {background-color: rgb(0,0,0);}");

  // Set default font and size
  int id = QFontDatabase::addApplicationFont(":/Roboto-Regular.ttf");
  QString family = QFontDatabase::applicationFontFamilies(id).at(0);
  QFont _font(family, 11);
  qApp->setFont(_font);

  this->configuration_ = configuration;

  // trigger files
  this->nightModeEnabled = check_file_exist(this->nightModeFile);
  this->devModeEnabled = check_file_exist(this->devModeFile);
  this->wifiButtonForce = check_file_exist(this->wifiButtonFile);
  this->brightnessButtonForce = check_file_exist(this->brightnessButtonFile);
  this->systemDebugmode = check_file_exist(this->debugModeFile);

  // wallpaper stuff
  this->wallpaperDayFileExists = check_file_exist("wallpaper.png");
  this->wallpaperNightFileExists = check_file_exist("wallpaper-night.png");

  ui_->setupUi(this);
  connect(ui_->pushButtonSettings, &QPushButton::clicked, this,
          &MainWindow::openSettings);
  connect(ui_->pushButtonUpdate, &QPushButton::clicked, this,
          &MainWindow::openUpdateDialog);
  connect(ui_->pushButtonExit, &QPushButton::clicked, this,
          &MainWindow::toggleExit);
  connect(ui_->pushButtonShutdown, &QPushButton::clicked, this,
          &MainWindow::exit);
  connect(ui_->pushButtonReboot, &QPushButton::clicked, this,
          &MainWindow::reboot);
  connect(ui_->pushButtonCancel, &QPushButton::clicked, this,
          &MainWindow::toggleExit);
  connect(ui_->pushButtonDay, &QPushButton::clicked, this,
          &MainWindow::TriggerScriptDay);
  connect(ui_->pushButtonDay, &QPushButton::clicked, this,
          &MainWindow::switchGuiToDay);
  connect(ui_->pushButtonNight, &QPushButton::clicked, this,
          &MainWindow::TriggerScriptNight);
  connect(ui_->pushButtonNight, &QPushButton::clicked, this,
          &MainWindow::switchGuiToNight);
  connect(ui_->pushButtonBrightness, &QPushButton::clicked, this,
          &MainWindow::showBrightnessSlider);
  connect(ui_->pushButtonVolume, &QPushButton::clicked, this,
          &MainWindow::showVolumeSlider);
  connect(ui_->pushButtonDebug, &QPushButton::clicked, this,
          &MainWindow::createDebuglog);
  connect(ui_->pushButtonBluetooth, &QPushButton::clicked, this,
          &MainWindow::setPairable);
  connect(ui_->pushButtonMute, &QPushButton::clicked, this,
          &MainWindow::toggleMuteButton);
  connect(ui_->pushButtonMute, &QPushButton::clicked, this,
          &MainWindow::setMute);
  connect(ui_->pushButtonUnMute, &QPushButton::clicked, this,
          &MainWindow::toggleMuteButton);
  connect(ui_->pushButtonUnMute, &QPushButton::clicked, this,
          &MainWindow::setUnMute);
  connect(ui_->pushButtonWifi, &QPushButton::clicked, this,
          &MainWindow::openConnectDialog);
  connect(ui_->pushButtonAndroidAuto, &QPushButton::clicked, this,
          &MainWindow::TriggerAppStart);
  connect(ui_->pushButtonAndroidAuto, &QPushButton::clicked, this,
          &MainWindow::setRetryUSBConnect);

  ui_->pushButtonBluetooth->hide();
  ui_->labelBluetoothPairable->hide();

  ui_->SysinfoTopLeft->hide();

  ui_->ButtonAndroidAuto->hide();

  ui_->SysinfoTopLeft2->hide();

  ui_->pushButtonUpdate->hide();
  ui_->label_dummy_right->hide();

  if (!configuration->showNetworkinfo()) {
    ui_->networkInfo->hide();
  }

  if (!this->devModeEnabled) {
    ui_->labelLock->hide();
    ui_->labelLockDummy->hide();
  }

  if (std::ifstream("/etc/crankshaft.branch")) {
    QString branch = configuration_->readFileContent("/etc/crankshaft.branch");
    if (branch != "crankshaft-ng") {
      if (branch == "csng-dev") {
      } else {
      }
    }
  }

  QTimer *timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(showTime()));
  timer->start(1000);

  // show debug button if enabled
  if (!this->systemDebugmode) {
    ui_->pushButtonDebug->hide();
  }

  ui_->pushButtonLock->hide();

  ui_->btDevice->hide();

  // check if a device is connected via bluetooth
  if (std::ifstream("/tmp/btdevice")) {
    if (ui_->btDevice->isVisible() == false ||
        ui_->btDevice->text().simplified() == "") {
      QString btdevicename = configuration_->readFileContent("/tmp/btdevice");
      ui_->btDevice->setText(btdevicename);
      ui_->btDevice->show();
    }
  } else {
    if (ui_->btDevice->isVisible() == true) {
      ui_->btDevice->hide();
    }
  }

  // hide brightness slider of control file is not existing
  QFileInfo brightnessFile(brightnessFilename);
  if (!brightnessFile.exists() && !this->brightnessButtonForce) {
    ui_->pushButtonBrightness->hide();
  }

  // as default hide brightness slider
  ui_->BrightnessSliderControl->hide();

  // as default hide power buttons
  ui_->horizontalWidgetPower->hide();

  // as default hide muted button
  ui_->pushButtonUnMute->hide();

  // hide wifi if not forced
  if (!this->wifiButtonForce &&
      !std::ifstream("/tmp/mobile_hotspot_detected")) {
    ui_->AAWIFIWidget->hide();
  } else {
    ui_->AAUSBWidget->hide();
  }

  if (std::ifstream("/tmp/temp_recent_list") ||
      std::ifstream("/tmp/mobile_hotspot_detected")) {
    ui_->pushButtonWifi->show();
    ui_->pushButtonNoWiFiDevice->hide();
  } else {
    ui_->pushButtonWifi->hide();
    ui_->pushButtonNoWiFiDevice->show();
  }

  // show dev labels if dev mode activated
  if (!this->devModeEnabled) {
    ui_->devlabel_left->hide();
    ui_->devlabel_right->hide();
  }

  // set brightness slider attribs from cs config
  ui_->horizontalSliderBrightness->setMinimum(
      configuration->getCSValue("BR_MIN").toInt());
  ui_->horizontalSliderBrightness->setMaximum(
      configuration->getCSValue("BR_MAX").toInt());
  ui_->horizontalSliderBrightness->setSingleStep(
      configuration->getCSValue("BR_STEP").toInt());
  ui_->horizontalSliderBrightness->setTickInterval(
      configuration->getCSValue("BR_STEP").toInt());

  // run monitor for custom brightness command if enabled in crankshaft_env.sh
  if (std::ifstream("/tmp/custombrightness")) {
    if (!configuration->hideBrightnessControl()) {
      ui_->pushButtonBrightness->show();
    }
    this->customBrightnessControl = true;
  }

  // read param file
  if (std::ifstream("/boot/crankshaft/volume")) {
    // init volume
    QString vol = QString::number(
        configuration_->readFileContent("/boot/crankshaft/volume").toInt());
    ui_->volumeValueLabel->setText(vol + "%");
    ui_->horizontalSliderVolume->setValue(vol.toInt());
  }

  // set bg's on startup
  MainWindow::updateBG();
  if (!this->nightModeEnabled) {
    ui_->pushButtonDay->hide();
    ui_->pushButtonNight->show();
  } else {
    ui_->pushButtonNight->hide();
    ui_->pushButtonDay->show();
  }

  // clock viibility by settings
  if (!configuration->showClock()) {
    ui_->Digital_clock->hide();
    this->NoClock = true;
  } else {
    this->NoClock = false;
    ui_->Digital_clock->show();
  }

  // hide brightness button if eanbled in settings
  if (configuration->hideBrightnessControl()) {
    ui_->pushButtonBrightness->hide();
    ui_->BrightnessSliderControl->hide();
    // also hide volume button cause not needed if brightness not visible
    ui_->pushButtonVolume->hide();
  }

  // init alpha values
  MainWindow::updateAlpha();

  if (configuration->mp3AutoPlay()) {
    if (configuration->showAutoPlay()) {
    }
  }

  watcher_tmp = new QFileSystemWatcher(this);
  watcher_tmp->addPath("/tmp");
  connect(watcher_tmp, &QFileSystemWatcher::directoryChanged, this,
          &MainWindow::tmpChanged);

  // Experimental test code
  localDevice = new QBluetoothLocalDevice(this);

  connect(localDevice,
          SIGNAL(hostModeStateChanged(QBluetoothLocalDevice::HostMode)), this,
          SLOT(hostModeStateChanged(QBluetoothLocalDevice::HostMode)));

  hostModeStateChanged(localDevice->hostMode());
  updateNetworkInfo();
}

MainWindow::~MainWindow() { delete ui_; }

}  // namespace ui
}  // namespace autoapp
}  // namespace openauto
}  // namespace f1x

void f1x::openauto::autoapp::ui::MainWindow::hostModeStateChanged(
    QBluetoothLocalDevice::HostMode mode) {
  if (mode != QBluetoothLocalDevice::HostPoweredOff) {
    this->bluetoothEnabled = true;
    ui_->pushButtonBluetooth->show();
    if (std::ifstream("/tmp/bluetooth_pairable")) {
      ui_->labelBluetoothPairable->show();
      ui_->pushButtonBluetooth->hide();
    } else {
      ui_->labelBluetoothPairable->hide();
    }
  } else {
    this->bluetoothEnabled = false;
    ui_->pushButtonBluetooth->hide();
    ui_->labelBluetoothPairable->hide();
  }
}

void f1x::openauto::autoapp::ui::MainWindow::updateNetworkInfo() {
  QNetworkInterface wlan0if = QNetworkInterface::interfaceFromName("wlan0");
  if (wlan0if.flags().testFlag(QNetworkInterface::IsUp)) {
    QList<QNetworkAddressEntry> entrieswlan0 = wlan0if.addressEntries();
    if (!entrieswlan0.isEmpty()) {
      QNetworkAddressEntry wlan0 = entrieswlan0.first();
      // qDebug() << "wlan0: " << wlan0.ip();
      ui_->value_ip->setText(wlan0.ip().toString().simplified());
      ui_->value_mask->setText(wlan0.netmask().toString().simplified());
      if (std::ifstream("/tmp/hotspot_active")) {
        ui_->value_ssid->setText(configuration_->getParamFromFile(
            "/etc/hostapd/hostapd.conf", "ssid"));
      } else {
        ui_->value_ssid->setText(
            configuration_->readFileContent("/tmp/wifi_ssid"));
      }
      ui_->value_gw->setText(
          configuration_->readFileContent("/tmp/gateway_wlan0"));
    }
  } else {
    // qDebug() << "wlan0: down";
    ui_->value_ip->setText("");
    ui_->value_mask->setText("");
    ui_->value_gw->setText("");
    ui_->value_ssid->setText("wlan0: down");
  }
}

void f1x::openauto::autoapp::ui::MainWindow::on_pushButtonBrightness_clicked() {
  this->brightnessFile = new QFile(this->brightnessFilename);
  this->brightnessFileAlt = new QFile(this->brightnessFilenameAlt);

  // Get the current brightness value
  if (!this->customBrightnessControl) {
    if (this->brightnessFile->open(QIODevice::ReadOnly)) {
      QByteArray data = this->brightnessFile->readAll();
      std::string::size_type sz;
      int brightness_val = std::stoi(data.toStdString(), &sz);
      ui_->horizontalSliderBrightness->setValue(brightness_val);
      QString bri = QString::number(brightness_val);
      ui_->brightnessValueLabel->setText(bri);
      this->brightnessFile->close();
    }
  } else {
    if (this->brightnessFileAlt->open(QIODevice::ReadOnly)) {
      QByteArray data = this->brightnessFileAlt->readAll();
      std::string::size_type sz;
      int brightness_val = std::stoi(data.toStdString(), &sz);
      ui_->horizontalSliderBrightness->setValue(brightness_val);
      QString bri = QString::number(brightness_val);
      ui_->brightnessValueLabel->setText(bri);
      this->brightnessFileAlt->close();
    }
  }
  ui_->BrightnessSliderControl->show();
  ui_->VolumeSliderControl->hide();
}

void f1x::openauto::autoapp::ui::MainWindow::on_pushButtonVolume_clicked() {
  ui_->horizontalSliderVolume->show();
  ui_->volumeValueLabel->show();
  if (this->toggleMute) {
    ui_->pushButtonUnMute->show();
  } else {
    ui_->pushButtonMute->show();
  }
  ui_->VolumeSliderControl->show();
  ui_->BrightnessSliderControl->hide();
}

void f1x::openauto::autoapp::ui::MainWindow::
    on_horizontalSliderBrightness_valueChanged(int value) {
  int n = snprintf(this->brightness_str, 5, "%d", value);
  this->brightnessFile = new QFile(this->brightnessFilename);
  this->brightnessFileAlt = new QFile(this->brightnessFilenameAlt);

  if (!this->customBrightnessControl) {
    if (this->brightnessFile->open(QIODevice::WriteOnly)) {
      this->brightness_str[n] = '\n';
      this->brightness_str[n + 1] = '\0';
      this->brightnessFile->write(this->brightness_str);
      this->brightnessFile->close();
    }
  } else {
    if (this->brightnessFileAlt->open(QIODevice::WriteOnly)) {
      this->brightness_str[n] = '\n';
      this->brightness_str[n + 1] = '\0';
      this->brightnessFileAlt->write(this->brightness_str);
      this->brightnessFileAlt->close();
    }
  }
  QString bri = QString::number(value);
  ui_->brightnessValueLabel->setText(bri);
}

void f1x::openauto::autoapp::ui::MainWindow::
    on_horizontalSliderVolume_valueChanged(int value) {
  QString vol = QString::number(value);
  ui_->volumeValueLabel->setText(vol + "%");
  system(
      ("/usr/local/bin/autoapp_helper setvolume " + std::to_string(value) + "&")
          .c_str());
}

void f1x::openauto::autoapp::ui::MainWindow::updateAlpha() {
  int value = configuration_->getAlphaTrans();
  // int n = snprintf(this->alpha_str, 5, "%d", value);

  if (value != this->alpha_current_str) {
    this->alpha_current_str = value;
    double alpha = value / 100.0;
    QString alp = QString::number(alpha);
    ui_->pushButtonExit->setStyleSheet(
        "background-color: rgba(164, 0, 0, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->pushButtonShutdown->setStyleSheet(
        "background-color: rgba(239, 41, 41, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->pushButtonReboot->setStyleSheet(
        "background-color: rgba(252, 175, 62, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->pushButtonCancel->setStyleSheet(
        "background-color: rgba(32, 74, 135, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->pushButtonBrightness->setStyleSheet(
        "background-color: rgba(245, 121, 0, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->pushButtonVolume->setStyleSheet(
        "background-color: rgba(64, 191, 191, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->pushButtonLock->setStyleSheet(
        "background-color: rgba(15, 54, 5, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->pushButtonSettings->setStyleSheet(
        "background-color: rgba(138, 226, 52, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->pushButtonDay->setStyleSheet(
        "background: rgba(252, 233, 79, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->pushButtonNight->setStyleSheet(
        "background-color: rgba(114, 159, 207, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->pushButtonWifi->setStyleSheet(
        "background-color: rgba(252, 175, 62, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->pushButtonDebug->setStyleSheet(
        "background-color: rgba(85, 87, 83, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->pushButtonAndroidAuto->setStyleSheet(
        "background-color: rgba(48, 140, 198, " + alp +
        " ); border: 2px solid rgba(255,255,255,0.5); color: rgb(255,255,255); "
        "border-bottom: 0px; border-top: 0px;");
    ui_->labelAndroidAutoBottom->setStyleSheet(
        "background-color: rgba(48, 140, 198, " + alp +
        " ); border-bottom-left-radius: 4px; border-bottom-right-radius: 4px; "
        "border: 2px solid rgba(255,255,255,0.5); color: rgb(255,255,255); "
        "border-top: 0px;");
    ui_->labelAndroidAutoTop->setStyleSheet(
        "background-color: rgba(48, 140, 198, " + alp +
        " ); border-top-left-radius: 4px; border-top-right-radius: 4px; "
        "border: 2px solid rgba(255,255,255,0.5); color: rgb(255,255,255); "
        "border-bottom: 0px;");
    ui_->pushButtonNoDevice->setStyleSheet(
        "background-color: rgba(48, 140, 198, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5); "
        "color: rgb(255,255,255);");
    ui_->pushButtonNoWiFiDevice->setStyleSheet(
        "background-color: rgba(252, 175, 62, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5); "
        "color: rgb(255,255,255);");
  }
}

void f1x::openauto::autoapp::ui::MainWindow::switchGuiToNight() {
  // MainWindow::on_pushButtonVolume_clicked();
  f1x::openauto::autoapp::ui::MainWindow::updateBG();
  ui_->pushButtonDay->show();
  ui_->pushButtonNight->hide();
  ui_->BrightnessSliderControl->hide();
}

void f1x::openauto::autoapp::ui::MainWindow::switchGuiToDay() {
  // MainWindow::on_pushButtonVolume_clicked();
  f1x::openauto::autoapp::ui::MainWindow::updateBG();
  ui_->pushButtonNight->show();
  ui_->pushButtonDay->hide();
  ui_->BrightnessSliderControl->hide();
}

void f1x::openauto::autoapp::ui::MainWindow::toggleExit() {
  if (!this->exitMenuVisible) {
    ui_->horizontalWidgetButtons->hide();
    ui_->horizontalWidgetPower->show();
    this->exitMenuVisible = true;
  } else {
    ui_->horizontalWidgetButtons->show();
    ui_->horizontalWidgetPower->hide();
    this->exitMenuVisible = false;
  }
}

void f1x::openauto::autoapp::ui::MainWindow::toggleMuteButton() {
  if (!this->toggleMute) {
    ui_->pushButtonMute->hide();
    ui_->pushButtonUnMute->show();
    this->toggleMute = true;
  } else {
    ui_->pushButtonUnMute->hide();
    ui_->pushButtonMute->show();
    this->toggleMute = false;
  }
}

void f1x::openauto::autoapp::ui::MainWindow::updateBG() {
  if (this->date_text == "12/24") {
    this->setStyleSheet(
        "QMainWindow { background: url(:/wallpaper-christmas.png); "
        "background-repeat: no-repeat; background-position: center; }");
  } else if (this->date_text == "12/31") {
    this->setStyleSheet(
        "QMainWindow { background: url(:/wallpaper-firework.png); "
        "background-repeat: no-repeat; background-position: center; }");
  }
  if (!this->nightModeEnabled) {
    if (this->wallpaperDayFileExists) {
      this->setStyleSheet(
          "QMainWindow { background: url(wallpaper.png); background-repeat: "
          "no-repeat; background-position: center; }");
    } else {
      this->setStyleSheet(
          "QMainWindow { background: url(:/black.png); background-repeat: "
          "repeat; background-position: center; }");
    }

  } else {
    if (this->wallpaperNightFileExists) {
      this->setStyleSheet(
          "QMainWindow { background: url(wallpaper-night.png) stretch "
          "stretch; background-repeat: no-repeat; background-position: "
          "center; }");
    } else {
      this->setStyleSheet(
          "QMainWindow { background: url(:/black.png); background-repeat: "
          "repeat; background-position: center; }");
    }
  }
}

void f1x::openauto::autoapp::ui::MainWindow::createDebuglog() {
  system("/usr/local/bin/crankshaft debuglog &");
}

void f1x::openauto::autoapp::ui::MainWindow::setPairable() {
  system("/usr/local/bin/crankshaft bluetooth pairable &");
}

void f1x::openauto::autoapp::ui::MainWindow::setMute() {
  system("/usr/local/bin/autoapp_helper setmute &");
}

void f1x::openauto::autoapp::ui::MainWindow::setUnMute() {
  system("/usr/local/bin/autoapp_helper setunmute &");
}

void f1x::openauto::autoapp::ui::MainWindow::showTime() {
  QTime time = QTime::currentTime();
  QDate date = QDate::currentDate();
  QString time_text = time.toString("hh : mm : ss");
  this->date_text = date.toString("MM/dd");

  if ((time.second() % 2) == 0) {
    time_text[3] = ' ';
    time_text[8] = ' ';
  }

  ui_->Digital_clock->setText(time_text);

  // check connected devices
  if (localDevice->isValid()) {
    QString localDeviceName = localDevice->name();
    QString localDeviceAddress = localDevice->address().toString();
    QList<QBluetoothAddress> btdevices;
    btdevices = localDevice->connectedDevices();

    int count = btdevices.count();
    if (count > 0) {
      // QBluetoothAddress btdevice = btdevices[0];
      // QString btmac = btdevice.toString();
      // ui_->btDeviceCount->setText(QString::number(count));
      if (ui_->btDevice->isVisible() == false) {
        ui_->btDevice->show();
      }
      if (std::ifstream("/tmp/btdevice")) {
        ui_->btDevice->setText(
            configuration_->readFileContent("/tmp/btdevice"));
      }
    } else {
      if (ui_->btDevice->isVisible() == true) {
        ui_->btDevice->hide();
        ui_->btDevice->setText("BT-Device");
      }
    }
  }
}

void f1x::openauto::autoapp::ui::MainWindow::setRetryUSBConnect() {
  ui_->SysinfoTopLeft->setText("Trying USB connect ...");
  ui_->SysinfoTopLeft->show();

  QTimer::singleShot(10000, this, SLOT(resetRetryUSBMessage()));
}

void f1x::openauto::autoapp::ui::MainWindow::resetRetryUSBMessage() {
  ui_->SysinfoTopLeft->setText("");
  ui_->SysinfoTopLeft->hide();
}

bool f1x::openauto::autoapp::ui::MainWindow::check_file_exist(
    const char *fileName) {
  std::ifstream ifile(fileName, std::ios::in);
  // file not ok - checking if symlink
  if (!ifile.good()) {
    QFileInfo linkFile = QString(fileName);
    if (linkFile.isSymLink()) {
      QFileInfo linkTarget(linkFile.symLinkTarget());
      return linkTarget.exists();
    } else {
      return ifile.good();
    }
  } else {
    return ifile.good();
  }
}

void f1x::openauto::autoapp::ui::MainWindow::tmpChanged() {
  try {
    if (std::ifstream("/tmp/entityexit")) {
      MainWindow::TriggerAppStop();
      std::remove("/tmp/entityexit");
    }
  } catch (...) {
    OPENAUTO_LOG(error) << "[OpenAuto] Error in entityexit";
  }

  // check if system is in display off mode (tap2wake)
  if (std::ifstream("/tmp/blankscreen")) {
    if (ui_->centralWidget->isVisible() == true) {
      CloseAllDialogs();
      ui_->centralWidget->hide();
    }
  } else {
    if (ui_->centralWidget->isVisible() == false) {
      ui_->centralWidget->show();
    }
  }

  // check if system is in display off mode (tap2wake/screensaver)
  if (std::ifstream("/tmp/screensaver")) {
    if (ui_->menuWidget->isVisible() == true) {
      ui_->menuWidget->hide();
    }

    if (ui_->headerWidget->isVisible() == true) {
      ui_->headerWidget->hide();
      CloseAllDialogs();
    }

    if (ui_->VolumeSliderControl->isVisible() == true) {
      ui_->VolumeSliderControl->hide();
    }
    if (ui_->BrightnessSliderControl->isVisible() == true) {
      ui_->BrightnessSliderControl->hide();
    }
  } else {
    if (ui_->headerWidget->isVisible() == false) {
      ui_->headerWidget->show();
    }
    if (ui_->VolumeSliderControl->isVisible() == false) {
      ui_->VolumeSliderControl->show();
    }
  }

  // check if custom command needs black background
  if (std::ifstream("/tmp/blackscreen")) {
    if (ui_->centralWidget->isVisible() == true) {
      ui_->centralWidget->hide();
      this->setStyleSheet("QMainWindow {background-color: rgb(0,0,0);}");
      this->background_set = false;
    }
  } else {
    if (this->background_set == false) {
      f1x::openauto::autoapp::ui::MainWindow::updateBG();
      this->background_set = true;
    }
  }

  // check if phone is conencted to usb
  if (std::ifstream("/tmp/android_device")) {
    if (ui_->ButtonAndroidAuto->isVisible() == false) {
      ui_->ButtonAndroidAuto->show();
      ui_->pushButtonNoDevice->hide();
    }
    try {
      QFile deviceData(QString("/tmp/android_device"));
      deviceData.open(QIODevice::ReadOnly);
      QTextStream data_date(&deviceData);
      QString linedate = data_date.readAll().split("\n")[1];
      deviceData.close();
      ui_->labelAndroidAutoBottom->setText(
          linedate.simplified().replace("_", " "));
    } catch (...) {
      ui_->labelAndroidAutoBottom->setText("");
    }
  } else {
    if (ui_->ButtonAndroidAuto->isVisible() == true) {
      ui_->pushButtonNoDevice->show();
      ui_->ButtonAndroidAuto->hide();
    }

    ui_->labelAndroidAutoBottom->setText("");
  }

  // check if bluetooth pairable
  if (this->bluetoothEnabled) {
    if (std::ifstream("/tmp/bluetooth_pairable")) {
      if (ui_->labelBluetoothPairable->isVisible() == false) {
        ui_->labelBluetoothPairable->show();
      }
      if (ui_->pushButtonBluetooth->isVisible() == true) {
        ui_->pushButtonBluetooth->hide();
      }
    } else {
      if (ui_->labelBluetoothPairable->isVisible() == true) {
        ui_->labelBluetoothPairable->hide();
      }
      if (ui_->pushButtonBluetooth->isVisible() == false) {
        ui_->pushButtonBluetooth->show();
      }
    }
  } else {
    if (ui_->labelBluetoothPairable->isVisible() == true) {
      ui_->labelBluetoothPairable->hide();
    }
    if (ui_->pushButtonBluetooth->isVisible() == true) {
      ui_->pushButtonBluetooth->hide();
    }
  }

  if (std::ifstream("/tmp/config_in_progress") ||
      std::ifstream("/tmp/debug_in_progress") ||
      std::ifstream("/tmp/enable_pairing")) {
    if (ui_->SysinfoTopLeft2->isVisible() == false) {
      if (std::ifstream("/tmp/config_in_progress")) {
        ui_->pushButtonSettings->hide();
        ui_->pushButtonLock->show();
        ui_->SysinfoTopLeft2->setText("Config in progress ...");
        ui_->SysinfoTopLeft2->show();
      }
      if (std::ifstream("/tmp/debug_in_progress")) {
        ui_->pushButtonSettings->hide();
        ui_->pushButtonDebug->hide();
        ui_->pushButtonLock->show();
        ui_->SysinfoTopLeft2->setText("Creating debug.zip ...");
        ui_->SysinfoTopLeft2->show();
      }
      if (std::ifstream("/tmp/enable_pairing")) {
        ui_->pushButtonDebug->hide();
        ui_->SysinfoTopLeft2->setText("Pairing enabled for 120 seconds!");
        ui_->SysinfoTopLeft2->show();
      }
    }
  } else {
    if (ui_->SysinfoTopLeft2->isVisible() == true) {
      ui_->SysinfoTopLeft2->setText("");
      ui_->SysinfoTopLeft2->hide();
      ui_->pushButtonSettings->show();
      ui_->pushButtonLock->hide();
      if (this->systemDebugmode) {
        ui_->pushButtonDebug->show();
      }
    }
  }

  // update day/night state
  this->nightModeEnabled = check_file_exist("/tmp/night_mode_enabled");

  if (this->nightModeEnabled) {
    if (!this->DayNightModeState) {
      this->DayNightModeState = true;
      f1x::openauto::autoapp::ui::MainWindow::switchGuiToNight();
    }
  } else {
    if (this->DayNightModeState) {
      this->DayNightModeState = false;
      f1x::openauto::autoapp::ui::MainWindow::switchGuiToDay();
    }
  }

  // check if shutdown is external triggered and init clean app exit
  if (std::ifstream("/tmp/external_exit")) {
    f1x::openauto::autoapp::ui::MainWindow::MainWindow::exit();
  }

  this->hotspotActive = check_file_exist("/tmp/hotspot_active");

  // hide wifi if hotspot disabled and force wifi unselected
  if (!this->hotspotActive && !std::ifstream("/tmp/mobile_hotspot_detected")) {
    if ((ui_->AAWIFIWidget->isVisible() == true)) {
      ui_->AAWIFIWidget->hide();
      ui_->AAUSBWidget->show();
    }
  } else {
    if ((ui_->AAWIFIWidget->isVisible() == false)) {
      ui_->AAWIFIWidget->show();
      ui_->AAUSBWidget->hide();
    }
  }

  if (std::ifstream("/tmp/temp_recent_list") ||
      std::ifstream("/tmp/mobile_hotspot_detected")) {
    if (ui_->pushButtonWifi->isVisible() == false) {
      ui_->pushButtonWifi->show();
    }
    if (ui_->pushButtonNoWiFiDevice->isVisible() == true) {
      ui_->pushButtonNoWiFiDevice->hide();
    }
  } else {
    if (ui_->pushButtonWifi->isVisible() == true) {
      ui_->pushButtonWifi->hide();
    }
    if (ui_->pushButtonNoWiFiDevice->isVisible() == false) {
      ui_->pushButtonNoWiFiDevice->show();
    }
  }

  // clock viibility by settings
  if (!this->configuration_->showClock()) {
    ui_->Digital_clock->hide();
    this->NoClock = true;
  } else {
    this->NoClock = false;
    ui_->Digital_clock->show();
  }

  if (!this->configuration_->showNetworkinfo()) {
    if (ui_->networkInfo->isVisible() == true) {
      ui_->networkInfo->hide();
    }
  } else {
    if (ui_->networkInfo->isVisible() == false) {
      ui_->networkInfo->show();
    }
  }

  // hide brightness button if eanbled in settings
  if (configuration_->hideBrightnessControl()) {
    if ((ui_->pushButtonBrightness->isVisible() == true) ||
        (ui_->pushButtonBrightness->isVisible() == true) ||
        (ui_->BrightnessSliderControl->isVisible() == true)) {
      ui_->pushButtonBrightness->hide();
      ui_->BrightnessSliderControl->hide();
      // also hide volume button if brightness hidden
      ui_->pushButtonVolume->hide();
      ui_->VolumeSliderControl->show();
    }
  } else {
  }

  // read value from tsl2561
  if (std::ifstream("/tmp/tsl2561") && this->configuration_->showLux()) {
    if (ui_->label_left->isVisible() == false) {
      ui_->label_left->show();
      ui_->label_right->show();
    }
    ui_->label_left->setText("Lux: " +
                             configuration_->readFileContent("/tmp/tsl2561"));
  } else {
    if (ui_->label_left->isVisible() == true) {
      ui_->label_left->hide();
      ui_->label_right->hide();
      ui_->label_left->setText("");
      ui_->label_right->setText("");
    }
  }
  MainWindow::updateAlpha();

  // update notify
  this->csmtupdate = check_file_exist("/tmp/csmt_update_available");
  this->udevupdate = check_file_exist("/tmp/udev_update_available");
  this->openautoupdate = check_file_exist("/tmp/openauto_update_available");
  this->systemupdate = check_file_exist("/tmp/system_update_available");

  if (this->csmtupdate || this->udevupdate || this->openautoupdate ||
      this->systemupdate) {
    if (ui_->pushButtonUpdate->isVisible() == false) {
      ui_->pushButtonUpdate->show();
      ui_->label_left->show();
      ui_->label_right->show();
      if (this->devModeEnabled) {
        ui_->devlabel_right->hide();
      } else {
        ui_->label_dummy_right->show();
      }
    }
  } else {
    if (ui_->pushButtonUpdate->isVisible() == true) {
      ui_->pushButtonUpdate->hide();
      ui_->label_left->hide();
      ui_->label_right->hide();
      ui_->label_dummy_right->hide();
      if (this->devModeEnabled) {
        ui_->devlabel_right->show();
      }
    }
  }

  if (std::ifstream("/tmp/btdevice") ||
      std::ifstream("/tmp/dev_mode_enabled") ||
      std::ifstream("/tmp/android_device")) {
    if (ui_->labelLock->isVisible() == false) {
      ui_->labelLock->show();
      ui_->labelLockDummy->show();
    }
  } else {
    if (ui_->labelLock->isVisible() == true) {
      ui_->labelLock->hide();
      ui_->labelLockDummy->hide();
    }
  }
  updateNetworkInfo();
}
