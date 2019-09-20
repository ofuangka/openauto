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

void MainWindow::refreshBluetooth() {
  if (doesFileExist("/tmp/btdevice")) {
    ui->BluetoothDevice->setText(cfg->readFileContent("/tmp/btdevice"));
  } else {
    ui->BluetoothDevice->clear();
  }
  bool bluetoothPairable = doesFileExist("/tmp/bluetooth_pairable");
  ui->Pairable->setVisible(bluetoothPairable);
  ui->ButtonBluetooth->setDisabled(bluetoothPairable);
}

MainWindow::MainWindow(configuration::IConfiguration::Pointer cfg,
                       QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      localDevice(new QBluetoothLocalDevice) {
  this->cfg = cfg;

  // trigger files
  forceNightMode = doesFileExist(PATH_FORCE_NIGHT_MODE.c_str());
  forceEnableWifi = doesFileExist(PATH_FORCE_WIFI.c_str());
  forceEnableBrightness = doesFileExist(PATH_FORCE_BRIGHTNESS.c_str());

  // wallpaper stuff
  wallpaperDayFileExists = doesFileExist("wallpaper.png");
  wallpaperNightFileExists = doesFileExist("wallpaper-night.png");

  ui->setupUi(this);
  connect(ui->ButtonSettings, &QPushButton::clicked, this,
          &MainWindow::openSettings);
  connect(ui->ButtonWifi, &QPushButton::clicked, this,
          &MainWindow::openConnectDialog);
  connect(ui->ButtonPower, &QPushButton::clicked, this, &MainWindow::powerMenu);
  connect(ui->ButtonBack, &QPushButton::clicked, this,
          &MainWindow::closePowerMenu);
  connect(ui->ButtonShutdown, &QPushButton::clicked, this,
          &MainWindow::shutdown);
  connect(ui->ButtonReboot, &QPushButton::clicked, this, &MainWindow::reboot);
  connect(ui->ButtonDay, &QPushButton::clicked, this,
          &MainWindow::dayModeEvent);
  connect(ui->ButtonDay, &QPushButton::clicked, this, &MainWindow::dayMode);
  connect(ui->ButtonNight, &QPushButton::clicked, this,
          &MainWindow::nightModeEvent);
  connect(ui->ButtonNight, &QPushButton::clicked, this, &MainWindow::nightMode);
  connect(ui->ButtonBrightness, &QPushButton::clicked, this,
          &MainWindow::rotateSlider);
  connect(ui->ButtonVolume, &QPushButton::clicked, this,
          &MainWindow::rotateSlider);
  connect(ui->ButtonTransparency, &QPushButton::clicked, this,
          &MainWindow::rotateSlider);
  connect(ui->ButtonBackground, &QPushButton::clicked, this,
          &MainWindow::rotateSlider);
  connect(ui->ButtonBluetooth, &QPushButton::clicked, this,
          &MainWindow::enablePairing);
  connect(ui->ButtonMute, &QPushButton::clicked, this, &MainWindow::toggleMute);
  connect(ui->ButtonAndroidAuto, &QPushButton::clicked, this,
          &MainWindow::appStartEvent);
  connect(ui->ButtonAndroidAuto, &QPushButton::clicked, this,
          &MainWindow::setRetryUsbConnect);

  ui->ButtonAndroidAuto->hide();

  QTimer *timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(showTime()));
  timer->start(1000);

  lockSettings(false);

  // hide brightness slider if control file is not existing
  QFileInfo brightnessFile(PATH_BRIGHTNESS.c_str());
  if (!brightnessFile.exists() && !forceEnableBrightness) {
    ui->ButtonBrightness->hide();
  }

  // as default hide power buttons
  ui->PowerMenu->hide();

  // hide wifi if not forced
  if (!forceEnableWifi && !std::ifstream("/tmp/mobile_hotspot_detected")) {
    ui->WifiWidget->hide();
  } else {
    ui->UsbWidget->hide();
  }

  if (std::ifstream("/tmp/temp_recent_list") ||
      std::ifstream("/tmp/mobile_hotspot_detected")) {
    ui->ButtonWifi->show();
    ui->ButtonNoWifiDevice->hide();
  } else {
    ui->ButtonWifi->hide();
    ui->ButtonNoWifiDevice->show();
  }

  // set brightness slider attribs from cs config
  ui->SliderBrightness->setMinimum(cfg->getCSValue("BR_MIN").toInt());
  ui->SliderBrightness->setMaximum(cfg->getCSValue("BR_MAX").toInt());
  ui->SliderBrightness->setSingleStep(cfg->getCSValue("BR_STEP").toInt());
  ui->SliderBrightness->setTickInterval(cfg->getCSValue("BR_STEP").toInt());

  // run monitor for custom brightness command if enabled in crankshaft_env.sh
  if (std::ifstream("/tmp/custombrightness")) {
    if (!cfg->hideBrightnessControl()) {
      ui->ButtonBrightness->show();
    }
    customBrightnessControl = true;
  }

  // read param file
  if (std::ifstream("/boot/crankshaft/volume")) {
    // init volume
    QString vol = QString::number(
        cfg->readFileContent("/boot/crankshaft/volume").toInt());
    ui->SliderVolume->setValue(vol.toInt());
  }

  // set bg's on startup
  if (forceNightMode) {
    nightMode();
  } else {
    dayMode();
  }

  // clock viibility by settings
  if (!cfg->showClock()) {
    ui->Clock->hide();
  } else {
    ui->Clock->show();
  }

  // init alpha values
  updateTransparency();

  rotateSlider();

  tmpDirWatcher = new QFileSystemWatcher(this);
  tmpDirWatcher->addPath("/tmp");
  connect(tmpDirWatcher, &QFileSystemWatcher::directoryChanged, this,
          &MainWindow::onChangeTmpDir);

  // Experimental test code
  localDevice = new QBluetoothLocalDevice(this);

  connect(localDevice,
          SIGNAL(onChangeHostMode(QBluetoothLocalDevice::HostMode)), this,
          SLOT(onChangeHostMode(QBluetoothLocalDevice::HostMode)));

  onChangeHostMode(localDevice->hostMode());
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::lockSettings(bool lock) {
  ui->ButtonSettings->setVisible(!lock);
  ui->ButtonLock->setVisible(lock);
}

void MainWindow::onChangeHostMode(QBluetoothLocalDevice::HostMode mode) {
  if (mode != QBluetoothLocalDevice::HostPoweredOff) {
    refreshBluetooth();
  }
}

void MainWindow::rotateSlider() {
  if (ui->VolumeSlider->isVisible()) {
    ui->VolumeSlider->setVisible(false);
    ui->BrightnessSlider->setVisible(true);
    ui->TransparencySlider->setVisible(false);
    ui->BackgroundSlider->setVisible(false);
  } else if (ui->BrightnessSlider->isVisible()) {
    ui->VolumeSlider->setVisible(false);
    ui->BrightnessSlider->setVisible(false);
    ui->TransparencySlider->setVisible(true);
    ui->BackgroundSlider->setVisible(false);
  } else if (ui->TransparencySlider->isVisible()) {
    ui->VolumeSlider->setVisible(false);
    ui->BrightnessSlider->setVisible(false);
    ui->TransparencySlider->setVisible(false);
    ui->BackgroundSlider->setVisible(true);
  } else {
    ui->VolumeSlider->setVisible(true);
    ui->BrightnessSlider->setVisible(false);
    ui->TransparencySlider->setVisible(false);
    ui->BackgroundSlider->setVisible(false);
  }

  /*
  brightnessFile = new QFile(brightnessFilename);

  // Get the current brightness value
  if (!customBrightnessControl && brightnessFile->open(QIODevice::ReadOnly)) {
    QByteArray data = brightnessFile->readAll();
    std::string::size_type sz;
    int brightness_val = std::stoi(data.toStdString(), &sz);
    ui->SliderBrightness->setValue(brightness_val);
    QString bri = QString::number(brightness_val);
    brightnessFile->close();
  }
  ui->SliderVolume->setVisible(false);
  ui->SliderBrightness->setVisible(true);
  */
}

void MainWindow::onChangeBrightness(int value) {
  int n = snprintf(brightnessStr, 5, "%d", value);
  QFile *brightnessFile = new QFile(PATH_BRIGHTNESS.c_str());

  if (!customBrightnessControl) {
    if (brightnessFile->open(QIODevice::WriteOnly)) {
      brightnessStr[n] = '\n';
      brightnessStr[n + 1] = '\0';
      brightnessFile->write(brightnessStr);
      brightnessFile->close();
    }
  }
  QString bri = QString::number(value);
}

void MainWindow::onChangeVolume(int value) {
  QString vol = QString::number(value);
  system(
      ("/usr/local/bin/autoapp_helper setvolume " + std::to_string(value) + "&")
          .c_str());
}

void MainWindow::updateTransparency() {
  int value = 50;
  // int n = snprintf(alpha_str, 5, "%d", value);

  if (value != alphaCurrentStr) {
    alphaCurrentStr = value;
    double alpha = value / 100.0;
    QString alp = QString::number(alpha);
    setAlpha(alp, ui->ButtonPower);
    setAlpha(alp, ui->ButtonShutdown);
    setAlpha(alp, ui->ButtonReboot);
    setAlpha(alp, ui->ButtonBack);
    setAlpha(alp, ui->ButtonBrightness);
    setAlpha(alp, ui->ButtonVolume);
    setAlpha(alp, ui->ButtonLock);
    setAlpha(alp, ui->ButtonSettings);
    setAlpha(alp, ui->ButtonDay);
    setAlpha(alp, ui->ButtonNight);
    setAlpha(alp, ui->ButtonWifi);
    setAlpha(alp, ui->ButtonAndroidAuto);
    setAlpha(alp, ui->ButtonNoDevice);
    setAlpha(alp, ui->ButtonNoWifiDevice);
  }
}

void MainWindow::setAlpha(QString newAlpha, QWidget *widget) {
  widget->setStyleSheet(
      widget->styleSheet().replace(backgroundColor, newAlpha));
}

void MainWindow::setNightMode(bool enabled) {
  ui->ButtonDay->setVisible(enabled);
  ui->ButtonNight->setVisible(!enabled);
  updateBg();
}

void MainWindow::dayMode() { setNightMode(false); }
void MainWindow::nightMode() { setNightMode(true); }

void MainWindow::setPowerMenuVisibility(bool visible) {
  ui->ButtonPower->setVisible(!visible);
  ui->PowerMenu->setVisible(visible);
}

void MainWindow::powerMenu() { setPowerMenuVisibility(true); }
void MainWindow::closePowerMenu() { setPowerMenuVisibility(false); }

void MainWindow::updateBg() {
  if (wallpaperDayFileExists && ui->ButtonNight->isVisible()) {
    setStyleSheet(
        "QMainWindow { background: url(wallpaper.png);"
        "background-repeat: no-repeat; background-position: center; }");
  } else if (wallpaperNightFileExists) {
    setStyleSheet(
        "QMainWindow { background: url(wallpaper-night.png);"
        "background-repeat: no-repeat; background-position: "
        "center; }");
  } else {
    setStyleSheet(
        "QMainWindow { background: url(:/black.png); background-repeat: "
        "repeat; background-position: center; }");
  }
}

void MainWindow::enablePairing() {
  system("/usr/local/bin/crankshaft bluetooth pairable &");
}

void MainWindow::setMuted(bool muted) {
  if (muted) {
    system("/usr/local/bin/autoapp_helper setmute &");
  } else {
    system("/usr/local/bin/autoapp_helper setunmute &");
  }
  ui->ButtonMute->setChecked(muted);
}

void MainWindow::toggleMute() { setMuted(true); }

void MainWindow::showTime() {
  QTime time = QTime::currentTime();
  QString formattedTime = time.toString("hh:mmap");

  if ((time.second() % 2) == 0) {
    formattedTime[2] = ' ';
  }

  ui->Clock->setText(formattedTime);

  // check connected devices
  if (localDevice->isValid()) {
    QString localDeviceName = localDevice->name();
    QString localDeviceAddress = localDevice->address().toString();
    QList<QBluetoothAddress> devices;
    devices = localDevice->connectedDevices();
    if (devices.count() > 0 && std::ifstream("/tmp/btdevice")) {
      refreshBluetooth();
    }
  }
}

void MainWindow::setRetryUsbConnect() {
  ui->StatusMessage->setText("Trying USB connect...");

  QTimer::singleShot(10000, this, SLOT(resetRetryUsbMessage()));
}

void MainWindow::resetRetryUsbMessage() { ui->StatusMessage->clear(); }

bool MainWindow::doesFileExist(const char *path) {
  std::ifstream file(path, std::ios::in);
  if (file.good()) {
    return true;
  }
  // file not ok - checking if symlink
  QFileInfo fileInfo = QString(path);
  if (fileInfo.isSymLink()) {
    QFileInfo linkTarget(fileInfo.symLinkTarget());
    return linkTarget.exists();
  }
  return false;
}

void MainWindow::onChangeTmpDir() {
  try {
    if (doesFileExist("/tmp/entityexit")) {
      std::remove("/tmp/entityexit");
      MainWindow::appStopEvent();
    }
  } catch (...) {
    OPENAUTO_LOG(error) << "[OpenAuto] Error in entityexit";
  }

  // check if system is in display off mode (tap2wake)
  bool blankScreen =
      doesFileExist("/tmp/blankscreen") || doesFileExist("/tmp/screensaver");
  if (blankScreen) {
    closeAllDialogs();
  } else {
    ui->MainWidget->setVisible(!blankScreen);
  }

  // check if custom command needs black background
  if (doesFileExist("/tmp/blackscreen")) {
    ui->MainWidget->hide();
    setStyleSheet("QMainWindow { background-color: rgb(0,0,0); }");
  } else {
    MainWindow::updateBg();
  }

  // check android device
  bool androidDevice = doesFileExist("/tmp/android_device");
  ui->ButtonAndroidAuto->setVisible(androidDevice);
  ui->ButtonNoDevice->setVisible(!androidDevice);
  try {
    QFile deviceData(QString("/tmp/android_device"));
    deviceData.open(QIODevice::ReadOnly);
    QTextStream data_date(&deviceData);
    QString linedate = data_date.readAll().split("\n")[1];
    deviceData.close();
  } catch (...) {
  }

  if (doesFileExist("/tmp/config_in_progress")) {
    lockSettings(true);
    ui->StatusMessage->setText("Config in progress...");
  } else if (doesFileExist("/tmp/debug_in_progress")) {
    lockSettings(true);
    ui->StatusMessage->setText("Creating debug.zip...");
  } else {
    lockSettings(false);
    ui->StatusMessage->clear();
  }

  // update day/night state
  setNightMode(doesFileExist("/tmp/night_mode_enabled"));

  // check if shutdown is external triggered and init clean app exit
  if (doesFileExist("/tmp/external_exit")) {
    MainWindow::shutdown();
  }

  hotspotActive = doesFileExist("/tmp/hotspot_active");

  // hide wifi if hotspot disabled and force wifi unselected
  bool forceNoWifi =
      !hotspotActive && !doesFileExist("/tmp/mobile_hotspot_detected");
  ui->WifiWidget->setVisible(!forceNoWifi);
  ui->UsbWidget->setVisible(forceNoWifi);

  if (doesFileExist("/tmp/temp_recent_list") ||
      doesFileExist("/tmp/mobile_hotspot_detected")) {
    ui->ButtonWifi->show();
    ui->ButtonNoWifiDevice->hide();
  } else {
    ui->ButtonWifi->hide();
    ui->ButtonNoWifiDevice->show();
  }

  // clock viibility by settings
  if (!cfg->showClock()) {
    ui->Clock->hide();
  } else {
    ui->Clock->show();
  }

  // hide brightness button
  bool shouldHideBrightness = cfg->hideBrightnessControl();
  ui->ButtonBrightness->setVisible(!shouldHideBrightness);
  ui->SliderBrightness->setVisible(!shouldHideBrightness);
  ui->ButtonVolume->setVisible(shouldHideBrightness);

  updateTransparency();
}  // namespace ui

}  // namespace ui
}  // namespace autoapp
}  // namespace openauto
}  // namespace f1x
