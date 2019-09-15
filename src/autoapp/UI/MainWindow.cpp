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
  this->nightModeEnabled = doesFileExist(this->nightModeFile);
  this->wifiButtonForce = doesFileExist(this->wifiButtonFile);
  this->brightnessButtonForce = doesFileExist(this->brightnessButtonFile);

  // wallpaper stuff
  this->wallpaperDayFileExists = doesFileExist("wallpaper.png");
  this->wallpaperNightFileExists = doesFileExist("wallpaper-night.png");

  ui_->setupUi(this);
  connect(ui_->ButtonSettings, &QPushButton::clicked, this,
          &MainWindow::openSettings);
  connect(ui_->ButtonExit, &QPushButton::clicked, this,
          &MainWindow::toggleExit);
  connect(ui_->ButtonShutdown, &QPushButton::clicked, this, &MainWindow::exit);
  connect(ui_->ButtonReboot, &QPushButton::clicked, this, &MainWindow::reboot);
  connect(ui_->ButtonCancel, &QPushButton::clicked, this,
          &MainWindow::toggleExit);
  connect(ui_->ButtonDay, &QPushButton::clicked, this,
          &MainWindow::triggerScriptDay);
  connect(ui_->ButtonDay, &QPushButton::clicked, this,
          &MainWindow::switchGuiToDay);
  connect(ui_->ButtonNight, &QPushButton::clicked, this,
          &MainWindow::triggerScriptNight);
  connect(ui_->ButtonNight, &QPushButton::clicked, this,
          &MainWindow::switchGuiToNight);
  connect(ui_->ButtonBrightness, &QPushButton::clicked, this,
          &MainWindow::showSliderBrightness);
  connect(ui_->ButtonVolume, &QPushButton::clicked, this,
          &MainWindow::showVolumeSlider);
  connect(ui_->ButtonBluetooth, &QPushButton::clicked, this,
          &MainWindow::setPairable);
  connect(ui_->ButtonMute, &QPushButton::clicked, this,
          &MainWindow::toggleMuteButton);
  connect(ui_->ButtonMute, &QPushButton::clicked, this, &MainWindow::setMute);
  connect(ui_->ButtonUnmute, &QPushButton::clicked, this,
          &MainWindow::toggleMuteButton);
  connect(ui_->ButtonUnmute, &QPushButton::clicked, this,
          &MainWindow::setUnMute);
  connect(ui_->ButtonWifi, &QPushButton::clicked, this,
          &MainWindow::openConnectDialog);
  connect(ui_->ButtonAndroidAuto, &QPushButton::clicked, this,
          &MainWindow::triggerAppStart);
  connect(ui_->ButtonAndroidAuto, &QPushButton::clicked, this,
          &MainWindow::setRetryUSBConnect);

  ui_->ButtonBluetooth->hide();
  ui_->Pairable->hide();

  ui_->ButtonAndroidAutoWidget->hide();

  if (!configuration->showNetworkInfo()) {
    ui_->NetworkInfo->hide();
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

  ui_->ButtonLock->hide();

  ui_->BluetoothDevice->hide();

  // check if a device is connected via bluetooth
  if (std::ifstream("/tmp/btdevice")) {
    if (ui_->BluetoothDevice->isHidden() ||
        ui_->BluetoothDevice->text().simplified() == "") {
      QString btdevicename = configuration_->readFileContent("/tmp/btdevice");
      ui_->BluetoothDevice->setText(btdevicename);
      ui_->BluetoothDevice->show();
    }
  } else {
    if (ui_->BluetoothDevice->isVisible()) {
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
  } else {
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

  tmpDirWatcher = new QFileSystemWatcher(this);
  tmpDirWatcher->addPath("/tmp");
  connect(tmpDirWatcher, &QFileSystemWatcher::directoryChanged, this,
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

void MainWindow::hostModeStateChanged(QBluetoothLocalDevice::HostMode mode) {
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

void MainWindow::updateNetworkInfo() {
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

void MainWindow::onClickButtonBrightness() {
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

void MainWindow::onClickButtonVolume() {
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

void MainWindow::onChangeSliderBrightness(int value) {
  int n = snprintf(this->brightnessStr, 5, "%d", value);
  this->brightnessFile = new QFile(this->brightnessFilename);
  this->brightnessFileAlt = new QFile(this->brightnessFilenameAlt);

  if (!this->customBrightnessControl) {
    if (this->brightnessFile->open(QIODevice::WriteOnly)) {
      this->brightnessStr[n] = '\n';
      this->brightnessStr[n + 1] = '\0';
      this->brightnessFile->write(this->brightnessStr);
      this->brightnessFile->close();
    }
  } else {
    if (this->brightnessFileAlt->open(QIODevice::WriteOnly)) {
      this->brightnessStr[n] = '\n';
      this->brightnessStr[n + 1] = '\0';
      this->brightnessFileAlt->write(this->brightnessStr);
      this->brightnessFileAlt->close();
    }
  }
  QString bri = QString::number(value);
  ui_->ValueBrightness->setText(bri);
}

void MainWindow::onChangeSliderVolume(int value) {
  QString vol = QString::number(value);
  ui_->ValueVolume->setText(vol + "%");
  system(
      ("/usr/local/bin/autoapp_helper setvolume " + std::to_string(value) + "&")
          .c_str());
}

void MainWindow::updateAlpha() {
  int value = configuration_->getAlphaTrans();
  // int n = snprintf(this->alpha_str, 5, "%d", value);

  if (value != this->alphaCurrentStr) {
    this->alphaCurrentStr = value;
    double alpha = value / 100.0;
    QString alp = QString::number(alpha);
    this->setAlpha(alp, ui_->ButtonExit);
    setAlpha(alp, ui_->ButtonShutdown);
    setAlpha(alp, ui_->ButtonReboot);
    setAlpha(alp, ui_->ButtonCancel);
    setAlpha(alp, ui_->ButtonBrightness);
    setAlpha(alp, ui_->ButtonVolume);
    setAlpha(alp, ui_->ButtonLock);
    setAlpha(alp, ui_->ButtonSettings);
    setAlpha(alp, ui_->ButtonDay);
    setAlpha(alp, ui_->ButtonNight);
    setAlpha(alp, ui_->ButtonWifi);
    setAlpha(alp, ui_->ButtonAndroidAuto);
    setAlpha(alp, ui_->LabelAndroidAutoTop);
    setAlpha(alp, ui_->LabelAndroidAutoBottom);
    setAlpha(alp, ui_->ButtonNoDevice);
    setAlpha(alp, ui_->ButtonNoWiFiDevice);
  }
}

void MainWindow::setAlpha(QString newAlpha, QWidget *widget) {
  widget->setStyleSheet(
      widget->styleSheet().replace(backgroundColor, newAlpha));
}

void MainWindow::switchGuiToNight() {
  // MainWindow::on_ButtonVolume_clicked();
  f1x::openauto::autoapp::ui::MainWindow::updateBG();
  ui_->ButtonDay->show();
  ui_->ButtonNight->hide();
  ui_->BrightnessWidget->hide();
}

void MainWindow::switchGuiToDay() {
  // MainWindow::on_ButtonVolume_clicked();
  f1x::openauto::autoapp::ui::MainWindow::updateBG();
  ui_->ButtonNight->show();
  ui_->ButtonDay->hide();
  ui_->BrightnessWidget->hide();
}

void MainWindow::toggleExit() {
  if (ui_->TilesFront->isVisible()) {
    ui_->TilesFront->hide();
    ui_->TilesBack->show();
  } else {
    ui_->TilesFront->show();
    ui_->TilesBack->hide();
  }
}

void MainWindow::toggleMuteButton() {
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

void MainWindow::updateBG() {
  if (this->dateText == "12/24") {
    this->setStyleSheet(
        "QMainWindow { background: url(:/wallpaper-christmas.png); "
        "background-repeat: no-repeat; background-position: center; }");
  } else if (this->dateText == "12/31") {
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

void MainWindow::createDebuglog() {
  system("/usr/local/bin/crankshaft debuglog &");
}

void MainWindow::setPairable() {
  system("/usr/local/bin/crankshaft bluetooth pairable &");
}

void MainWindow::setMute() {
  system("/usr/local/bin/autoapp_helper setmute &");
}

void MainWindow::setUnMute() {
  system("/usr/local/bin/autoapp_helper setunmute &");
}

void MainWindow::showTime() {
  QTime time = QTime::currentTime();
  QDate date = QDate::currentDate();
  QString time_text = time.toString("hh : mm : ss");
  this->dateText = date.toString("MM/dd");

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
      if (ui_->BluetoothDevice->isHidden()) {
        ui_->BluetoothDevice->show();
      }
      if (std::ifstream("/tmp/btdevice")) {
        ui_->BluetoothDevice->setText(
            configuration_->readFileContent("/tmp/btdevice"));
      }
    } else {
      if (ui_->BluetoothDevice->isVisible()) {
        ui_->BluetoothDevice->hide();
        ui_->BluetoothDevice->setText("BT-Device");
      }
    }
  }
}

void MainWindow::setRetryUSBConnect() {
  ui_->SysInfoTop->setText("Trying USB connect ...");

  QTimer::singleShot(10000, this, SLOT(resetRetryUSBMessage()));
}

void MainWindow::resetRetryUSBMessage() {
  ui_->SysInfoTop->clear();
}

bool MainWindow::doesFileExist(const char *fileName) {
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

void MainWindow::tmpChanged() {
  try {
    if (std::ifstream("/tmp/entityexit")) {
      MainWindow::triggerAppStop();
      std::remove("/tmp/entityexit");
    }
  } catch (...) {
    OPENAUTO_LOG(error) << "[OpenAuto] Error in entityexit";
  }

  // check if system is in display off mode (tap2wake)
  if (std::ifstream("/tmp/blankscreen")) {
    if (ui_->MainWidget->isVisible()) {
      closeAllDialogs();
      ui_->MainWidget->hide();
    }
  } else {
    if (ui_->MainWidget->isHidden()) {
      ui_->MainWidget->show();
    }
  }

  // check if system is in display off mode (tap2wake/screensaver)
  if (std::ifstream("/tmp/screensaver")) {
    if (ui_->Tiles->isVisible()) {
      ui_->Tiles->hide();
    }

    if (ui_->Header->isVisible()) {
      ui_->Header->hide();
      closeAllDialogs();
    }

    if (ui_->VolumeWidget->isVisible()) {
      ui_->VolumeWidget->hide();
    }
    if (ui_->BrightnessWidget->isVisible()) {
      ui_->BrightnessWidget->hide();
    }
  } else {
    if (ui_->Header->isHidden()) {
      ui_->Header->show();
    }
    if (ui_->VolumeWidget->isHidden()) {
      ui_->VolumeWidget->show();
    }
  }

  // check if custom command needs black background
  if (std::ifstream("/tmp/blackscreen")) {
    if (ui_->MainWidget->isVisible()) {
      ui_->MainWidget->hide();
      this->setStyleSheet("QMainWindow {background-color: rgb(0,0,0);}");
    }
  } else {
    MainWindow::updateBG();
  }

  // check if phone is conencted to usb
  if (std::ifstream("/tmp/android_device")) {
    if (ui_->ButtonAndroidAutoWidget->isHidden()) {
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
    if (ui_->ButtonAndroidAutoWidget->isVisible()) {
      ui_->ButtonNoDevice->show();
      ui_->ButtonAndroidAutoWidget->hide();
    }

    ui_->LabelAndroidAutoBottom->setText("");
  }

  // check if bluetooth pairable
  if (this->bluetoothEnabled) {
    if (std::ifstream("/tmp/bluetooth_pairable")) {
      if (ui_->Pairable->isHidden()) {
        ui_->Pairable->show();
      }
      if (ui_->ButtonBluetooth->isVisible()) {
        ui_->ButtonBluetooth->hide();
      }
    } else {
      if (ui_->Pairable->isVisible()) {
        ui_->Pairable->hide();
      }
      if (ui_->ButtonBluetooth->isHidden()) {
        ui_->ButtonBluetooth->show();
      }
    }
  } else {
    if (ui_->Pairable->isVisible()) {
      ui_->Pairable->hide();
    }
    if (ui_->ButtonBluetooth->isVisible()) {
      ui_->ButtonBluetooth->hide();
    }
  }

  if (std::ifstream("/tmp/config_in_progress") ||
      std::ifstream("/tmp/debug_in_progress") ||
      std::ifstream("/tmp/enable_pairing")) {\
      if (std::ifstream("/tmp/config_in_progress")) {
        ui_->ButtonSettings->hide();
        ui_->ButtonLock->show();
        ui_->SysInfoTop->setText("Config in progress ...");
      }
      if (std::ifstream("/tmp/debug_in_progress")) {
        ui_->ButtonSettings->hide();
        ui_->ButtonLock->show();
        ui_->SysInfoTop->setText("Creating debug.zip ...");
      }
      if (std::ifstream("/tmp/enable_pairing")) {
        ui_->SysInfoTop->setText("Pairing enabled for 120 seconds!");
      }
  } else {
      ui_->SysInfoTop->clear();
      ui_->ButtonSettings->show();
      ui_->ButtonLock->hide();
  }

  // update day/night state
  this->nightModeEnabled = doesFileExist("/tmp/night_mode_enabled");

  if (this->nightModeEnabled) {
    if (!this->dayNightModeState) {
      this->dayNightModeState = true;
      MainWindow::switchGuiToNight();
    }
  } else {
    if (this->dayNightModeState) {
      this->dayNightModeState = false;
      MainWindow::switchGuiToDay();
    }
  }

  // check if shutdown is external triggered and init clean app exit
  if (std::ifstream("/tmp/external_exit")) {
    MainWindow::exit();
  }

  this->hotspotActive = doesFileExist("/tmp/hotspot_active");

  // hide wifi if hotspot disabled and force wifi unselected
  if (!this->hotspotActive && !std::ifstream("/tmp/mobile_hotspot_detected")) {
    if ((ui_->AAWIFIWidget->isVisible())) {
      ui_->AAWIFIWidget->hide();
      ui_->AAUSBWidget->show();
    }
  } else {
    if ((ui_->AAWIFIWidget->isHidden())) {
      ui_->AAWIFIWidget->show();
      ui_->AAUSBWidget->hide();
    }
  }

  if (std::ifstream("/tmp/temp_recent_list") ||
      std::ifstream("/tmp/mobile_hotspot_detected")) {
    if (ui_->ButtonWifi->isHidden()) {
      ui_->ButtonWifi->show();
    }
    if (ui_->ButtonNoWiFiDevice->isVisible()) {
      ui_->ButtonNoWiFiDevice->hide();
    }
  } else {
    if (ui_->ButtonWifi->isVisible()) {
      ui_->ButtonWifi->hide();
    }
    if (ui_->ButtonNoWiFiDevice->isHidden()) {
      ui_->ButtonNoWiFiDevice->show();
    }
  }

  // clock viibility by settings
  if (!this->configuration_->showClock()) {
    ui_->Clock->hide();
  } else {
    ui_->Clock->show();
  }

  if (!this->configuration_->showNetworkInfo()) {
    if (ui_->NetworkInfo->isVisible()) {
      ui_->NetworkInfo->hide();
    }
  } else {
    if (ui_->NetworkInfo->isHidden()) {
      ui_->NetworkInfo->show();
    }
  }

  // hide brightness button if eanbled in settings
  if (configuration_->hideBrightnessControl()) {
    if ((ui_->ButtonBrightness->isVisible()) ||
        (ui_->ButtonBrightness->isVisible()) ||
        (ui_->BrightnessWidget->isVisible())) {
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
    if (ui_->Lock->isHidden()) {
      ui_->Lock->show();
    }
  } else {
    if (ui_->Lock->isVisible()) {
      ui_->Lock->hide();
    }
  }
  updateNetworkInfo();
}

}  // namespace ui
}  // namespace autoapp
}  // namespace openauto
}  // namespace f1x
