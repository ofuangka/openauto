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
  connect(ui_->ButtonSettings, &QPushButton::clicked, this,
          &MainWindow::openSettings);
  connect(ui_->ButtonExit, &QPushButton::clicked, this,
          &MainWindow::toggleExit);
  connect(ui_->ButtonShutdown, &QPushButton::clicked, this,
          &MainWindow::exit);
  connect(ui_->ButtonReboot, &QPushButton::clicked, this,
          &MainWindow::reboot);
  connect(ui_->ButtonCancel, &QPushButton::clicked, this,
          &MainWindow::toggleExit);
  connect(ui_->ButtonDay, &QPushButton::clicked, this,
          &MainWindow::TriggerScriptDay);
  connect(ui_->ButtonDay, &QPushButton::clicked, this,
          &MainWindow::switchGuiToDay);
  connect(ui_->ButtonNight, &QPushButton::clicked, this,
          &MainWindow::TriggerScriptNight);
  connect(ui_->ButtonNight, &QPushButton::clicked, this,
          &MainWindow::switchGuiToNight);
  connect(ui_->ButtonBrightness, &QPushButton::clicked, this,
          &MainWindow::showSliderBrightness);
  connect(ui_->ButtonVolume, &QPushButton::clicked, this,
          &MainWindow::showVolumeSlider);
  connect(ui_->ButtonDebug, &QPushButton::clicked, this,
          &MainWindow::createDebuglog);
  connect(ui_->ButtonBluetooth, &QPushButton::clicked, this,
          &MainWindow::setPairable);
  connect(ui_->ButtonMute, &QPushButton::clicked, this,
          &MainWindow::toggleMuteButton);
  connect(ui_->ButtonMute, &QPushButton::clicked, this,
          &MainWindow::setMute);
  connect(ui_->ButtonUnmute, &QPushButton::clicked, this,
          &MainWindow::toggleMuteButton);
  connect(ui_->ButtonUnmute, &QPushButton::clicked, this,
          &MainWindow::setUnMute);
  connect(ui_->ButtonWifi, &QPushButton::clicked, this,
          &MainWindow::openConnectDialog);
  connect(ui_->ButtonAndroidAuto, &QPushButton::clicked, this,
          &MainWindow::TriggerAppStart);
  connect(ui_->ButtonAndroidAuto, &QPushButton::clicked, this,
          &MainWindow::setRetryUSBConnect);

  ui_->ButtonBluetooth->hide();
  ui_->Pairable->hide();

  ui_->SysinfoTopLeft->hide();

  ui_->ButtonAndroidAutoWidget->hide();

  ui_->SysinfoTopLeft2->hide();

  if (!configuration->showNetworkInfo()) {
    ui_->NetworkInfo->hide();
  }

  if (!this->devModeEnabled) {
    ui_->Lock->hide();
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
    ui_->ButtonDebug->hide();
  }

  ui_->ButtonLock->hide();

  ui_->BluetoothDevice->hide();

  // check if a device is connected via bluetooth
  if (std::ifstream("/tmp/btdevice")) {
    if (ui_->BluetoothDevice->isVisible() == false ||
        ui_->BluetoothDevice->text().simplified() == "") {
      QString btdevicename = configuration_->readFileContent("/tmp/btdevice");
      ui_->BluetoothDevice->setText(btdevicename);
      ui_->BluetoothDevice->show();
    }
  } else {
    if (ui_->BluetoothDevice->isVisible() == true) {
      ui_->BluetoothDevice->hide();
    }
  }

  // hide brightness slider of control file is not existing
  QFileInfo brightnessFile(brightnessFilename);
  if (!brightnessFile.exists() && !this->brightnessButtonForce) {
    ui_->ButtonBrightness->hide();
  }

  // as default hide brightness slider
  ui_->BrightnessWidget->hide();

  // as default hide power buttons
  ui_->TilesBack->hide();

  // as default hide muted button
  ui_->ButtonUnmute->hide();

  // hide wifi if not forced
  if (!this->wifiButtonForce &&
      !std::ifstream("/tmp/mobile_hotspot_detected")) {
    ui_->AAWIFIWidget->hide();
  } else {
    ui_->AAUSBWidget->hide();
  }

  if (std::ifstream("/tmp/temp_recent_list") ||
      std::ifstream("/tmp/mobile_hotspot_detected")) {
    ui_->ButtonWifi->show();
    ui_->ButtonNoWiFiDevice->hide();
  } else {
    ui_->ButtonWifi->hide();
    ui_->ButtonNoWiFiDevice->show();
  }

  // show dev labels if dev mode activated
  if (!this->devModeEnabled) {
    ui_->DevModeEnabled->hide();
    ui_->DevModeEnabledLeft->hide();
  }

  // set brightness slider attribs from cs config
  ui_->SliderBrightness->setMinimum(
      configuration->getCSValue("BR_MIN").toInt());
  ui_->SliderBrightness->setMaximum(
      configuration->getCSValue("BR_MAX").toInt());
  ui_->SliderBrightness->setSingleStep(
      configuration->getCSValue("BR_STEP").toInt());
  ui_->SliderBrightness->setTickInterval(
      configuration->getCSValue("BR_STEP").toInt());

  // run monitor for custom brightness command if enabled in crankshaft_env.sh
  if (std::ifstream("/tmp/custombrightness")) {
    if (!configuration->hideBrightnessControl()) {
      ui_->ButtonBrightness->show();
    }
    this->customBrightnessControl = true;
  }

  // read param file
  if (std::ifstream("/boot/crankshaft/volume")) {
    // init volume
    QString vol = QString::number(
        configuration_->readFileContent("/boot/crankshaft/volume").toInt());
    ui_->ValueVolume->setText(vol + "%");
    ui_->SliderVolume->setValue(vol.toInt());
  }

  // set bg's on startup
  MainWindow::updateBG();
  if (!this->nightModeEnabled) {
    ui_->ButtonDay->hide();
    ui_->ButtonNight->show();
  } else {
    ui_->ButtonNight->hide();
    ui_->ButtonDay->show();
  }

  // clock viibility by settings
  if (!configuration->showClock()) {
    ui_->Clock->hide();
    this->NoClock = true;
  } else {
    this->NoClock = false;
    ui_->Clock->show();
  }

  // hide brightness button if eanbled in settings
  if (configuration->hideBrightnessControl()) {
    ui_->ButtonBrightness->hide();
    ui_->BrightnessWidget->hide();
    // also hide volume button cause not needed if brightness not visible
    ui_->ButtonVolume->hide();
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
    ui_->ButtonBluetooth->show();
    if (std::ifstream("/tmp/bluetooth_pairable")) {
      ui_->Pairable->show();
      ui_->ButtonBluetooth->hide();
    } else {
      ui_->Pairable->hide();
    }
  } else {
    this->bluetoothEnabled = false;
    ui_->ButtonBluetooth->hide();
    ui_->Pairable->hide();
  }
}

void f1x::openauto::autoapp::ui::MainWindow::updateNetworkInfo() {
  QNetworkInterface wlan0if = QNetworkInterface::interfaceFromName("wlan0");
  if (wlan0if.flags().testFlag(QNetworkInterface::IsUp)) {
    QList<QNetworkAddressEntry> entrieswlan0 = wlan0if.addressEntries();
    if (!entrieswlan0.isEmpty()) {
      QNetworkAddressEntry wlan0 = entrieswlan0.first();
      // qDebug() << "wlan0: " << wlan0.ip();
      ui_->ValueIp->setText(wlan0.ip().toString().simplified());
      ui_->ValueMask->setText(wlan0.netmask().toString().simplified());
      if (std::ifstream("/tmp/hotspot_active")) {
        ui_->ValueSsid->setText(configuration_->getParamFromFile(
            "/etc/hostapd/hostapd.conf", "ssid"));
      } else {
        ui_->ValueSsid->setText(
            configuration_->readFileContent("/tmp/wifi_ssid"));
      }
      ui_->ValueGw->setText(
          configuration_->readFileContent("/tmp/gateway_wlan0"));
    }
  } else {
    // qDebug() << "wlan0: down";
    ui_->ValueIp->setText("");
    ui_->ValueMask->setText("");
    ui_->ValueGw->setText("");
    ui_->ValueSsid->setText("wlan0: down");
  }
}

void f1x::openauto::autoapp::ui::MainWindow::onClickButtonBrightness() {
  this->brightnessFile = new QFile(this->brightnessFilename);
  this->brightnessFileAlt = new QFile(this->brightnessFilenameAlt);

  // Get the current brightness value
  if (!this->customBrightnessControl) {
    if (this->brightnessFile->open(QIODevice::ReadOnly)) {
      QByteArray data = this->brightnessFile->readAll();
      std::string::size_type sz;
      int brightness_val = std::stoi(data.toStdString(), &sz);
      ui_->SliderBrightness->setValue(brightness_val);
      QString bri = QString::number(brightness_val);
      ui_->ValueBrightness->setText(bri);
      this->brightnessFile->close();
    }
  } else {
    if (this->brightnessFileAlt->open(QIODevice::ReadOnly)) {
      QByteArray data = this->brightnessFileAlt->readAll();
      std::string::size_type sz;
      int brightness_val = std::stoi(data.toStdString(), &sz);
      ui_->SliderBrightness->setValue(brightness_val);
      QString bri = QString::number(brightness_val);
      ui_->ValueBrightness->setText(bri);
      this->brightnessFileAlt->close();
    }
  }
  ui_->BrightnessWidget->show();
  ui_->VolumeWidget->hide();
}

void f1x::openauto::autoapp::ui::MainWindow::onClickButtonVolume() {
  ui_->SliderVolume->show();
  ui_->ValueVolume->show();
  if (this->toggleMute) {
    ui_->ButtonUnmute->show();
  } else {
    ui_->ButtonMute->show();
  }
  ui_->VolumeWidget->show();
  ui_->BrightnessWidget->hide();
}

void f1x::openauto::autoapp::ui::MainWindow::
    onChangeSliderBrightness(int value) {
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
  ui_->ValueBrightness->setText(bri);
}

void f1x::openauto::autoapp::ui::MainWindow::
    onChangeSliderVolume(int value) {
  QString vol = QString::number(value);
  ui_->ValueVolume->setText(vol + "%");
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
    ui_->ButtonExit->setStyleSheet(
        "background-color: rgba(164, 0, 0, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->ButtonShutdown->setStyleSheet(
        "background-color: rgba(239, 41, 41, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->ButtonReboot->setStyleSheet(
        "background-color: rgba(252, 175, 62, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->ButtonCancel->setStyleSheet(
        "background-color: rgba(32, 74, 135, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->ButtonBrightness->setStyleSheet(
        "background-color: rgba(245, 121, 0, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->ButtonVolume->setStyleSheet(
        "background-color: rgba(64, 191, 191, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->ButtonLock->setStyleSheet(
        "background-color: rgba(15, 54, 5, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->ButtonSettings->setStyleSheet(
        "background-color: rgba(138, 226, 52, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->ButtonDay->setStyleSheet(
        "background: rgba(252, 233, 79, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->ButtonNight->setStyleSheet(
        "background-color: rgba(114, 159, 207, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->ButtonWifi->setStyleSheet(
        "background-color: rgba(252, 175, 62, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->ButtonDebug->setStyleSheet(
        "background-color: rgba(85, 87, 83, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5);");
    ui_->ButtonAndroidAuto->setStyleSheet(
        "background-color: rgba(48, 140, 198, " + alp +
        " ); border: 2px solid rgba(255,255,255,0.5); color: rgb(255,255,255); "
        "border-bottom: 0px; border-top: 0px;");
    ui_->LabelAndroidAutoBottom->setStyleSheet(
        "background-color: rgba(48, 140, 198, " + alp +
        " ); border-bottom-left-radius: 4px; border-bottom-right-radius: 4px; "
        "border: 2px solid rgba(255,255,255,0.5); color: rgb(255,255,255); "
        "border-top: 0px;");
    ui_->LabelAndroidAutoTop->setStyleSheet(
        "background-color: rgba(48, 140, 198, " + alp +
        " ); border-top-left-radius: 4px; border-top-right-radius: 4px; "
        "border: 2px solid rgba(255,255,255,0.5); color: rgb(255,255,255); "
        "border-bottom: 0px;");
    ui_->ButtonNoDevice->setStyleSheet(
        "background-color: rgba(48, 140, 198, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5); "
        "color: rgb(255,255,255);");
    ui_->ButtonNoWiFiDevice->setStyleSheet(
        "background-color: rgba(252, 175, 62, " + alp +
        " ); border-radius: 4px; border: 2px solid rgba(255,255,255,0.5); "
        "color: rgb(255,255,255);");
  }
}

void f1x::openauto::autoapp::ui::MainWindow::switchGuiToNight() {
  // MainWindow::on_ButtonVolume_clicked();
  f1x::openauto::autoapp::ui::MainWindow::updateBG();
  ui_->ButtonDay->show();
  ui_->ButtonNight->hide();
  ui_->BrightnessWidget->hide();
}

void f1x::openauto::autoapp::ui::MainWindow::switchGuiToDay() {
  // MainWindow::on_ButtonVolume_clicked();
  f1x::openauto::autoapp::ui::MainWindow::updateBG();
  ui_->ButtonNight->show();
  ui_->ButtonDay->hide();
  ui_->BrightnessWidget->hide();
}

void f1x::openauto::autoapp::ui::MainWindow::toggleExit() {
  if (!this->exitMenuVisible) {
    ui_->TilesFront->hide();
    ui_->TilesBack->show();
    this->exitMenuVisible = true;
  } else {
    ui_->TilesFront->show();
    ui_->TilesBack->hide();
    this->exitMenuVisible = false;
  }
}

void f1x::openauto::autoapp::ui::MainWindow::toggleMuteButton() {
  if (!this->toggleMute) {
    ui_->ButtonMute->hide();
    ui_->ButtonUnmute->show();
    this->toggleMute = true;
  } else {
    ui_->ButtonUnmute->hide();
    ui_->ButtonMute->show();
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

  ui_->Clock->setText(time_text);

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
      // ui_->BluetoothDeviceCount->setText(QString::number(count));
      if (ui_->BluetoothDevice->isVisible() == false) {
        ui_->BluetoothDevice->show();
      }
      if (std::ifstream("/tmp/btdevice")) {
        ui_->BluetoothDevice->setText(
            configuration_->readFileContent("/tmp/btdevice"));
      }
    } else {
      if (ui_->BluetoothDevice->isVisible() == true) {
        ui_->BluetoothDevice->hide();
        ui_->BluetoothDevice->setText("BT-Device");
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
    if (ui_->MainWidget->isVisible() == true) {
      CloseAllDialogs();
      ui_->MainWidget->hide();
    }
  } else {
    if (ui_->MainWidget->isVisible() == false) {
      ui_->MainWidget->show();
    }
  }

  // check if system is in display off mode (tap2wake/screensaver)
  if (std::ifstream("/tmp/screensaver")) {
    if (ui_->Tiles->isVisible() == true) {
      ui_->Tiles->hide();
    }

    if (ui_->Header->isVisible() == true) {
      ui_->Header->hide();
      CloseAllDialogs();
    }

    if (ui_->VolumeWidget->isVisible() == true) {
      ui_->VolumeWidget->hide();
    }
    if (ui_->BrightnessWidget->isVisible() == true) {
      ui_->BrightnessWidget->hide();
    }
  } else {
    if (ui_->Header->isVisible() == false) {
      ui_->Header->show();
    }
    if (ui_->VolumeWidget->isVisible() == false) {
      ui_->VolumeWidget->show();
    }
  }

  // check if custom command needs black background
  if (std::ifstream("/tmp/blackscreen")) {
    if (ui_->MainWidget->isVisible() == true) {
      ui_->MainWidget->hide();
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
    if (ui_->ButtonAndroidAutoWidget->isVisible() == false) {
      ui_->ButtonAndroidAutoWidget->show();
      ui_->ButtonNoDevice->hide();
    }
    try {
      QFile deviceData(QString("/tmp/android_device"));
      deviceData.open(QIODevice::ReadOnly);
      QTextStream data_date(&deviceData);
      QString linedate = data_date.readAll().split("\n")[1];
      deviceData.close();
      ui_->LabelAndroidAutoBottom->setText(
          linedate.simplified().replace("_", " "));
    } catch (...) {
      ui_->LabelAndroidAutoBottom->setText("");
    }
  } else {
    if (ui_->ButtonAndroidAutoWidget->isVisible() == true) {
      ui_->ButtonNoDevice->show();
      ui_->ButtonAndroidAutoWidget->hide();
    }

    ui_->LabelAndroidAutoBottom->setText("");
  }

  // check if bluetooth pairable
  if (this->bluetoothEnabled) {
    if (std::ifstream("/tmp/bluetooth_pairable")) {
      if (ui_->Pairable->isVisible() == false) {
        ui_->Pairable->show();
      }
      if (ui_->ButtonBluetooth->isVisible() == true) {
        ui_->ButtonBluetooth->hide();
      }
    } else {
      if (ui_->Pairable->isVisible() == true) {
        ui_->Pairable->hide();
      }
      if (ui_->ButtonBluetooth->isVisible() == false) {
        ui_->ButtonBluetooth->show();
      }
    }
  } else {
    if (ui_->Pairable->isVisible() == true) {
      ui_->Pairable->hide();
    }
    if (ui_->ButtonBluetooth->isVisible() == true) {
      ui_->ButtonBluetooth->hide();
    }
  }

  if (std::ifstream("/tmp/config_in_progress") ||
      std::ifstream("/tmp/debug_in_progress") ||
      std::ifstream("/tmp/enable_pairing")) {
    if (ui_->SysinfoTopLeft2->isVisible() == false) {
      if (std::ifstream("/tmp/config_in_progress")) {
        ui_->ButtonSettings->hide();
        ui_->ButtonLock->show();
        ui_->SysinfoTopLeft2->setText("Config in progress ...");
        ui_->SysinfoTopLeft2->show();
      }
      if (std::ifstream("/tmp/debug_in_progress")) {
        ui_->ButtonSettings->hide();
        ui_->ButtonDebug->hide();
        ui_->ButtonLock->show();
        ui_->SysinfoTopLeft2->setText("Creating debug.zip ...");
        ui_->SysinfoTopLeft2->show();
      }
      if (std::ifstream("/tmp/enable_pairing")) {
        ui_->ButtonDebug->hide();
        ui_->SysinfoTopLeft2->setText("Pairing enabled for 120 seconds!");
        ui_->SysinfoTopLeft2->show();
      }
    }
  } else {
    if (ui_->SysinfoTopLeft2->isVisible() == true) {
      ui_->SysinfoTopLeft2->setText("");
      ui_->SysinfoTopLeft2->hide();
      ui_->ButtonSettings->show();
      ui_->ButtonLock->hide();
      if (this->systemDebugmode) {
        ui_->ButtonDebug->show();
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
    if (ui_->ButtonWifi->isVisible() == false) {
      ui_->ButtonWifi->show();
    }
    if (ui_->ButtonNoWiFiDevice->isVisible() == true) {
      ui_->ButtonNoWiFiDevice->hide();
    }
  } else {
    if (ui_->ButtonWifi->isVisible() == true) {
      ui_->ButtonWifi->hide();
    }
    if (ui_->ButtonNoWiFiDevice->isVisible() == false) {
      ui_->ButtonNoWiFiDevice->show();
    }
  }

  // clock viibility by settings
  if (!this->configuration_->showClock()) {
    ui_->Clock->hide();
    this->NoClock = true;
  } else {
    this->NoClock = false;
    ui_->Clock->show();
  }

  if (!this->configuration_->showNetworkInfo()) {
    if (ui_->NetworkInfo->isVisible() == true) {
      ui_->NetworkInfo->hide();
    }
  } else {
    if (ui_->NetworkInfo->isVisible() == false) {
      ui_->NetworkInfo->show();
    }
  }

  // hide brightness button if eanbled in settings
  if (configuration_->hideBrightnessControl()) {
    if ((ui_->ButtonBrightness->isVisible() == true) ||
        (ui_->ButtonBrightness->isVisible() == true) ||
        (ui_->BrightnessWidget->isVisible() == true)) {
      ui_->ButtonBrightness->hide();
      ui_->BrightnessWidget->hide();
      // also hide volume button if brightness hidden
      ui_->ButtonVolume->hide();
      ui_->VolumeWidget->show();
    }
  } else {
  }

  MainWindow::updateAlpha();

  if (std::ifstream("/tmp/btdevice") ||
      std::ifstream("/tmp/dev_mode_enabled") ||
      std::ifstream("/tmp/android_device")) {
    if (ui_->Lock->isVisible() == false) {
      ui_->Lock->show();
    }
  } else {
    if (ui_->Lock->isVisible() == true) {
      ui_->Lock->hide();
    }
  }
  updateNetworkInfo();
}
