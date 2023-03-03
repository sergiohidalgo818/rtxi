/*
         The Real-Time eXperiment Interface (RTXI)
         Copyright (C) 2011 Georgia Institute of Technology, University of Utah,
   Weill Cornell Medical College

         This program is free software: you can redistribute it and/or modify
         it under the terms of the GNU General Public License as published by
         the Free Software Foundation, either version 3 of the License, or
         (at your option) any later version.

         This program is distributed in the hope that it will be useful,
         but WITHOUT ANY WARRANTY; without even the implied warranty of
         MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
         GNU General Public License for more details.

         You should have received a copy of the GNU General Public License
         along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef SYSTEM_CONTROL_H
#define SYSTEM_CONTROL_H

#include <QtWidgets>

#include "event.hpp"
#include "main_window.hpp"
#include "module.hpp"

class SystemControlPanel : public Modules::Panel
{
  Q_OBJECT

public:
  SystemControlPanel(MainWindow* mw, Event::Manager* ev_manager);

public slots:
  void apply();
  void display();
  void updateDevice();
  void updateFreq();
  void updatePeriod();

private:
  void __display();
  void receiveEvent(const Event::Object*);

  QGroupBox* deviceGroup = nullptr;
  QGroupBox* analogGroup = nullptr;
  QGroupBox* digitalGroup = nullptr;
  QGroupBox* buttonGroup = nullptr;

  QMdiSubWindow* subWindow = nullptr;

  QComboBox* deviceList = nullptr;
  QComboBox* analogChannelList = nullptr;
  QComboBox* analogRangeList = nullptr;
  QComboBox* analogDownsampleList = nullptr;
  QComboBox* analogReferenceList = nullptr;
  QComboBox* analogSubdeviceList = nullptr;
  QComboBox* analogUnitPrefixList = nullptr;
  QComboBox* analogUnitList = nullptr;
  QComboBox* analogUnitPrefixList2 = nullptr;
  QComboBox* analogUnitList2 = nullptr;
  QLineEdit* analogGainEdit = nullptr;
  QLineEdit* analogZeroOffsetEdit = nullptr;
  QPushButton* analogActiveButton = nullptr;
  QPushButton* analogCalibrationButton = nullptr;

  QComboBox* digitalChannelList = nullptr;
  QComboBox* digitalDirectionList = nullptr;
  QComboBox* digitalSubdeviceList = nullptr;
  QPushButton* digitalActiveButton = nullptr;

  bool rateUpdate;
  QComboBox* freqUnitList = nullptr;
  QComboBox* periodUnitList = nullptr;
  QLineEdit* freqEdit = nullptr;
  QLineEdit* periodEdit = nullptr;
};

class SystemControl : public Modules::Plugin
{
  Q_OBJECT

public:
  SystemControl();
  ~SystemControl();

public slots:

  void createControlPanel(void);

private:
  static SystemControl* instance;

  void removeControlPanel(SystemControlPanel*);

  std::list<SystemControlPanel*> panelList;
};

#endif /* SYSTEM_CONTROL_H */
