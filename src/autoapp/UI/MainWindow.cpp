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

MainWindow::MainWindow(configuration::IConfiguration::Pointer cfg,
                       QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  this->cfg = cfg;
  ui->setupUi(this);
  initUi();
  readHostCapabilities();
  readConfig();

  QTimer *timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(clockTick()));
  timer->start(1000);

  triggerWatch = new QFileSystemWatcher(this);
  triggerWatch->addPath(PATH_TRIGGER_DIR.c_str());
  connect(triggerWatch, &QFileSystemWatcher::directoryChanged, this,
          &MainWindow::onTrigger);
}

MainWindow::~MainWindow() {
  delete ui;
  delete sliders;
  delete triggerWatch;
  delete timeFormat;
}

void MainWindow::initUi() {
  connect(ui->TileSettings, &QPushButton::clicked, this,
          &MainWindow::openSettings);
  connect(ui->TileWifi, &QPushButton::clicked, this,
          &MainWindow::openConnectDialog);
  connect(ui->TileAndroidAuto, &QPushButton::clicked, this,
          &MainWindow::appStartEvent);
  connect(ui->TileAndroidAuto, &QPushButton::clicked, this,
          &MainWindow::showUsbConnectMessage);
  connect(ui->TilePowerMenu, &QPushButton::clicked, this, &MainWindow::powerMenu);
  connect(ui->TileBack, &QPushButton::clicked, this,
          &MainWindow::closePowerMenu);
  connect(ui->TileShutdown, &QPushButton::clicked, this, &MainWindow::shutdown);
  connect(ui->TileReboot, &QPushButton::clicked, this, &MainWindow::reboot);
  connect(ui->TileDay, &QPushButton::clicked, this, &MainWindow::dayModeEvent);
  connect(ui->TileDay, &QPushButton::clicked, this, &MainWindow::dayMode);
  connect(ui->TileNight, &QPushButton::clicked, this,
          &MainWindow::nightModeEvent);
  connect(ui->TileNight, &QPushButton::clicked, this, &MainWindow::nightMode);
  connect(ui->TileMute, &QPushButton::clicked, this, &MainWindow::toggleMute);
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

  /* hide conditional/transient UI elements */
  ui->Locked->hide();
  ui->Pairable->hide();
  ui->Muted->hide();
  ui->Clock->hide();
  ui->TileDay->hide();
  ui->TileNight->hide();
  ui->TileAndroidAuto->hide();
  ui->ButtonBluetooth->hide();
  ui->WifiWidget->hide();
  setPowerMenuVisibility(false);

  ui->BluetoothDevice->hide();
  ui->Divider->hide();
  ui->Ssid->hide();
  ui->StatusMessage->clear();
}

void MainWindow::readHostCapabilities() {
  // force files
  forceNightMode = doesFileExist(PATH_FORCE_NIGHT_MODE.c_str());
  forceEnableWifi = doesFileExist(PATH_FORCE_WIFI.c_str());
  forceEnableBrightness = doesFileExist(PATH_FORCE_BRIGHTNESS.c_str());

  // wallpaper
  wallpaperDayExists = doesFileExist(PATH_WALLPAPER.c_str());
  wallpaperNightExists = doesFileExist(PATH_WALLPAPER_NIGHT.c_str());

  /* default to day mode */
  if (wallpaperDayExists && wallpaperNightExists) {
    ui->TileNight->show();
    updateBg();
  }

  // sliders
  sliders = new QList<QWidget *>();
  sliders->append(ui->VolumeSlider);
  if (!forceEnableBrightness && !doesFileExist(PATH_BRIGHTNESS.c_str())) {
    ui->BrightnessSlider->hide();
  } else {
    sliders->append(ui->BrightnessSlider);
  }
  sliders->append(ui->TransparencySlider);
  sliders->append(ui->BackgroundSlider);
  for (int i = 0; i < sliders->length(); i++) {
    sliders->at(i)->hide();
  }
  rotateSliders();
}

void MainWindow::readConfig() {
  // set brightness slider attribs from cs config
  ui->SliderBrightness->setMinimum(cfg->getCSValue("BR_MIN").toInt());
  ui->SliderBrightness->setMaximum(cfg->getCSValue("BR_MAX").toInt());
  ui->SliderBrightness->setSingleStep(cfg->getCSValue("BR_STEP").toInt());
  ui->SliderBrightness->setTickInterval(cfg->getCSValue("BR_STEP").toInt());

  // TODO: set volume, transparency and background
  ui->SliderTransparency->setMinimum(0);
  ui->SliderTransparency->setMaximum(100);
  ui->SliderTransparency->setValue(75);

  ui->SliderBackground->setMinimum(0);
  ui->SliderBackground->setMaximum(100);
  ui->SliderBackground->setValue(25);

  // clock visibility
  ui->Clock->setVisible(!!cfg->showClock());

  timeFormat = new QString("hh:mmap");

  updateTransparency();
}

void MainWindow::lockSettings(bool lock) {
  ui->TileSettings->setEnabled(!lock);
  ui->Locked->setVisible(lock);
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
  int value = ui->SliderTransparency->value();

  if (value != currentAlpha) {
    currentAlpha = value;
    double alpha = value / 100.0;
    QString alp = QString::number(alpha);
    setAlpha(alp, ui->TilePowerMenu);
    setAlpha(alp, ui->TileShutdown);
    setAlpha(alp, ui->TileReboot);
    setAlpha(alp, ui->TileBack);
    setAlpha(alp, ui->TileMute);
    setAlpha(alp, ui->TileSettings);
    setAlpha(alp, ui->TileDay);
    setAlpha(alp, ui->TileNight);
    setAlpha(alp, ui->TileWifi);
    setAlpha(alp, ui->TileAndroidAuto);
    setAlpha(alp, ui->TileNoUsbDevice);
    setAlpha(alp, ui->TileNoWifiDevice);
    setAlpha(alp, ui->ButtonBrightness);
  }
}

void MainWindow::setAlpha(QString newAlpha, QWidget *widget) {
  QString newValue = widget->styleSheet().replace(REGEX_BACKGROUND_COLOR,
                                                  "rgba(\\1" + newAlpha);
  widget->setStyleSheet(newValue);
}

void MainWindow::setNightMode(bool enabled) {
  ui->TileDay->setVisible(enabled);
  ui->TileNight->setVisible(!enabled);
  updateBg();
}

void MainWindow::dayMode() { setNightMode(false); }
void MainWindow::nightMode() { setNightMode(true); }

void MainWindow::setPowerMenuVisibility(bool visible) {
  ui->TilePowerMenu->setVisible(!visible);
  ui->PowerMenu->setVisible(visible);
}

void MainWindow::powerMenu() { setPowerMenuVisibility(true); }
void MainWindow::closePowerMenu() { setPowerMenuVisibility(false); }

void MainWindow::updateBg() {
  std::string wallpaper = PATH_WALLPAPER_BLACK;
  if (wallpaperDayExists) {
    wallpaper = PATH_WALLPAPER;
  }
  if (ui->TileDay->isVisible() && wallpaperNightExists) {
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
  ui->TileMute->setChecked(muted);
  ui->Muted->setVisible(muted);
}

void MainWindow::toggleMute() { setMuted(true); }

void MainWindow::clockTick() {
  QTime time = QTime::currentTime();
  ui->Clock->setText(time.toString(*timeFormat));
}

void MainWindow::showUsbConnectMessage() {
  ui->StatusMessage->setText("Trying USB connect...");
  QTimer::singleShot(10000, this, SLOT(clearStatusMessage()));
}

void MainWindow::clearStatusMessage() { ui->StatusMessage->clear(); }

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
  bool shouldStopAndroidAuto = doesFileExist(PATH_APP_STOP.c_str());
  bool androidDevice = doesFileExist(PATH_ANDROID_DEVICE.c_str());
  bool shouldLockSettings = doesFileExist(PATH_CONFIG_IN_PROGRESS.c_str());
  bool shouldExit = doesFileExist(PATH_EXTERNAL_EXIT.c_str());
  bool hotspotActive = doesFileExist(PATH_HOTSPOT_ACTIVE.c_str());
  bool hotspotDetected = doesFileExist(PATH_HOTSPOT_DETECTED.c_str());
  bool bluetoothDevice = doesFileExist(PATH_BT_DEVICE.c_str());
  bool bluetoothPairable = doesFileExist(PATH_BT_PAIRABLE.c_str());

  if (shouldStopAndroidAuto) {
    std::remove(PATH_APP_STOP.c_str());
    try {
      appStopEvent();
    } catch (...) {
      OPENAUTO_LOG(error) << "Error exiting Android Auto";
    }
  }

  // check android device
  ui->TileAndroidAuto->setVisible(androidDevice);
  ui->TileNoUsbDevice->setVisible(!androidDevice);
  if (androidDevice) {
    ui->TileAndroidAuto->show();
    ui->TileNoUsbDevice->hide();
    try {
      QFile deviceInfo(QString(PATH_ANDROID_DEVICE.c_str()));
      deviceInfo.open(QIODevice::ReadOnly);
      QTextStream deviceData(&deviceInfo);
      QString deviceName = deviceData.readAll().split("\n")[1];
      deviceInfo.close();
    } catch (...) {
      OPENAUTO_LOG(error) << "Error reading device name";
    }
  } else {
    ui->TileAndroidAuto->hide();
    ui->TileNoUsbDevice->show();
  }

  if (shouldLockSettings) {
    lockSettings(true);
    ui->StatusMessage->setText("Config in progress...");
  } else {
    lockSettings(false);
    ui->StatusMessage->clear();
  }

  if (shouldExit) {
    std::remove(PATH_EXTERNAL_EXIT.c_str());
    shutdown();
  }

  if (!hotspotActive && !hotspotDetected) {
    ui->WifiWidget->hide();
  } else {
    ui->WifiWidget->show();
  }

  if (bluetoothDevice) {
    ui->BluetoothDevice->setText(cfg->readFileContent(PATH_BT_DEVICE.c_str()));
  } else {
    ui->BluetoothDevice->clear();
  }
  ui->Pairable->setVisible(bluetoothPairable);
  ui->ButtonBluetooth->setDisabled(bluetoothPairable);
}

}  // namespace ui
}  // namespace autoapp
}  // namespace openauto
}  // namespace f1x
