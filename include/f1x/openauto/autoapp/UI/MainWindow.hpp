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

 signals:
  void shutdown();
  void reboot();
  void openSettings();
  void openConnectDialog();
  void openWifiDialog();
  void closeAllDialogs();
  void dayModeEvent();
  void nightModeEvent();
  void appStartEvent();
  void appStopEvent();

 private slots:
  void clockTick();
  void rotateSliders();
  void onChangeBrightness(int value);
  void onChangeVolume(int value);
  void onTrigger();
  void updateTransparency();
  void powerMenu();
  void closePowerMenu();
  void enablePairing();
  void updateBg();
  void nightMode();
  void dayMode();
  void toggleMute();

  void onChangeHostMode(QBluetoothLocalDevice::HostMode);

 private:
  void setAlpha(QString newAlpha, QWidget *widget);
  bool doesFileExist(const char *filename);
  void setRetryUsbConnect();
  void resetRetryUsbMessage();
  void setNightMode(bool enabled);
  void setMuted(bool muted);
  void setPowerMenuVisibility(bool visible);
  void lockSettings(bool lock);
  void initSliders();
  void refreshBluetooth();
  void readHostCapabilities();
  void readConfig();
  void initStatuses();

  Ui::MainWindow *ui;
  configuration::IConfiguration::Pointer cfg;

  std::string PATH_BRIGHTNESS = "/sys/class/backlight/rpi_backlight/brightness";
  std::string PATH_FORCE_NIGHT_MODE = "/tmp/force-night-mode";
  std::string PATH_FORCE_WIFI = "/tmp/force-wifi";
  std::string PATH_FORCE_BRIGHTNESS = "/tmp/force-brightness";
  std::string PATH_BT_DEVICE = "/tmp/bt-device";
  std::string PATH_BT_PAIRABLE = "/tmp/bt-pairable";
  std::string PATH_WALLPAPER = "wallpaper.png";
  std::string PATH_WALLPAPER_NIGHT = "wallpaper-night.png";
  std::string PATH_HOTSPOT_DETECTED = "/tmp/hotspot-detected";
  std::string PATH_CUSTOM_BRIGHTNESS = "/tmp/custom-brightness";
  std::string PATH_WALLPAPER_BLACK = ":/black.png";
  std::string PATH_CONFIG_IN_PROGRESS = "/tmp/config-in-progress";
  std::string PATH_DEBUG_IN_PROGRESS = "/tmp/debug-in-progress";
  std::string PATH_EXTERNAL_EXIT = "/tmp/external-exit";
  std::string PATH_HOTSPOT_ACTIVE = "/tmp/hotspot_active";
  std::string PATH_RECENT_SSIDS = "/tmp/recent-ssids";
  std::string PATH_BLACK_SCREEN = "/tmp/black-screen";
  std::string PATH_BLANK_SCREEN = "/tmp/blank-screen";
  std::string PATH_SCREENSAVER = "/tmp/screensaver";
  std::string PATH_ANDROID_DEVICE = "/tmp/android_device";
  std::string PATH_APP_STOP = "/tmp/app-stop";

  int currentAlpha;

  QRegExp REGEX_BACKGROUND_COLOR = QRegExp("rgba\\(((?:[0-9]*,\\s*){3})");
  QRegExp REGEX_BACKGROUND_IMAGE = QRegExp("url\\('[^']*'\\)");

  bool forceEnableWifi = false;
  bool forceEnableBrightness = false;
  bool forceNightMode = false;

  bool wallpaperDayExists = false;
  bool wallpaperNightExists = false;

  bool hotspotActive = false;

  QBluetoothLocalDevice *localDevice;

  QList<QWidget *> *sliders;
  QFileSystemWatcher *triggerWatch;
};

}  // namespace ui
}  // namespace autoapp
}  // namespace openauto
}  // namespace f1x
