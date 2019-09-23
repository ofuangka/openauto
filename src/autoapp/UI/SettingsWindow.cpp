/*subfolder
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

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QProcess>
#include <QStorageInfo>
#include <QTextStream>
#include <QTimer>
#include <f1x/openauto/autoapp/UI/SettingsWindow.hpp>
#include <fstream>
#include <string>

#include "ui_settingswindow.h"

namespace f1x {
namespace openauto {
namespace autoapp {
namespace ui {

SettingsWindow::SettingsWindow(
    configuration::IConfiguration::Pointer configuration, QWidget* parent)
    : QWidget(parent),
      ui_(new Ui::SettingsWindow),
      configuration_(std::move(configuration)) {
  ui_->setupUi(this);
  connect(ui_->ButtonCancel, &QPushButton::clicked, this,
          &SettingsWindow::close);
  connect(ui_->ButtonSave, &QPushButton::clicked, this,
          &SettingsWindow::onSave);
  connect(ui_->ButtonUnpair, &QPushButton::clicked, this,
          &SettingsWindow::unpairAll);
  connect(ui_->ButtonUnpair, &QPushButton::clicked, this,
          &SettingsWindow::close);
  connect(ui_->horizontalSliderScreenDPI, &QSlider::valueChanged, this,
          &SettingsWindow::onUpdateScreenDPI);
  connect(ui_->horizontalSliderAlphaTrans, &QSlider::valueChanged, this,
          &SettingsWindow::onUpdateAlphaTrans);
  connect(ui_->horizontalSliderDay, &QSlider::valueChanged, this,
          &SettingsWindow::onUpdateBrightnessDay);
  connect(ui_->horizontalSliderNight, &QSlider::valueChanged, this,
          &SettingsWindow::onUpdateBrightnessNight);
  connect(ui_->RadioUseExternalBluetoothAdapter, &QRadioButton::clicked,
          [&](bool checked) {
            ui_->lineEditExternalBluetoothAdapterAddress->setEnabled(checked);
          });
  connect(ui_->RadioDisableBluetooth, &QRadioButton::clicked, [&]() {
    ui_->lineEditExternalBluetoothAdapterAddress->setEnabled(false);
  });
  connect(ui_->RadioUseLocalBluetoothAdapter, &QRadioButton::clicked, [&]() {
    ui_->lineEditExternalBluetoothAdapterAddress->setEnabled(false);
  });
  connect(ui_->ButtonResetToDefaults, &QPushButton::clicked, this,
          &SettingsWindow::onResetToDefaults);
  connect(ui_->horizontalSliderSystemVolume, &QSlider::valueChanged, this,
          &SettingsWindow::onUpdateSystemVolume);
  connect(ui_->horizontalSliderSystemCapture, &QSlider::valueChanged, this,
          &SettingsWindow::onUpdateSystemCapture);
  connect(ui_->RadioHotspot, &QPushButton::clicked, this,
          &SettingsWindow::onStartHotspot);
  connect(ui_->RadioClient, &QPushButton::clicked, this,
          &SettingsWindow::onStopHotspot);
  connect(ui_->ButtonSetTime, &QPushButton::clicked, this,
          &SettingsWindow::setTime);
  connect(ui_->ButtonSetTime, &QPushButton::clicked, this,
          &SettingsWindow::close);
  connect(ui_->ButtonNTP, &QPushButton::clicked,
          [&]() { system("/usr/local/bin/crankshaft rtc sync &"); });
  connect(ui_->ButtonNTP, &QPushButton::clicked, this, &SettingsWindow::close);
  connect(ui_->ButtonNetworkAuto, &QPushButton::clicked,
          [&]() { system("/usr/local/bin/crankshaft network auto &"); });
  connect(ui_->ButtonNetwork0, &QPushButton::clicked, this,
          &SettingsWindow::on_ButtonNetwork0_clicked);
  connect(ui_->ButtonNetwork1, &QPushButton::clicked, this,
          &SettingsWindow::on_ButtonNetwork1_clicked);

  ui_->horizontalGroupBox->hide();
  ui_->labelBluetoothAdapterAddress->hide();
  ui_->lineEditExternalBluetoothAdapterAddress->hide();
  ui_->labelTestInProgress->hide();

  QTime time = QTime::currentTime();
  QString time_text_hour = time.toString("hh");
  QString time_text_minute = time.toString("mm");
  ui_->spinBoxHour->setValue((time_text_hour).toInt());
  ui_->spinBoxMinute->setValue((time_text_minute).toInt());
  ui_->label_modeswitchprogress->setText("Ok");
  ui_->label_notavailable->hide();

  QString wifi_ssid = configuration_->getCSValue("WIFI_SSID");
  QString wifi2_ssid = configuration_->getCSValue("WIFI2_SSID");

  ui_->ButtonNetwork0->setText(wifi_ssid);
  ui_->ButtonNetwork1->setText(wifi2_ssid);

  if (!std::ifstream("/boot/crankshaft/network1.conf")) {
    ui_->ButtonNetwork1->hide();
    ui_->ButtonNetwork0->show();
  }
  if (!std::ifstream("/boot/crankshaft/network0.conf")) {
    ui_->ButtonNetwork1->hide();
    ui_->ButtonNetwork0->setText(configuration_->getCSValue("WIFI2_SSID"));
  }
  if (!std::ifstream("/boot/crankshaft/network0.conf") &&
      !std::ifstream("/boot/crankshaft/network1.conf")) {
    ui_->ButtonNetwork0->hide();
    ui_->ButtonNetwork1->hide();
    ui_->ButtonNetworkAuto->hide();
    ui_->label_notavailable->show();
  }

  if (std::ifstream("/tmp/hotspot_active")) {
    ui_->RadioClient->setChecked(0);
    ui_->RadioHotspot->setChecked(1);
    ui_->lineEditWifiSSID->setText(
        configuration_->getParamFromFile("/etc/hostapd/hostapd.conf", "ssid"));
    ui_->lineEditPassword->show();
    ui_->label_password->show();
    ui_->lineEditPassword->setText("1234567890");
    ui_->clientNetworkSelect->hide();
    ui_->label_notavailable->show();
  } else {
    ui_->RadioClient->setChecked(1);
    ui_->RadioHotspot->setChecked(0);
    ui_->lineEditWifiSSID->setText(
        configuration_->readFileContent("/tmp/wifi_ssid"));
    ui_->lineEditPassword->hide();
    ui_->label_password->hide();
    ui_->lineEditPassword->setText("");
    ui_->clientNetworkSelect->hide();
    ui_->label_notavailable->show();
  }

  QTimer* refresh = new QTimer(this);
  connect(refresh, SIGNAL(timeout()), this, SLOT(updateInfo()));
  refresh->start(5000);
}

SettingsWindow::~SettingsWindow() { delete ui_; }

void SettingsWindow::updateInfo() {
  updateSystemInfo();
  updateNetworkInfo();
}

void SettingsWindow::onSave() {
  configuration_->setHandednessOfTrafficType(
      ui_->RadioLeftHandDrive->isChecked()
          ? configuration::HandednessOfTrafficType::LEFT_HAND_DRIVE
          : configuration::HandednessOfTrafficType::RIGHT_HAND_DRIVE);

  configuration_->showClock(ui_->CheckBoxShowClock->isChecked());
  configuration_->setAlphaTrans(
      static_cast<size_t>(ui_->horizontalSliderAlphaTrans->value()));
  configuration_->showCursor(ui_->CheckBoxShowCursor->isChecked());
  configuration_->hideBrightnessControl(
      ui_->CheckBoxHideBrightnessControl->isChecked());
  configuration_->showNetworkInfo(ui_->CheckBoxNetworkinfo->isChecked());

  configuration_->setVideoFPS(ui_->radioButton30FPS->isChecked()
                                  ? aasdk::proto::enums::VideoFPS::_30
                                  : aasdk::proto::enums::VideoFPS::_60);

  if (ui_->radioButton480p->isChecked()) {
    configuration_->setVideoResolution(
        aasdk::proto::enums::VideoResolution::_480p);
  } else if (ui_->radioButton720p->isChecked()) {
    configuration_->setVideoResolution(
        aasdk::proto::enums::VideoResolution::_720p);
  } else if (ui_->radioButton1080p->isChecked()) {
    configuration_->setVideoResolution(
        aasdk::proto::enums::VideoResolution::_1080p);
  }

  configuration_->setScreenDPI(
      static_cast<size_t>(ui_->horizontalSliderScreenDPI->value()));
  configuration_->setOMXLayerIndex(ui_->spinBoxOmxLayerIndex->value());

  QRect videoMargins(0, 0, ui_->spinBoxVideoMarginWidth->value(),
                     ui_->spinBoxVideoMarginHeight->value());
  configuration_->setVideoMargins(std::move(videoMargins));

  configuration_->setTouchscreenEnabled(
      ui_->CheckBoxEnableTouchscreen->isChecked());
  this->saveButtonCheckBoxes();

  if (ui_->RadioDisableBluetooth->isChecked()) {
    configuration_->setBluetoothAdapterType(
        configuration::BluetoothAdapterType::NONE);
  } else if (ui_->RadioUseLocalBluetoothAdapter->isChecked()) {
    configuration_->setBluetoothAdapterType(
        configuration::BluetoothAdapterType::LOCAL);
  } else if (ui_->RadioUseExternalBluetoothAdapter->isChecked()) {
    configuration_->setBluetoothAdapterType(
        configuration::BluetoothAdapterType::REMOTE);
  }

  configuration_->setBluetoothRemoteAdapterAddress(
      ui_->lineEditExternalBluetoothAdapterAddress->text().toStdString());

  configuration_->setMusicAudioChannelEnabled(
      ui_->CheckBoxMusicAudioChannel->isChecked());
  configuration_->setSpeechAudioChannelEnabled(
      ui_->CheckBoxSpeechAudioChannel->isChecked());
  configuration_->setAudioOutputBackendType(
      ui_->RadioRtAudio->isChecked()
          ? configuration::AudioOutputBackendType::RTAUDIO
          : configuration::AudioOutputBackendType::QT);

  configuration_->save();

  // generate param string for autoapp_helper
  std::string params;
  params.append(std::to_string(ui_->horizontalSliderSystemVolume->value()));
  params.append("#");
  params.append(std::to_string(ui_->horizontalSliderSystemCapture->value()));
  params.append("#");
  params.append(std::to_string(ui_->spinBoxDisconnect->value()));
  params.append("#");
  params.append(std::to_string(ui_->spinBoxShutdown->value()));
  params.append("#");
  params.append(std::to_string(ui_->spinBoxDay->value()));
  params.append("#");
  params.append(std::to_string(ui_->spinBoxNight->value()));
  params.append("#");
  params.append(std::string(ui_->comboBoxInvert->currentText().toStdString()));
  params.append("#");
  params.append(std::string(ui_->comboBoxX11->currentText().toStdString()));
  params.append("#");
  params.append(std::string(ui_->comboBoxRearcam->currentText().toStdString()));
  params.append("#");
  params.append(std::string(ui_->comboBoxAndroid->currentText().toStdString()));
  params.append("#");
  if (ui_->RadioX11->isChecked()) {
    params.append("1");
  } else {
    params.append("0");
  }
  params.append("#");
  if (ui_->RadioScreenRotated->isChecked()) {
    params.append("1");
  } else {
    params.append("0");
  }
  params.append("#");
  params.append(
      std::string("'") +
      std::string(ui_->comboBoxPulseOutput->currentText().toStdString()) +
      std::string("'"));
  params.append("#");
  params.append(
      std::string("'") +
      std::string(ui_->comboBoxPulseInput->currentText().toStdString()) +
      std::string("'"));
  params.append("#");
  if (ui_->CheckBoxDisableShutdown->isChecked()) {
    params.append("1");
  } else {
    params.append("0");
  }
  params.append("#");
  if (ui_->CheckBoxDisableScreenOff->isChecked()) {
    params.append("1");
  } else {
    params.append("0");
  }
  params.append("#");
  params.append(
      std::string(ui_->comboBoxGPIOShutdown->currentText().toStdString()));
  params.append("#");
  params.append(std::to_string(ui_->spinBoxGPIOShutdownDelay->value()));
  params.append("#");
  if (ui_->CheckBoxHotspot->isChecked()) {
    params.append("1");
  } else {
    params.append("0");
  }
  params.append("#");
  if (ui_->CheckBoxBluetoothAutoPair->isChecked()) {
    params.append("1");
  } else {
    params.append("0");
  }
  params.append("#");
  params.append(
      std::string(ui_->comboBoxBluetooth->currentText().toStdString()));
  params.append("#");
  params.append(
      std::string(ui_->comboBoxDayNight->currentText().toStdString()));
  params.append("#");
  params.append(std::to_string(ui_->horizontalSliderDay->value()));
  params.append("#");
  params.append(std::to_string(ui_->horizontalSliderNight->value()));
  params.append("#");
  if (ui_->CheckBoxDisableDayNightRTC->isChecked()) {
    params.append("0");
  } else {
    params.append("1");
  }
  params.append("#");
  params.append(std::string(ui_->comboBoxCountryCode->currentText()
                                .split("|")[0]
                                .replace(" ", "")
                                .toStdString()));
  params.append("#");
  if (ui_->CheckBoxBlankOnly->isChecked()) {
    params.append("1");
  } else {
    params.append("0");
  }
  params.append("#");
  system((std::string("/usr/local/bin/autoapp_helper setparams#") +
          std::string(params) + std::string(" &"))
             .c_str());

  this->close();
}

void SettingsWindow::onResetToDefaults() {
  QMessageBox confirmationMessage(QMessageBox::Question, "Confirmation",
                                  "Are you sure you want to reset settings?",
                                  QMessageBox::Yes | QMessageBox::Cancel);
  confirmationMessage.setWindowFlags(Qt::WindowStaysOnTopHint);
  if (confirmationMessage.exec() == QMessageBox::Yes) {
    configuration_->reset();
    this->load();
  }
}

void SettingsWindow::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  this->load();
}

void SettingsWindow::load() {
  ui_->RadioLeftHandDrive->setChecked(
      configuration_->getHandednessOfTrafficType() ==
      configuration::HandednessOfTrafficType::LEFT_HAND_DRIVE);
  ui_->RadioRightHandDrive->setChecked(
      configuration_->getHandednessOfTrafficType() ==
      configuration::HandednessOfTrafficType::RIGHT_HAND_DRIVE);
  ui_->CheckBoxShowClock->setChecked(configuration_->showClock());
  ui_->horizontalSliderAlphaTrans->setValue(
      static_cast<int>(configuration_->getAlphaTrans()));
  ui_->CheckBoxShowCursor->setChecked(configuration_->showCursor());
  ui_->CheckBoxHideBrightnessControl->setChecked(
      configuration_->hideBrightnessControl());
  ui_->CheckBoxNetworkinfo->setChecked(configuration_->showNetworkInfo());

  ui_->radioButton30FPS->setChecked(configuration_->getVideoFPS() ==
                                    aasdk::proto::enums::VideoFPS::_30);
  ui_->radioButton60FPS->setChecked(configuration_->getVideoFPS() ==
                                    aasdk::proto::enums::VideoFPS::_60);

  ui_->radioButton480p->setChecked(configuration_->getVideoResolution() ==
                                   aasdk::proto::enums::VideoResolution::_480p);
  ui_->radioButton720p->setChecked(configuration_->getVideoResolution() ==
                                   aasdk::proto::enums::VideoResolution::_720p);
  ui_->radioButton1080p->setChecked(
      configuration_->getVideoResolution() ==
      aasdk::proto::enums::VideoResolution::_1080p);
  ui_->horizontalSliderScreenDPI->setValue(
      static_cast<int>(configuration_->getScreenDPI()));
  ui_->spinBoxOmxLayerIndex->setValue(configuration_->getOMXLayerIndex());

  const auto& videoMargins = configuration_->getVideoMargins();
  ui_->spinBoxVideoMarginWidth->setValue(videoMargins.width());
  ui_->spinBoxVideoMarginHeight->setValue(videoMargins.height());

  ui_->CheckBoxEnableTouchscreen->setChecked(
      configuration_->getTouchscreenEnabled());
  this->loadButtonCheckBoxes();
  ui_->RadioDisableBluetooth->setChecked(
      configuration_->getBluetoothAdapterType() ==
      configuration::BluetoothAdapterType::NONE);
  ui_->RadioUseLocalBluetoothAdapter->setChecked(
      configuration_->getBluetoothAdapterType() ==
      configuration::BluetoothAdapterType::LOCAL);
  ui_->RadioUseExternalBluetoothAdapter->setChecked(
      configuration_->getBluetoothAdapterType() ==
      configuration::BluetoothAdapterType::REMOTE);
  ui_->lineEditExternalBluetoothAdapterAddress->setEnabled(
      configuration_->getBluetoothAdapterType() ==
      configuration::BluetoothAdapterType::REMOTE);
  ui_->lineEditExternalBluetoothAdapterAddress->setText(QString::fromStdString(
      configuration_->getBluetoothRemoteAdapterAddress()));

  ui_->CheckBoxMusicAudioChannel->setChecked(
      configuration_->musicAudioChannelEnabled());
  ui_->CheckBoxSpeechAudioChannel->setChecked(
      configuration_->speechAudioChannelEnabled());

  const auto& audioOutputBackendType =
      configuration_->getAudioOutputBackendType();
  ui_->RadioRtAudio->setChecked(audioOutputBackendType ==
                                configuration::AudioOutputBackendType::RTAUDIO);
  ui_->RadioQtAudio->setChecked(audioOutputBackendType ==
                                configuration::AudioOutputBackendType::QT);
  QStorageInfo storage("/media/USBDRIVES/CSSTORAGE");
  storage.refresh();
}

void SettingsWindow::loadButtonCheckBoxes() {
  const auto& buttonCodes = configuration_->getButtonCodes();
}

void SettingsWindow::setButtonCheckBoxes(bool value) {}

void SettingsWindow::saveButtonCheckBoxes() {
  configuration::IConfiguration::ButtonCodes buttonCodes;
  configuration_->setButtonCodes(buttonCodes);
}

void SettingsWindow::saveButtonCheckBox(
    const QCheckBox* checkBox,
    configuration::IConfiguration::ButtonCodes& buttonCodes,
    aasdk::proto::enums::ButtonCode::Enum buttonCode) {
  if (checkBox->isChecked()) {
    buttonCodes.push_back(buttonCode);
  }
}

void SettingsWindow::onUpdateScreenDPI(int value) {
  ui_->labelScreenDPIValue->setText(QString::number(value));
}

void SettingsWindow::onUpdateAlphaTrans(int value) {
  double alpha = value / 100.0;
  ui_->labelAlphaTransValue->setText(QString::number(alpha));
}

void SettingsWindow::onUpdateBrightnessDay(int value) {
  ui_->labelBrightnessDay->setText(QString::number(value));
}

void SettingsWindow::onUpdateBrightnessNight(int value) {
  ui_->labelBrightnessNight->setText(QString::number(value));
}

void SettingsWindow::onUpdateSystemVolume(int value) {
  ui_->labelSystemVolumeValue->setText(QString::number(value));
}

void SettingsWindow::onUpdateSystemCapture(int value) {
  ui_->labelSystemCaptureValue->setText(QString::number(value));
}

void SettingsWindow::unpairAll() {
  system("/usr/local/bin/crankshaft bluetooth unpair &");
}

void SettingsWindow::setTime() {
  // generate param string for autoapp_helper
  std::string params;
  params.append(std::to_string(ui_->spinBoxHour->value()));
  params.append("#");
  params.append(std::to_string(ui_->spinBoxMinute->value()));
  params.append("#");
  system((std::string("/usr/local/bin/autoapp_helper settime#") +
          std::string(params) + std::string(" &"))
             .c_str());
}

void SettingsWindow::syncNTPTime() {
  system("/usr/local/bin/crankshaft rtc sync &");
}

void SettingsWindow::loadSystemValues() {
  // set brightness slider attribs
  ui_->horizontalSliderDay->setMinimum(
      configuration_->getCSValue("BR_MIN").toInt());
  ui_->horizontalSliderDay->setMaximum(
      configuration_->getCSValue("BR_MAX").toInt());
  ui_->horizontalSliderDay->setSingleStep(
      configuration_->getCSValue("BR_STEP").toInt());
  ui_->horizontalSliderDay->setTickInterval(
      configuration_->getCSValue("BR_STEP").toInt());
  ui_->horizontalSliderDay->setValue(
      configuration_->getCSValue("BR_DAY").toInt());

  ui_->horizontalSliderNight->setMinimum(
      configuration_->getCSValue("BR_MIN").toInt());
  ui_->horizontalSliderNight->setMaximum(
      configuration_->getCSValue("BR_MAX").toInt());
  ui_->horizontalSliderNight->setSingleStep(
      configuration_->getCSValue("BR_STEP").toInt());
  ui_->horizontalSliderNight->setTickInterval(
      configuration_->getCSValue("BR_STEP").toInt());
  ui_->horizontalSliderNight->setValue(
      configuration_->getCSValue("BR_NIGHT").toInt());

  if (std::ifstream("/tmp/return_value")) {
    QString return_values =
        configuration_->readFileContent("/tmp/return_value");
    QStringList getparams = return_values.split("#");

    // version string
    ui_->valueSystemVersion->setText(
        configuration_->readFileContent("/etc/crankshaft.build"));
    // date string
    ui_->valueSystemBuildDate->setText(
        configuration_->readFileContent("/etc/crankshaft.date"));
    // set volume
    ui_->labelSystemVolumeValue->setText(
        configuration_->readFileContent("/boot/crankshaft/volume"));
    ui_->horizontalSliderSystemVolume->setValue(
        configuration_->readFileContent("/boot/crankshaft/volume").toInt());
    // set cap volume
    ui_->labelSystemCaptureValue->setText(
        configuration_->readFileContent("/boot/crankshaft/capvolume"));
    ui_->horizontalSliderSystemCapture->setValue(
        configuration_->readFileContent("/boot/crankshaft/capvolume").toInt());
    // set shutdown
    ui_->valueShutdownTimer->setText("- - -");
    ui_->spinBoxShutdown->setValue(
        configuration_->getCSValue("DISCONNECTION_POWEROFF_MINS").toInt());
    // set disconnect
    ui_->valueDisconnectTimer->setText("- - -");
    ui_->spinBoxDisconnect->setValue(
        configuration_->getCSValue("DISCONNECTION_SCREEN_POWEROFF_SECS")
            .toInt());
    // set day/night
    ui_->spinBoxDay->setValue(
        configuration_->getCSValue("RTC_DAY_START").toInt());
    ui_->spinBoxNight->setValue(
        configuration_->getCSValue("RTC_NIGHT_START").toInt());
    ui_->comboBoxInvert->setCurrentText(
        configuration_->getCSValue("INVERT_PIN"));
    ui_->comboBoxX11->setCurrentText(configuration_->getCSValue("X11_PIN"));
    ui_->comboBoxRearcam->setCurrentText(
        configuration_->getCSValue("REARCAM_PIN"));
    ui_->comboBoxAndroid->setCurrentText(
        configuration_->getCSValue("ANDROID_PIN"));
    // set mode
    if (configuration_->getCSValue("START_X11") == "0") {
      ui_->radioButtonEGL->setChecked(true);
    } else {
      ui_->RadioX11->setChecked(true);
    }
    // set rotation
    if (configuration_->getCSValue("FLIP_SCREEN") == "0") {
      ui_->RadioScreenNormal->setChecked(true);
    } else {
      ui_->RadioScreenRotated->setChecked(true);
    }

    if (std::ifstream("/tmp/get_inputs")) {
      QFile inputsFile(QString("/tmp/get_inputs"));
      inputsFile.open(QIODevice::ReadOnly);
      QTextStream data_return(&inputsFile);
      QStringList inputs = data_return.readAll().split("\n");
      inputsFile.close();
      int cleaner = ui_->comboBoxPulseInput->count();
      while (cleaner > -1) {
        ui_->comboBoxPulseInput->removeItem(cleaner);
        cleaner--;
      }
      int indexin = inputs.count();
      int countin = 0;
      while (countin < indexin - 1) {
        ui_->comboBoxPulseInput->addItem(inputs[countin]);
        countin++;
      }
    }

    if (std::ifstream("/tmp/get_outputs")) {
      QFile outputsFile(QString("/tmp/get_outputs"));
      outputsFile.open(QIODevice::ReadOnly);
      QTextStream data_return(&outputsFile);
      QStringList outputs = data_return.readAll().split("\n");
      outputsFile.close();
      int cleaner = ui_->comboBoxPulseOutput->count();
      while (cleaner > -1) {
        ui_->comboBoxPulseOutput->removeItem(cleaner);
        cleaner--;
      }
      int indexout = outputs.count();
      int countout = 0;
      while (countout < indexout - 1) {
        ui_->comboBoxPulseOutput->addItem(outputs[countout]);
        countout++;
      }
    }

    ui_->comboBoxPulseOutput->setCurrentText(
        configuration_->readFileContent("/tmp/get_default_output"));
    ui_->comboBoxPulseInput->setCurrentText(
        configuration_->readFileContent("/tmp/get_default_input"));

    if (std::ifstream("/tmp/timezone_listing")) {
      QFile zoneFile(QString("/tmp/timezone_listing"));
      zoneFile.open(QIODevice::ReadOnly);
      QTextStream data_return(&zoneFile);
      QStringList zones = data_return.readAll().split("\n");
      zoneFile.close();
    }

    // set rtc
    QString rtcstring = configuration_->getParamFromFile("/boot/config.txt",
                                                         "dtoverlay=i2c-rtc");

    // set dac
    QString dac = "Custom";
    if (getparams[4] == "allo-boss-dac-pcm512x-audio") {
      dac = "Allo - Boss";
    }
    if (getparams[4] == "allo-piano-dac-pcm512x-audio") {
      dac = "Allo - Piano";
    }
    if (getparams[4] == "iqaudio-dacplus") {
      dac = "IQaudIO - Pi-DAC Plus/Pro/Zero";
    }
    if (getparams[4] == "iqaudio-dacplus,unmute_amp") {
      dac = "IQaudIO - Pi-Digi Amp Plus";
    }
    if (getparams[4] == "iqaudio-dacplus,auto_mute_amp") {
      dac = "IQaudIO - Pi-Digi Amp Plus - Automute";
    }
    if (getparams[4] == "iqaudio-digi-wm8804-audio") {
      dac = "IQaudIO - Pi-Digi Plus";
    }
    if (getparams[4] == "audioinjector-wm8731-audio") {
      dac = "Audioinjector - Zero/Stereo";
    }
    if (getparams[4] == "hifiberry-dac") {
      dac = "Hifiberry - DAC";
    }
    if (getparams[4] == "hifiberry-dacplus") {
      dac = "Hifiberry - DAC Plus";
    }
    if (getparams[4] == "hifiberry-digi") {
      dac = "Hifiberry - Digi";
    }
    if (getparams[4] == "hifiberry-digi-pro") {
      dac = "Hifiberry - Digi Pro";
    }
    if (getparams[4] == "hifiberry-amp") {
      dac = "Hifiberry - DAC Amp";
    }
    if (getparams[4] == "audio") {
      dac = "Raspberry Pi - Onboard";
    }

    // set shutdown disable
    if (configuration_->getCSValue("DISCONNECTION_POWEROFF_DISABLE") == "1") {
      ui_->CheckBoxDisableShutdown->setChecked(true);
    } else {
      ui_->CheckBoxDisableShutdown->setChecked(false);
    }

    // set screen off disable
    if (configuration_->getCSValue("DISCONNECTION_SCREEN_POWEROFF_DISABLE") ==
        "1") {
      ui_->CheckBoxDisableScreenOff->setChecked(true);
    } else {
      ui_->CheckBoxDisableScreenOff->setChecked(false);
    }

    // GPIO based shutdown
    ui_->comboBoxGPIOShutdown->setCurrentText(
        configuration_->getCSValue("IGNITION_PIN"));
    ui_->spinBoxGPIOShutdownDelay->setValue(
        configuration_->getCSValue("IGNITION_DELAY").toInt());

    // Wifi Hotspot
    if (configuration_->getCSValue("ENABLE_HOTSPOT") == "1") {
      ui_->CheckBoxHotspot->setChecked(true);
    } else {
      ui_->CheckBoxHotspot->setChecked(false);
    }
    // set bluetooth
    if (configuration_->getCSValue("ENABLE_BLUETOOTH") == "1") {
      // check external bluetooth enabled
      if (configuration_->getCSValue("EXTERNAL_BLUETOOTH") == "1") {
        ui_->RadioUseExternalBluetoothAdapter->setChecked(true);
      } else {
        ui_->RadioUseLocalBluetoothAdapter->setChecked(true);
      }
      // mac
      // ui_->lineEditExternalBluetoothAdapterAddress->setText(getparams[37]);
    } else {
      ui_->RadioDisableBluetooth->setChecked(true);
      ui_->lineEditExternalBluetoothAdapterAddress->setText("");
    }
    if (configuration_->getCSValue("ENABLE_PAIRABLE") == "1") {
      ui_->CheckBoxBluetoothAutoPair->setChecked(true);
    } else {
      ui_->CheckBoxBluetoothAutoPair->setChecked(false);
    }
    // set bluetooth type
    if (configuration_->getCSValue("ENABLE_BLUETOOTH") == "1") {
      QString bt = configuration_->getParamFromFile("/boot/config.txt",
                                                    "dtoverlay=pi3-disable-bt");
      if (bt.contains("pi3-disable-bt")) {
        ui_->comboBoxBluetooth->setCurrentText("external");
      } else {
        ui_->comboBoxBluetooth->setCurrentText("builtin");
      }
    } else {
      ui_->comboBoxBluetooth->setCurrentText("none");
    }
    ui_->comboBoxDayNight->setCurrentText(
        configuration_->getCSValue("DAYNIGHT_PIN"));
    if (configuration_->getCSValue("RTC_DAYNIGHT") == "1") {
      ui_->CheckBoxDisableDayNightRTC->setChecked(false);
    } else {
      ui_->CheckBoxDisableDayNightRTC->setChecked(true);
    }
    QString theme = configuration_->getParamFromFile(
        "/etc/plymouth/plymouthd.conf", "Theme");
    // wifi country code
    ui_->comboBoxCountryCode->setCurrentIndex(
        ui_->comboBoxCountryCode->findText(
            configuration_->getCSValue("WIFI_COUNTRY"),
            Qt::MatchFlag::MatchStartsWith));
    // set screen blank instead off
    if (configuration_->getCSValue("SCREEN_POWEROFF_OVERRIDE") == "1") {
      ui_->CheckBoxBlankOnly->setChecked(true);
    } else {
      ui_->CheckBoxBlankOnly->setChecked(false);
    }
  }
  // update network info
  updateNetworkInfo();
}

void SettingsWindow::onStartHotspot() {
  ui_->label_modeswitchprogress->setText("Wait ...");
  ui_->clientNetworkSelect->hide();
  ui_->label_notavailable->show();
  ui_->RadioClient->setEnabled(0);
  ui_->RadioHotspot->setEnabled(0);
  ui_->lineEdit_wlan0->setText("");
  ui_->lineEditWifiSSID->setText("");
  ui_->ButtonNetworkAuto->hide();
  qApp->processEvents();
  std::remove("/tmp/manual_hotspot_control");
  std::ofstream("/tmp/manual_hotspot_control");
  system("/opt/crankshaft/service_hotspot.sh start &");
}

void SettingsWindow::onStopHotspot() {
  ui_->label_modeswitchprogress->setText("Wait ...");
  ui_->clientNetworkSelect->hide();
  ui_->label_notavailable->show();
  ui_->RadioClient->setEnabled(0);
  ui_->RadioHotspot->setEnabled(0);
  ui_->lineEdit_wlan0->setText("");
  ui_->lineEditWifiSSID->setText("");
  ui_->lineEditPassword->setText("");
  ui_->ButtonNetworkAuto->hide();
  qApp->processEvents();
  system("/opt/crankshaft/service_hotspot.sh stop &");
}

void SettingsWindow::updateSystemInfo() {
  // free ram
  struct sysinfo info;
  sysinfo(&info);
  ui_->valueSystemFreeMem->setText(QString::number(info.freeram / 1024 / 1024) +
                                   " MB");
  // current cpu speed
  QString freq = configuration_->readFileContent(
      "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq");
  int currentfreq = freq.toInt() / 1000;
  ui_->valueSystemCPUFreq->setText(QString::number(currentfreq) + "MHz");
  // current cpu temp
  QString temp =
      configuration_->readFileContent("/sys/class/thermal/thermal_zone0/temp");
  int currenttemp = temp.toInt() / 1000;
  ui_->valueSystemCPUTemp->setText(QString::number(currenttemp) + "°C");
  // get remaining times
  QProcess process;
  process.start(
      "/bin/bash",
      QStringList()
          << "-c"
          << "systemctl list-timers -all | grep disconnect | awk {'print $1'}");
  process.waitForFinished(-1);
  QString stdout = process.readAllStandardOutput();
  if (stdout.simplified() != "n/a") {
    process.start("/bin/bash", QStringList()
                                   << "-c"
                                   << "systemctl list-timers -all | grep "
                                      "disconnect | awk {'print $5\" \"$6'}");
    process.waitForFinished(-1);
    QString stdout = process.readAllStandardOutput();
    if (stdout.simplified() != "") {
      ui_->valueDisconnectTimer->setText(stdout.simplified());
    } else {
      ui_->valueDisconnectTimer->setText("Stopped");
    }
  } else {
    ui_->valueDisconnectTimer->setText("Stopped");
  }
  process.start(
      "/bin/bash",
      QStringList()
          << "-c"
          << "systemctl list-timers -all | grep shutdown | awk {'print $1'}");
  process.waitForFinished(-1);
  stdout = process.readAllStandardOutput();
  if (stdout.simplified() != "n/a") {
    process.start("/bin/bash", QStringList()
                                   << "-c"
                                   << "systemctl list-timers -all | grep "
                                      "shutdown | awk {'print $5\" \"$6'}");
    process.waitForFinished(-1);
    QString stdout = process.readAllStandardOutput();
    if (stdout.simplified() != "") {
      ui_->valueShutdownTimer->setText(stdout.simplified());
    } else {
      ui_->valueShutdownTimer->setText("Stopped");
    }
  } else {
    ui_->valueShutdownTimer->setText("Stopped");
  }
}

}  // namespace ui
}  // namespace autoapp
}  // namespace openauto
}  // namespace f1x

void f1x::openauto::autoapp::ui::SettingsWindow::on_ButtonAudioTest_clicked() {
  ui_->labelTestInProgress->show();
  ui_->ButtonAudioTest->hide();
  qApp->processEvents();
  system("/usr/local/bin/crankshaft audio test");
  ui_->ButtonAudioTest->show();
  ui_->labelTestInProgress->hide();
}

void f1x::openauto::autoapp::ui::SettingsWindow::updateNetworkInfo() {
  if (!std::ifstream("/tmp/mode_change_progress")) {
    QNetworkInterface eth0if = QNetworkInterface::interfaceFromName("eth0");
    if (eth0if.flags().testFlag(QNetworkInterface::IsUp)) {
      QList<QNetworkAddressEntry> entrieseth0 = eth0if.addressEntries();
      if (!entrieseth0.isEmpty()) {
        QNetworkAddressEntry eth0 = entrieseth0.first();
        // qDebug() << "eth0: " << eth0.ip();
        ui_->lineEdit_eth0->setText(eth0.ip().toString());
      }
    } else {
      // qDebug() << "eth0: down";
      ui_->lineEdit_eth0->setText("interface down");
    }

    QNetworkInterface wlan0if = QNetworkInterface::interfaceFromName("wlan0");
    if (wlan0if.flags().testFlag(QNetworkInterface::IsUp)) {
      QList<QNetworkAddressEntry> entrieswlan0 = wlan0if.addressEntries();
      if (!entrieswlan0.isEmpty()) {
        QNetworkAddressEntry wlan0 = entrieswlan0.first();
        // qDebug() << "wlan0: " << wlan0.ip();
        ui_->lineEdit_wlan0->setText(wlan0.ip().toString());
      }
    } else {
      // qDebug() << "wlan0: down";
      ui_->lineEdit_wlan0->setText("interface down");
    }

    if (std::ifstream("/tmp/hotspot_active")) {
      ui_->RadioClient->setEnabled(1);
      ui_->RadioHotspot->setEnabled(1);
      ui_->RadioHotspot->setChecked(1);
      ui_->RadioClient->setChecked(0);
      ui_->label_modeswitchprogress->setText("Ok");
      ui_->lineEditWifiSSID->setText(configuration_->getParamFromFile(
          "/etc/hostapd/hostapd.conf", "ssid"));
      ui_->lineEditPassword->show();
      ui_->label_password->show();
      ui_->lineEditPassword->setText("1234567890");
      ui_->clientNetworkSelect->hide();
      ui_->ButtonNetworkAuto->hide();
      ui_->label_notavailable->show();
    } else {
      ui_->RadioClient->setEnabled(1);
      ui_->RadioHotspot->setEnabled(1);
      ui_->RadioHotspot->setChecked(0);
      ui_->RadioClient->setChecked(1);
      ui_->label_modeswitchprogress->setText("Ok");
      ui_->lineEditWifiSSID->setText(
          configuration_->readFileContent("/tmp/wifi_ssid"));
      ui_->lineEditPassword->hide();
      ui_->label_password->hide();
      ui_->lineEditPassword->setText("");
      ui_->clientNetworkSelect->show();
      ui_->label_notavailable->hide();
      ui_->ButtonNetworkAuto->show();

      if (!std::ifstream("/boot/crankshaft/network1.conf")) {
        ui_->ButtonNetwork1->hide();
        ui_->ButtonNetwork0->show();
      }
      if (!std::ifstream("/boot/crankshaft/network0.conf")) {
        ui_->ButtonNetwork1->hide();
        ui_->ButtonNetwork0->setText(configuration_->getCSValue("WIFI2_SSID"));
      }
      if (!std::ifstream("/boot/crankshaft/network0.conf") &&
          !std::ifstream("/boot/crankshaft/network1.conf")) {
        ui_->ButtonNetwork0->hide();
        ui_->ButtonNetwork1->hide();
        ui_->ButtonNetworkAuto->hide();
        ui_->label_notavailable->show();
      }
    }
  }
}

void f1x::openauto::autoapp::ui::SettingsWindow::on_ButtonNetwork0_clicked() {
  ui_->lineEdit_wlan0->setText("");
  ui_->lineEditWifiSSID->setText("");
  ui_->lineEditPassword->setText("");
  qApp->processEvents();
  system("/usr/local/bin/crankshaft network 0 >/dev/null 2>&1 &");
}

void f1x::openauto::autoapp::ui::SettingsWindow::on_ButtonNetwork1_clicked() {
  ui_->lineEdit_wlan0->setText("");
  ui_->lineEditWifiSSID->setText("");
  ui_->lineEditPassword->setText("");
  qApp->processEvents();
  system("/usr/local/bin/crankshaft network 1 >/dev/null 2>&1 &");
}
