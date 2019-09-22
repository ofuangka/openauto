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
  if (doesFileExist(PATH_BT_DEVICE.c_str())) {
    ui->BluetoothDevice->setText(cfg->readFileContent(PATH_BT_DEVICE.c_str()));
  } else {
    ui->BluetoothDevice->clear();
  }
  bool bluetoothPairable = doesFileExist(PATH_BT_PAIRABLE.c_str());
  ui->Pairable->setVisible(bluetoothPairable);
  ui->ButtonBluetooth->setDisabled(bluetoothPairable);
}

MainWindow::MainWindow(configuration::IConfiguration::Pointer cfg,
                       QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      localDevice(new QBluetoothLocalDevice) {
  this->cfg = cfg;

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
          &MainWindow::rotateSliders);
  connect(ui->ButtonVolume, &QPushButton::clicked, this,
          &MainWindow::rotateSliders);
  connect(ui->ButtonTransparency, &QPushButton::clicked, this,
          &MainWindow::rotateSliders);
  connect(ui->ButtonBackground, &QPushButton::clicked, this,
          &MainWindow::rotateSliders);
  connect(ui->ButtonBluetooth, &QPushButton::clicked, this,
          &MainWindow::enablePairing);
  connect(ui->ButtonMute, &QPushButton::clicked, this, &MainWindow::toggleMute);
  connect(ui->ButtonAndroidAuto, &QPushButton::clicked, this,
          &MainWindow::appStartEvent);
  connect(ui->ButtonAndroidAuto, &QPushButton::clicked, this,
          &MainWindow::setRetryUsbConnect);

  readHostCapabilities();
  readConfig();

  QTimer *timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(clockTick()));
  timer->start(1000);

  lockSettings(false);

  setPowerMenuVisibility(false);

  // init alpha values
  setNightMode(forceNightMode);
  updateTransparency();

  sliders = new QList<QWidget *>();
  initSliders();
  initStatuses();

  triggerWatch = new QFileSystemWatcher(this);
  triggerWatch->addPath("/tmp");
  connect(triggerWatch, &QFileSystemWatcher::directoryChanged, this,
          &MainWindow::onTrigger);

  // Experimental test code
  localDevice = new QBluetoothLocalDevice(this);

  connect(localDevice,
          SIGNAL(hostModeStateChanged(QBluetoothLocalDevice::HostMode)), this,
          SLOT(onChangeHostMode(QBluetoothLocalDevice::HostMode)));

  onChangeHostMode(localDevice->hostMode());
}

MainWindow::~MainWindow() {
  delete ui;
  delete sliders;
  delete triggerWatch;
}

void MainWindow::readHostCapabilities() {
  // trigger files
  forceNightMode = doesFileExist(PATH_FORCE_NIGHT_MODE.c_str());
  forceEnableWifi = doesFileExist(PATH_FORCE_WIFI.c_str());
  forceEnableBrightness = doesFileExist(PATH_FORCE_BRIGHTNESS.c_str());

  // wallpaper stuff
  wallpaperDayExists = doesFileExist(PATH_WALLPAPER.c_str());
  wallpaperNightExists = doesFileExist(PATH_WALLPAPER_NIGHT.c_str());
}

void MainWindow::readConfig() {
  // set brightness slider attribs from cs config
  ui->SliderBrightness->setMinimum(cfg->getCSValue("BR_MIN").toInt());
  ui->SliderBrightness->setMaximum(cfg->getCSValue("BR_MAX").toInt());
  ui->SliderBrightness->setSingleStep(cfg->getCSValue("BR_STEP").toInt());
  ui->SliderBrightness->setTickInterval(cfg->getCSValue("BR_STEP").toInt());

  // clock visibility
  ui->Clock->setVisible(!!cfg->showClock());

  if (!forceEnableWifi) {
    ui->WifiWidget->hide();
  } else {
    if (doesFileExist(PATH_RECENT_SSIDS.c_str()) ||
        doesFileExist(PATH_HOTSPOT_DETECTED.c_str())) {
      ui->ButtonWifi->show();
      ui->ButtonNoWifiDevice->hide();
    } else {
      ui->ButtonWifi->hide();
      ui->ButtonNoWifiDevice->show();
    }
  }
}

void MainWindow::initStatuses() {
  ui->Locked->hide();
  ui->Pairable->hide();
}

void MainWindow::lockSettings(bool lock) {
  ui->ButtonSettings->setEnabled(!lock);
}

void MainWindow::onChangeHostMode(QBluetoothLocalDevice::HostMode mode) {
  if (mode != QBluetoothLocalDevice::HostPoweredOff) {
    refreshBluetooth();
  }
}

void MainWindow::initSliders() {
  sliders->append(ui->VolumeSlider);
  sliders->append(ui->BrightnessSlider);
  sliders->append(ui->TransparencySlider);

  // hide brightness slider if control file is not existing
  QFileInfo brightnessFile(PATH_BRIGHTNESS.c_str());
  if (forceEnableBrightness || brightnessFile.exists()) {
    sliders->append(ui->BackgroundSlider);
  } else {
    ui->BackgroundSlider->hide();
  }
  for (int i = 0; i < sliders->length(); i++) {
    sliders->at(i)->hide();
  }
  rotateSliders();
}

void MainWindow::rotateSliders() {
  int i = 0;
  while (i < sliders->length() && sliders->at(i)->isHidden()) {
    i++;
  }
  if (i == sliders->length()) {
    i--;
  }
  sliders->at(i)->setVisible(false);
  sliders->at((i + 1) % sliders->length())->setVisible(true);
}

void MainWindow::onChangeBrightness(int value) {
  char *brightnessStr = new char[5];
  int n = snprintf(brightnessStr, 5, "%d", value);
  QFile *brightnessFile = new QFile(PATH_BRIGHTNESS.c_str());

  if (brightnessFile->open(QIODevice::WriteOnly)) {
    brightnessStr[n] = '\n';
    brightnessStr[n + 1] = '\0';
    brightnessFile->write(brightnessStr);
    brightnessFile->close();
  }
}

void MainWindow::onChangeVolume(int value) { /* TODO: implement */
}

void MainWindow::updateTransparency() {
  int value = 50;
  // int n = snprintf(alpha_str, 5, "%d", value);

  if (value != currentAlpha) {
    currentAlpha = value;
    double alpha = value / 100.0;
    QString alp = QString::number(alpha);
    setAlpha(alp, ui->ButtonPower);
    setAlpha(alp, ui->ButtonShutdown);
    setAlpha(alp, ui->ButtonReboot);
    setAlpha(alp, ui->ButtonBack);
    setAlpha(alp, ui->ButtonBrightness);
    setAlpha(alp, ui->ButtonMute);
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
  QString newValue = widget->styleSheet().replace(REGEX_BACKGROUND_COLOR,
                                                  "rgba(\\1" + newAlpha);
  widget->setStyleSheet(newValue);
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
  std::string wallpaper = PATH_WALLPAPER_BLACK;
  if (wallpaperDayExists) {
    wallpaper = PATH_WALLPAPER;
  }
  if (ui->ButtonDay->isVisible() && wallpaperNightExists) {
    wallpaper = PATH_WALLPAPER_NIGHT;
  }
  setStyleSheet(
      styleSheet().replace(REGEX_BACKGROUND_IMAGE,
                           QString::fromStdString("url('" + wallpaper + "')")));
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

void MainWindow::clockTick() {
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
    if (devices.count() > 0 && std::ifstream(PATH_BT_DEVICE.c_str())) {
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

void MainWindow::onTrigger() {
  try {
    if (doesFileExist(PATH_APP_STOP.c_str())) {
      std::remove(PATH_APP_STOP.c_str());
      MainWindow::appStopEvent();
    }
  } catch (...) {
    OPENAUTO_LOG(error) << "[OpenAuto] Error in entityexit";
  }

  // check if system is in display off mode (tap2wake)
  bool blankScreen = doesFileExist(PATH_BLANK_SCREEN.c_str()) ||
                     doesFileExist(PATH_SCREENSAVER.c_str());
  if (blankScreen) {
    closeAllDialogs();
  } else {
    ui->MainWidget->setVisible(!blankScreen);
  }

  // check if custom command needs black background
  if (doesFileExist(PATH_BLACK_SCREEN.c_str())) {
    ui->MainWidget->hide();
    setStyleSheet("QMainWindow { background-color: rgb(0,0,0); }");
  } else {
    MainWindow::updateBg();
  }

  // check android device
  bool androidDevice = doesFileExist(PATH_ANDROID_DEVICE.c_str());
  ui->ButtonAndroidAuto->setVisible(androidDevice);
  ui->ButtonNoDevice->setVisible(!androidDevice);
  try {
    QFile deviceData(QString(PATH_ANDROID_DEVICE.c_str()));
    deviceData.open(QIODevice::ReadOnly);
    QTextStream data_date(&deviceData);
    QString linedate = data_date.readAll().split("\n")[1];
    deviceData.close();
  } catch (...) {
  }

  if (doesFileExist(PATH_CONFIG_IN_PROGRESS.c_str())) {
    lockSettings(true);
    ui->StatusMessage->setText("Config in progress...");
  } else if (doesFileExist(PATH_DEBUG_IN_PROGRESS.c_str())) {
    lockSettings(true);
    ui->StatusMessage->setText("Creating debug.zip...");
  } else {
    lockSettings(false);
    ui->StatusMessage->clear();
  }

  // check if shutdown is external triggered and init clean app exit
  if (doesFileExist(PATH_EXTERNAL_EXIT.c_str())) {
    MainWindow::shutdown();
  }

  hotspotActive = doesFileExist(PATH_HOTSPOT_ACTIVE.c_str());

  // hide wifi if hotspot disabled and force wifi unselected
  bool forceNoWifi =
      !hotspotActive && !doesFileExist(PATH_HOTSPOT_DETECTED.c_str());
  ui->WifiWidget->setVisible(!forceNoWifi);
  ui->UsbWidget->setVisible(forceNoWifi);

  if (doesFileExist(PATH_RECENT_SSIDS.c_str()) ||
      doesFileExist(PATH_HOTSPOT_DETECTED.c_str())) {
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
