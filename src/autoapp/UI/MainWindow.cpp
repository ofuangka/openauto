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

MainWindow::MainWindow(configuration::IConfiguration::Pointer cfg,
                       QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      localDevice(new QBluetoothLocalDevice) {
  this->cfg = cfg;

  setStyleSheet("color: rgb(255, 255, 255);");

  // trigger files
  forceNightMode = doesFileExist(nightModeFile);
  wifiButtonForce = doesFileExist(wifiButtonFile);
  brightnessButtonForce = doesFileExist(brightnessButtonFile);

  // wallpaper stuff
  wallpaperDayFileExists = doesFileExist("wallpaper.png");
  wallpaperNightFileExists = doesFileExist("wallpaper-night.png");

  ui->setupUi(this);
  connect(ui->ButtonSettings, &QPushButton::clicked, this,
          &MainWindow::openSettings);
  connect(ui->ButtonPower, &QPushButton::clicked, this,
          &MainWindow::showPowerMenu);
  connect(ui->ButtonShutdown, &QPushButton::clicked, this, &MainWindow::exit);
  connect(ui->ButtonReboot, &QPushButton::clicked, this, &MainWindow::reboot);
  connect(ui->ButtonBack, &QPushButton::clicked, this,
          &MainWindow::hidePowerMenu);
  connect(ui->ButtonDay, &QPushButton::clicked, this,
          &MainWindow::triggerScriptDay);
  connect(ui->ButtonDay, &QPushButton::clicked, this, &MainWindow::dayMode);
  connect(ui->ButtonNight, &QPushButton::clicked, this,
          &MainWindow::triggerScriptNight);
  connect(ui->ButtonNight, &QPushButton::clicked, this, &MainWindow::nightMode);
  connect(ui->ButtonBrightness, &QPushButton::clicked, this,
          &MainWindow::showSliderBrightness);
  connect(ui->ButtonVolume, &QPushButton::clicked, this,
          &MainWindow::showVolumeSlider);
  connect(ui->ButtonBluetooth, &QPushButton::clicked, this,
          &MainWindow::enablePairing);
  connect(ui->ButtonMute, &QPushButton::clicked, this, &MainWindow::mute);
  connect(ui->ButtonUnmute, &QPushButton::clicked, this, &MainWindow::unmute);
  connect(ui->ButtonWifi, &QPushButton::clicked, this,
          &MainWindow::openConnectDialog);
  connect(ui->ButtonAndroidAuto, &QPushButton::clicked, this,
          &MainWindow::triggerAppStart);
  connect(ui->ButtonAndroidAuto, &QPushButton::clicked, this,
          &MainWindow::setRetryUsbConnect);

  ui->ButtonBluetooth->hide();
  ui->Pairable->hide();
  ui->ButtonAndroidAuto->hide();

  if (!cfg->showNetworkInfo()) {
    ui->NetworkInfo->hide();
  }

  QTimer *timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(showTime()));
  timer->start(1000);

  ui->ButtonLock->hide();
  ui->BluetoothDevice->hide();

  // check if a device is connected via bluetooth
  if (std::ifstream("/tmp/btdevice")) {
    if (ui->BluetoothDevice->isHidden() ||
        ui->BluetoothDevice->text().simplified() == "") {
      QString btdevicename = cfg->readFileContent("/tmp/btdevice");
      ui->BluetoothDevice->setText(btdevicename);
      ui->BluetoothDevice->show();
    }
  } else {
    ui->BluetoothDevice->hide();
  }

  // hide brightness slider if control file is not existing
  QFileInfo brightnessFile(brightnessFilename);
  if (!brightnessFile.exists() && !brightnessButtonForce) {
    ui->ButtonBrightness->hide();
  }

  // as default hide brightness slider
  ui->BrightnessWidget->hide();

  // as default hide power buttons
  ui->PowerMenu->hide();

  // as default hide muted button
  ui->ButtonUnmute->hide();

  // hide wifi if not forced
  if (!wifiButtonForce && !std::ifstream("/tmp/mobile_hotspot_detected")) {
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
    ui->ValueVolume->setText(vol + "%");
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

  // hide brightness button if eanbled in settings
  if (cfg->hideBrightnessControl()) {
    ui->ButtonBrightness->hide();
    ui->BrightnessWidget->hide();

    // also hide volume button cause not needed if brightness not visible
    ui->ButtonVolume->hide();
  }

  // init alpha values
  updateAlpha();

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
  updateNetworkInfo();
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::onChangeHostMode(QBluetoothLocalDevice::HostMode mode) {
  if (mode != QBluetoothLocalDevice::HostPoweredOff) {
    bluetoothEnabled = true;
    ui->ButtonBluetooth->show();
    if (std::ifstream("/tmp/bluetooth_pairable")) {
      ui->Pairable->show();
      ui->ButtonBluetooth->hide();
    } else {
      ui->Pairable->hide();
    }
  } else {
    bluetoothEnabled = false;
    ui->ButtonBluetooth->hide();
    ui->Pairable->hide();
  }
}

void MainWindow::updateNetworkInfo() {
  QNetworkInterface wlan0if = QNetworkInterface::interfaceFromName("wlan0");
  if (wlan0if.flags().testFlag(QNetworkInterface::IsUp)) {
    QList<QNetworkAddressEntry> entrieswlan0 = wlan0if.addressEntries();
    if (!entrieswlan0.isEmpty()) {
      QNetworkAddressEntry wlan0 = entrieswlan0.first();
      // qDebug() << "wlan0: " << wlan0.ip();
      ui->ValueIp->setText(wlan0.ip().toString().simplified());
      ui->ValueMask->setText(wlan0.netmask().toString().simplified());
      if (std::ifstream("/tmp/hotspot_active")) {
        ui->ValueSsid->setText(
            cfg->getParamFromFile("/etc/hostapd/hostapd.conf", "ssid"));
      } else {
        ui->ValueSsid->setText(cfg->readFileContent("/tmp/wifi_ssid"));
      }
      ui->ValueGw->setText(cfg->readFileContent("/tmp/gateway_wlan0"));
    }
  } else {
    // qDebug() << "wlan0: down";
    ui->ValueIp->setText("");
    ui->ValueMask->setText("");
    ui->ValueGw->setText("");
    ui->ValueSsid->setText("wlan0: down");
  }
}

void MainWindow::onClickButtonBrightness() {
  brightnessFile = new QFile(brightnessFilename);
  brightnessFileAlt = new QFile(brightnessFilenameAlt);

  // Get the current brightness value
  if (!customBrightnessControl) {
    if (brightnessFile->open(QIODevice::ReadOnly)) {
      QByteArray data = brightnessFile->readAll();
      std::string::size_type sz;
      int brightness_val = std::stoi(data.toStdString(), &sz);
      ui->SliderBrightness->setValue(brightness_val);
      QString bri = QString::number(brightness_val);
      ui->ValueBrightness->setText(bri);
      brightnessFile->close();
    }
  } else {
    if (brightnessFileAlt->open(QIODevice::ReadOnly)) {
      QByteArray data = brightnessFileAlt->readAll();
      std::string::size_type sz;
      int brightness_val = std::stoi(data.toStdString(), &sz);
      ui->SliderBrightness->setValue(brightness_val);
      QString bri = QString::number(brightness_val);
      ui->ValueBrightness->setText(bri);
      brightnessFileAlt->close();
    }
  }
  ui->BrightnessWidget->show();
  ui->VolumeWidget->hide();
}

void MainWindow::onClickButtonVolume() {
  ui->VolumeWidget->show();
  ui->BrightnessWidget->hide();
}

void MainWindow::onChangeSliderBrightness(int value) {
  int n = snprintf(brightnessStr, 5, "%d", value);
  brightnessFile = new QFile(brightnessFilename);
  brightnessFileAlt = new QFile(brightnessFilenameAlt);

  if (!customBrightnessControl) {
    if (brightnessFile->open(QIODevice::WriteOnly)) {
      brightnessStr[n] = '\n';
      brightnessStr[n + 1] = '\0';
      brightnessFile->write(brightnessStr);
      brightnessFile->close();
    }
  } else {
    if (brightnessFileAlt->open(QIODevice::WriteOnly)) {
      brightnessStr[n] = '\n';
      brightnessStr[n + 1] = '\0';
      brightnessFileAlt->write(brightnessStr);
      brightnessFileAlt->close();
    }
  }
  QString bri = QString::number(value);
  ui->ValueBrightness->setText(bri);
}

void MainWindow::onChangeSliderVolume(int value) {
  QString vol = QString::number(value);
  ui->ValueVolume->setText(vol + "%");
  system(
      ("/usr/local/bin/autoapp_helper setvolume " + std::to_string(value) + "&")
          .c_str());
}

void MainWindow::updateAlpha() {
  int value = cfg->getAlphaTrans();
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
  ui->ButtonDay->setEnabled(enabled);
  ui->ButtonNight->setEnabled(!enabled);
  updateBg();
}

void MainWindow::dayMode() { setNightMode(false); }
void MainWindow::nightMode() { setNightMode(true); }

void MainWindow::setPowerMenuVisibility(bool visible) {
  ui->ButtonPower->setVisible(!visible);
  ui->PowerMenu->setVisible(visible);
}

void MainWindow::showPowerMenu() { setPowerMenuVisibility(true); }
void MainWindow::hidePowerMenu() { setPowerMenuVisibility(false); }

void MainWindow::updateBg() {
  if (!forceNightMode) {
    if (wallpaperDayFileExists) {
      setStyleSheet(
          "QMainWindow { background: url(wallpaper.png); background-repeat: "
          "no-repeat; background-position: center; }");
    } else {
      setStyleSheet(
          "QMainWindow { background: url(:/black.png); background-repeat: "
          "repeat; background-position: center; }");
    }

  } else {
    if (wallpaperNightFileExists) {
      setStyleSheet(
          "QMainWindow { background: url(wallpaper-night.png) stretch "
          "stretch; background-repeat: no-repeat; background-position: "
          "center; }");
    } else {
      setStyleSheet(
          "QMainWindow { background: url(:/black.png); background-repeat: "
          "repeat; background-position: center; }");
    }
  }
}

void MainWindow::createDebugLog() {
  system("/usr/local/bin/crankshaft debuglog &");
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
  ui->ButtonUnmute->setVisible(muted);
  ui->ButtonMute->setVisible(!muted);
}

void MainWindow::mute() { setMuted(true); }
void MainWindow::unmute() { setMuted(false); }

void MainWindow::showTime() {
  QTime time = QTime::currentTime();
  QDate date = QDate::currentDate();
  QString time_text = time.toString("hh : mm : ss");
  dateText = date.toString("MM/dd");

  if ((time.second() % 2) == 0) {
    time_text[3] = ' ';
    time_text[8] = ' ';
  }

  ui->Clock->setText(time_text);

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
      if (ui->BluetoothDevice->isHidden()) {
        ui->BluetoothDevice->show();
      }
      if (std::ifstream("/tmp/btdevice")) {
        ui->BluetoothDevice->setText(cfg->readFileContent("/tmp/btdevice"));
      }
    } else {
      if (ui->BluetoothDevice->isVisible()) {
        ui->BluetoothDevice->hide();
        ui->BluetoothDevice->setText("BT-Device");
      }
    }
  }
}

void MainWindow::setRetryUsbConnect() {
  ui->SysInfoBottom->setText("Trying USB connect ...");

  QTimer::singleShot(10000, this, SLOT(resetRetryUsbMessage()));
}

void MainWindow::resetRetryUsbMessage() { ui->SysInfoBottom->clear(); }

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

void MainWindow::onChangeTmpDir() {
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
    if (ui->MainWidget->isVisible()) {
      closeAllDialogs();
      ui->MainWidget->hide();
    }
  } else {
    if (ui->MainWidget->isHidden()) {
      ui->MainWidget->show();
    }
  }

  // check if system is in display off mode (tap2wake/screensaver)
  if (std::ifstream("/tmp/screensaver")) {
    if (ui->Tiles->isVisible()) {
      ui->Tiles->hide();
    }

    if (ui->Header->isVisible()) {
      ui->Header->hide();
      closeAllDialogs();
    }

    if (ui->VolumeWidget->isVisible()) {
      ui->VolumeWidget->hide();
    }
    if (ui->BrightnessWidget->isVisible()) {
      ui->BrightnessWidget->hide();
    }
  } else {
    if (ui->Header->isHidden()) {
      ui->Header->show();
    }
    if (ui->VolumeWidget->isHidden()) {
      ui->VolumeWidget->show();
    }
  }

  // check if custom command needs black background
  if (std::ifstream("/tmp/blackscreen")) {
    if (ui->MainWidget->isVisible()) {
      ui->MainWidget->hide();
      setStyleSheet("QMainWindow {background-color: rgb(0,0,0);}");
    }
  } else {
    MainWindow::updateBg();
  }

  // check if phone is conencted to usb
  if (std::ifstream("/tmp/android_device")) {
    if (ui->ButtonAndroidAuto->isHidden()) {
      ui->ButtonAndroidAuto->show();
      ui->ButtonNoDevice->hide();
    }
    try {
      QFile deviceData(QString("/tmp/android_device"));
      deviceData.open(QIODevice::ReadOnly);
      QTextStream data_date(&deviceData);
      QString linedate = data_date.readAll().split("\n")[1];
      deviceData.close();
    } catch (...) {
    }
  } else {
    if (ui->ButtonAndroidAuto->isVisible()) {
      ui->ButtonNoDevice->show();
      ui->ButtonAndroidAuto->hide();
    }
  }

  // check if bluetooth pairable
  if (bluetoothEnabled) {
    if (std::ifstream("/tmp/bluetooth_pairable")) {
      if (ui->Pairable->isHidden()) {
        ui->Pairable->show();
      }
      if (ui->ButtonBluetooth->isVisible()) {
        ui->ButtonBluetooth->hide();
      }
    } else {
      if (ui->Pairable->isVisible()) {
        ui->Pairable->hide();
      }
      if (ui->ButtonBluetooth->isHidden()) {
        ui->ButtonBluetooth->show();
      }
    }
  } else {
    if (ui->Pairable->isVisible()) {
      ui->Pairable->hide();
    }
    if (ui->ButtonBluetooth->isVisible()) {
      ui->ButtonBluetooth->hide();
    }
  }

  if (std::ifstream("/tmp/config_in_progress") ||
      std::ifstream("/tmp/debug_in_progress") ||
      std::ifstream("/tmp/enable_pairing")) {
    if (std::ifstream("/tmp/config_in_progress")) {
      ui->ButtonSettings->hide();
      ui->ButtonLock->show();
      ui->SysInfoBottom->setText("Config in progress ...");
    }
    if (std::ifstream("/tmp/debug_in_progress")) {
      ui->ButtonSettings->hide();
      ui->ButtonLock->show();
      ui->SysInfoBottom->setText("Creating debug.zip ...");
    }
    if (std::ifstream("/tmp/enable_pairing")) {
      ui->SysInfoTop->setText("Pairing enabled for 120 seconds!");
    }
  } else {
    ui->SysInfoTop->clear();
    ui->ButtonSettings->show();
    ui->ButtonLock->hide();
  }

  // update day/night state
  forceNightMode = doesFileExist("/tmp/night_mode_enabled");

  if (forceNightMode) {
    nightMode();
  } else {
    dayMode();
  }

  // check if shutdown is external triggered and init clean app exit
  if (std::ifstream("/tmp/external_exit")) {
    MainWindow::exit();
  }

  hotspotActive = doesFileExist("/tmp/hotspot_active");

  // hide wifi if hotspot disabled and force wifi unselected
  if (!hotspotActive && !std::ifstream("/tmp/mobile_hotspot_detected")) {
    if ((ui->WifiWidget->isVisible())) {
      ui->WifiWidget->hide();
      ui->UsbWidget->show();
    }
  } else {
    if ((ui->WifiWidget->isHidden())) {
      ui->WifiWidget->show();
      ui->UsbWidget->hide();
    }
  }

  if (std::ifstream("/tmp/temp_recent_list") ||
      std::ifstream("/tmp/mobile_hotspot_detected")) {
    if (ui->ButtonWifi->isHidden()) {
      ui->ButtonWifi->show();
    }
    if (ui->ButtonNoWifiDevice->isVisible()) {
      ui->ButtonNoWifiDevice->hide();
    }
  } else {
    if (ui->ButtonWifi->isVisible()) {
      ui->ButtonWifi->hide();
    }
    if (ui->ButtonNoWifiDevice->isHidden()) {
      ui->ButtonNoWifiDevice->show();
    }
  }

  // clock viibility by settings
  if (!cfg->showClock()) {
    ui->Clock->hide();
  } else {
    ui->Clock->show();
  }

  if (!cfg->showNetworkInfo()) {
    if (ui->NetworkInfo->isVisible()) {
      ui->NetworkInfo->hide();
    }
  } else {
    if (ui->NetworkInfo->isHidden()) {
      ui->NetworkInfo->show();
    }
  }

  // hide brightness button if eanbled in settings
  if (cfg->hideBrightnessControl()) {
    if ((ui->ButtonBrightness->isVisible()) ||
        (ui->ButtonBrightness->isVisible()) ||
        (ui->BrightnessWidget->isVisible())) {
      ui->ButtonBrightness->hide();
      ui->BrightnessWidget->hide();
      // also hide volume button if brightness hidden
      ui->ButtonVolume->hide();
      ui->VolumeWidget->show();
    }
  } else {
  }

  MainWindow::updateAlpha();

  if (std::ifstream("/tmp/btdevice") ||
      std::ifstream("/tmp/dev_mode_enabled") ||
      std::ifstream("/tmp/android_device")) {
    if (ui->Locked->isHidden()) {
      ui->Locked->show();
    }
  } else {
    if (ui->Locked->isVisible()) {
      ui->Locked->hide();
    }
  }
  updateNetworkInfo();
}

}  // namespace ui
}  // namespace autoapp
}  // namespace openauto
}  // namespace f1x
