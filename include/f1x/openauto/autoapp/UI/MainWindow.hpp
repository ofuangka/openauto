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
  QFileSystemWatcher *watcher_tmp;

 signals:
  void exit();
  void reboot();
  void openSettings();
  void toggleCursor();
  void TriggerScriptDay();
  void TriggerScriptNight();
  void openConnectDialog();
  void openWifiDialog();
  void showSliderBrightness();
  void showVolumeSlider();
  void showAlphaSlider();
  void TriggerAppStart();
  void TriggerAppStop();
  void CloseAllDialogs();

 private slots:
  void onChangeSliderBrightness(int value);
  void onChangeSliderVolume(int value);
  void updateAlpha();

 private slots:
  void onClickButtonBrightness();
  void onClickButtonVolume();
  void switchGuiToDay();
  void switchGuiToNight();
  void showTime();
  void toggleExit();
  void createDebuglog();
  void setPairable();
  void toggleMuteButton();
  void setMute();
  void setUnMute();
  void updateBG();

  void tmpChanged();
  void setRetryUSBConnect();
  void resetRetryUSBMessage();
  void updateNetworkInfo();
  bool check_file_exist(const char *filename);

  void hostModeStateChanged(QBluetoothLocalDevice::HostMode);

 private:
  Ui::MainWindow *ui_;
  configuration::IConfiguration::Pointer configuration_;

  QString brightnessFilename = "/sys/class/backlight/rpi_backlight/brightness";
  QString brightnessFilenameAlt = "/tmp/custombrightness";
  QFile *brightnessFile;
  QFile *brightnessFileAlt;
  char brightness_str[6];
  char volume_str[6];
  int alpha_current_str;
  QString bversion;
  QString bdate;

  char nightModeFile[32] = "/tmp/night_mode_enabled";
  char wifiButtonFile[32] = "/etc/button_wifi_visible";
  char brightnessButtonFile[32] = "/etc/button_brightness_visible";
  char debugModeFile[32] = "/tmp/usb_debug_mode";

  QString date_text;

  bool customBrightnessControl = false;

  bool wifiButtonForce = false;
  bool brightnessButtonForce = false;

  bool nightModeEnabled = false;
  bool DayNightModeState = false;

  bool wallpaperDayFileExists = false;
  bool wallpaperNightFileExists = false;

  bool exitMenuVisible = false;

  bool systemDebugmode = false;

  bool bluetoothEnabled = false;

  bool toggleMute = false;
  bool NoClock = false;

  bool hotspotActive = false;
  bool background_set = false;

  QBluetoothLocalDevice *localDevice;
};

}  // namespace ui
}  // namespace autoapp
}  // namespace openauto
}  // namespace f1x
