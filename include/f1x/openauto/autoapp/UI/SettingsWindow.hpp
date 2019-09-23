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

#include <sys/sysinfo.h>

#include <QFileDialog>
#include <QWidget>
#include <f1x/openauto/autoapp/Configuration/IConfiguration.hpp>
#include <memory>

class QCheckBox;
class QTimer;

namespace Ui {
class SettingsWindow;
}

namespace f1x {
namespace openauto {
namespace autoapp {
namespace ui {

class SettingsWindow : public QWidget {
  Q_OBJECT
 public:
  explicit SettingsWindow(configuration::IConfiguration::Pointer configuration,
                          QWidget* parent = nullptr);
  ~SettingsWindow() override;
  void loadSystemValues();

 private slots:
  void unpairAll();
  void onSave();
  void onResetToDefaults();
  void onUpdateScreenDPI(int value);
  void onUpdateAlphaTrans(int value);
  void onUpdateBrightnessDay(int value);
  void onUpdateBrightnessNight(int value);
  void onUpdateSystemVolume(int value);
  void onUpdateSystemCapture(int value);
  void setTime();
  void onStartHotspot();
  void onStopHotspot();
  void syncNTPTime();
  void on_ButtonAudioTest_clicked();
  void updateNetworkInfo();
  void on_ButtonNetwork0_clicked();
  void on_ButtonNetwork1_clicked();
  void updateSystemInfo();
  void updateInfo();

 private:
  void showEvent(QShowEvent* event);
  void load();
  void loadButtonCheckBoxes();
  void saveButtonCheckBoxes();
  void saveButtonCheckBox(
      const QCheckBox* checkBox,
      configuration::IConfiguration::ButtonCodes& buttonCodes,
      aasdk::proto::enums::ButtonCode::Enum buttonCode);
  void setButtonCheckBoxes(bool value);

  Ui::SettingsWindow* ui_;
  configuration::IConfiguration::Pointer configuration_;
};

}  // namespace ui
}  // namespace autoapp
}  // namespace openauto
}  // namespace f1x
