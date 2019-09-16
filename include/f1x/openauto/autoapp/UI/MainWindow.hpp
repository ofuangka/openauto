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

#pragma once

#include <QAudioProbe>
#include <QBluetoothLocalDevice>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QKeyEvent>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QVideoProbe>
#include <f1x/openauto/autoapp/Configuration/IConfiguration.hpp>
#include <memory>
//#include <QtBluetooth>

namespace Ui {
class MainWindow;
}

namespace f1x {
namespace openauto {
namespace autoapp {
namespace ui {

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(configuration::IConfiguration::Pointer configuration,
                      QWidget *parent = nullptr);
  ~MainWindow() override;
  QFileSystemWatcher *tmpDirWatcher;

 signals:
  void exit();
  void reboot();
  void openSettings();
  void openConnectDialog();
  void openWifiDialog();
  void closeAllDialogs();
  void triggerScriptDay();
  void triggerScriptNight();
  void triggerAppStart();
  void triggerAppStop();
  void showAlphaSlider();
  void showBrightnessSlider();
  void showVolumeSlider();

 private slots:
  void onClickButtonBrightness();
  void onClickButtonVolume();
  void onChangeSliderBrightness(int value);
  void onChangeSliderVolume(int value);
  void onChangeTmpDir();
  void updateAlpha();
  void showPowerMenu();
  void hidePowerMenu();
  void enablePairing();
  void updateBg();
  void nightMode();
  void dayMode();
  void mute();
  void unmute();

  void onChangeHostMode(QBluetoothLocalDevice::HostMode);

 private:
  void showTime();
  void setAlpha(QString newAlpha, QWidget *widget);
  bool doesFileExist(const char *filename);
  void setRetryUsbConnect();
  void resetRetryUsbMessage();
  void updateNetworkInfo();
  void setNightMode(bool enabled);
  void setMuted(bool muted);
  void setPowerMenuVisibility(bool visible);

  Ui::MainWindow *ui;
  configuration::IConfiguration::Pointer cfg;

  QString brightnessFilename = "/sys/class/backlight/rpi_backlight/brightness";
  QString brightnessFilenameAlt = "/tmp/custombrightness";
  QFile *brightnessFile;
  QFile *brightnessFileAlt;
  char brightnessStr[6];
  char volumeStr[6];
  int alphaCurrentStr;
  QString bversion;
  QString bdate;

  char nightModeFile[32] = "/tmp/night_mode_enabled";
  char wifiButtonFile[32] = "/etc/button_wifi_visible";
  char brightnessButtonFile[32] = "/etc/button_brightness_visible";

  QString dateText;
  QRegExp backgroundColor = QRegExp(
      "(?<=background-color)(\\s*:\\s*rgba\\s*\\((?:[0-9]{1,3}\\s*,\\s*){3})[0-"
      "1]?(?:\\.[0-9]+)?(?=\\s*\\))");

  bool customBrightnessControl = false;

  bool wifiButtonForce = false;
  bool brightnessButtonForce = false;

  bool forceNightMode = false;

  bool wallpaperDayFileExists = false;
  bool wallpaperNightFileExists = false;

  bool bluetoothEnabled = false;

  bool hotspotActive = false;

  QBluetoothLocalDevice *localDevice;
};

}  // namespace ui
}  // namespace autoapp
}  // namespace openauto
}  // namespace f1x
