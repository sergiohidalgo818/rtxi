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

/*
         This class creates and controls the drawing parameters
         A control panel is instantiated for all the active channels/modules
         and user selection is enabled to change color, style, width and other
         oscilloscope properties.
 */

#include <cmath>
#include <sstream>

#include "oscilloscope.h"

#include <qwt_plot_renderer.h>

#include "debug.hpp"
#include "main_window.hpp"
#include "oscilloscope/scope.h"
#include "rt.hpp"

void Oscilloscope::Plugin::receiveEvent(Event::Object* event)
{
  switch (event->getType()) {
    case Event::Type::RT_THREAD_INSERT_EVENT:
    case Event::Type::RT_DEVICE_INSERT_EVENT:
    case Event::Type::RT_THREAD_REMOVE_EVENT:
    case Event::Type::RT_DEVICE_REMOVE_EVENT:
      dynamic_cast<Oscilloscope::Panel*>(this->getPanel())->updateBlockInfo();
      break;
    default:
      break;
  }
}

Oscilloscope::Component* Oscilloscope::Plugin::getProbeComponent(
    IO::endpoint probeInfo)
{
  Oscilloscope::Component* component = nullptr;
  auto probe_loc =
      std::find_if(this->chanInfoList.begin(),
                   this->chanInfoList.end(),
                   [&](const Oscilloscope::channel_info& chan)
                   {
                     return chan.probe.block == probeInfo.block
                         && chan.probe.port == probeInfo.port
                         && chan.probe.direction == probeInfo.direction;
                   });
  if (probe_loc != this->chanInfoList.end()) {
    component = probe_loc->measuring_component;
  }
  return component;
}

void Oscilloscope::Panel::updateChannelScale(IO::endpoint probe_info)
{
  const int scale_index = this->scalesList->currentIndex();
  double chanscale = 1.0;
  switch (scale_index % 4) {
    case 0:
      chanscale = pow(10, 1 - scale_index / 4.0);
      break;
    case 1:
      chanscale = 5 * pow(10, -scale_index / 4.0);
      break;
    case 2:
      chanscale = 2.5 * pow(10, -scale_index / 4.0);
      break;
    case 3:
      chanscale = 2 * pow(10, -scale_index / 4.0);
      break;
    default:
      ERROR_MSG(
          "Oscilloscope::Panel::applyChannelTab : invalid chan.scale "
          "selection\n");
  }
  this->scopeWindow->setChannelScale(probe_info, chanscale);
}

void Oscilloscope::Panel::updateChannelOffset(IO::endpoint probe_info)
{
  const double chanoffset = this->offsetsEdit->text().toDouble()
      * pow(10, -3 * offsetsList->currentIndex());
  this->scopeWindow->setChannelOffset(probe_info, chanoffset);
}

void Oscilloscope::Panel::updateChannelLineWidth(IO::endpoint probe_info)
{
  const int width_indx = this->widthsList->currentIndex();
  QPen* pen = this->scopeWindow->getChannelPen(probe_info);
  if (width_indx < 0) {
    ERROR_MSG(
        "Oscilloscope::Panel::applyChannelTab : invalid style selection\n");
    pen->setStyle(Qt::SolidLine);
    return;
  }
  const Qt::PenStyle style =
      Oscilloscope::penStyles.at(static_cast<size_t>(width_indx));
  pen->setStyle(style);
}

void Oscilloscope::Panel::updateChannelLineStyle(IO::endpoint probe_info)
{
  QPen* pen = this->scopeWindow->getChannelPen(probe_info);
  const int style_indx = this->stylesList->currentIndex();
  if (style_indx < 0) {
    ERROR_MSG(
        "Oscilloscope::Panel::applyChannelTab : invalid style selection\n");
    pen->setStyle(Oscilloscope::penStyles[0]);
  } else {
    pen->setStyle(Oscilloscope::penStyles.at(static_cast<size_t>(style_indx)));
  }
}

void Oscilloscope::Panel::updateChannelPenColor(IO::endpoint probe_info)
{
  QPen* pen = this->scopeWindow->getChannelPen(probe_info);
  const int color_indx = this->colorsList->currentIndex();
  if (color_indx < 0) {
    ERROR_MSG(
        "Oscilloscope::Panel::applyChannelTab : invalid color selection\n");
    pen->setColor(Oscilloscope::penColors[0]);
  } else {
    pen->setColor(Oscilloscope::penColors.at(static_cast<size_t>(color_indx)));
  }
}

void Oscilloscope::Panel::updateChannelLabel(IO::endpoint probe_info)
{
  const QString chanlabel = QString::number(probe_info.block->getID()) + " "
      + QString::fromStdString(probe_info.block->getName()) + " "
      + this->scalesList->currentText();

  this->scopeWindow->setChannelLabel(probe_info, chanlabel);
}

void Oscilloscope::Panel::enableChannel()
{
  // make some initial checks
  if (!this->activateButton->isChecked()) {
    return;
  }

  // create component before we create the channel proper
  auto* oscilloscope_plugin =
      dynamic_cast<Oscilloscope::Plugin*>(this->getHostPlugin());
  auto* chanblock = this->blocksListDropdown->currentData().value<IO::Block*>();
  auto chanport = this->channelsList->currentData().value<size_t>();
  auto chandirection = this->typesList->currentData().value<IO::flags_t>();

  // this will try to create the probing component first. return if something
  // goes wrong
  const IO::endpoint probe {chanblock, chanport, chandirection};
  if (!oscilloscope_plugin->addProbe(probe)) {
    ERROR_MSG("Unable to create probing channel for block {}",
              chanblock->getName());
    return;
  }

  this->scopeWindow->createChannel(probe);
  // we were able to create the probe, so we should populate metainfo about it
  // in scope window
  this->updateChannelOffset(probe);
  this->updateChannelScale(probe);
  this->updateChannelLineWidth(probe);
  this->updateChannelLineStyle(probe);
  this->updateChannelPenColor(probe);
}

void Oscilloscope::Panel::disableChannel()
{
  // make some initial checks
  if (!this->activateButton->isChecked()) {
    return;
  }

  auto* oscilloscope_plugin =
      dynamic_cast<Oscilloscope::Plugin*>(this->getHostPlugin());
  auto* chanblock = this->blocksListDropdown->currentData().value<IO::Block*>();
  auto chanport = this->channelsList->currentData().value<size_t>();
  auto chandirection = this->typesList->currentData().value<IO::flags_t>();

  const IO::endpoint probe {chanblock, chanport, chandirection};
  // we should remove the scope channel before we attempt to remove block
  this->scopeWindow->removeChannel(probe);
  oscilloscope_plugin->removeProbe(probe);
}

void Oscilloscope::Panel::activateChannel(bool active)
{
  const bool enable =
      active && blocksListDropdown->count() > 0 && channelsList->count() > 0;
  scalesList->setEnabled(enable);
  offsetsEdit->setEnabled(enable);
  offsetsList->setEnabled(enable);
  colorsList->setEnabled(enable);
  widthsList->setEnabled(enable);
  stylesList->setEnabled(enable);
}

void Oscilloscope::Panel::apply()
{
  switch (tabWidget->currentIndex()) {
    case 0:
      applyChannelTab();
      break;
    case 1:
      applyDisplayTab();
      break;
    default:
      ERROR_MSG("Oscilloscope::Panel::showTab : invalid tab\n");
  }
}

void Oscilloscope::Panel::buildChannelList()
{
  channelsList->clear();
  if (blocksListDropdown->count() <= 0) {
    return;
  }

  if (blocksListDropdown->currentIndex() < 0) {
    blocksListDropdown->setCurrentIndex(0);
  }

  auto* block = this->blocksListDropdown->currentData().value<IO::Block*>();
  auto type = this->typesList->currentData().value<IO::flags_t>();

  for (size_t i = 0; i < block->getCount(type); ++i) {
    channelsList->addItem(
        QString::fromStdString(block->getChannelName(type, i)),
        QVariant::fromValue(i));
  }

  showChannelTab();
}

void Oscilloscope::Panel::showTab(int index)
{
  switch (index) {
    case 0:
      showChannelTab();
      break;
    case 1:
      showDisplayTab();
      break;
    default:
      ERROR_MSG("Oscilloscope::Panel::showTab : invalid tab\n");
  }
}

void Oscilloscope::Panel::setActivity(Oscilloscope::Component* comp,
                                      bool activity)
{
  const Event::Type event_type = activity ? Event::Type::RT_THREAD_UNPAUSE_EVENT
                                          : Event::Type::RT_THREAD_PAUSE_EVENT;
  Event::Object event(event_type);
  event.setParam("thread", std::any(static_cast<RT::Thread*>(comp)));
  this->getRTXIEventManager()->postEvent(&event);
}

void Oscilloscope::Panel::applyChannelTab()
{
  if (this->blocksListDropdown->count() <= 0
      || this->channelsList->count() <= 0) {
    return;
  }

  auto* block = this->blocksListDropdown->currentData().value<IO::Block*>();
  auto port = this->channelsList->currentData().value<size_t>();
  auto type = this->typesList->currentData().value<IO::flags_t>();
  auto* host_plugin =
      dynamic_cast<Oscilloscope::Plugin*>(this->getHostPlugin());
  const IO::endpoint probeInfo {block, port, type};
  Oscilloscope::Component* component =
      host_plugin->getProbeComponent(probeInfo);
  auto* oscilloscope_plugin =
      dynamic_cast<Oscilloscope::Plugin*>(this->getHostPlugin());
  if (!activateButton->isChecked()) {
    if (component == nullptr) {
      return;
    }
    this->setActivity(component, /*activity=*/false);
    scopeWindow->removeChannel(probeInfo);
    oscilloscope_plugin->removeProbe(probeInfo);
  } else {
    host_plugin->addProbe(probeInfo);
    this->scopeWindow->createChannel(probeInfo);
  }

  scopeWindow->replot();
  showChannelTab();
}

void Oscilloscope::Panel::applyDisplayTab()
{
  // Update X divisions
  double divT = NAN;
  if (timesList->currentIndex() % 3 == 1) {
    divT = 2 * pow(10, 3 - timesList->currentIndex() / 3.0);
  } else if (timesList->currentIndex() % 3 == 2) {
    divT = pow(10, 3 - timesList->currentIndex() / 3.0);
  } else {
    divT = 5 * pow(10, 3 - timesList->currentIndex() / 3.0);
  }
  scopeWindow->setDivT(divT);
  // scopeWindow->setPeriod(RT::System::getInstance()->getPeriod() * 1e-6);
  this->adjustDataSize();

  // Update trigger direction
  updateTrigger();

  // Update trigger threshold
  // double trigThreshold = trigsThreshEdit->text().toDouble()
  //    * pow(10, -3 * trigsThreshList->currentIndex());

  //// Update pre-trigger window for displaying
  // double trigWindow = trigWindowEdit->text().toDouble()
  //     * pow(10, -3 * trigWindowList->currentIndex());

  // std::list<Scope::Channel>::iterator trigChannel =
  //     scopeWindow->getChannelsEnd();
  // for (std::list<Scope::Channel>::iterator i =
  // scopeWindow->getChannelsBegin(),
  //                                          end =
  //                                          scopeWindow->getChannelsEnd();
  //      i != end;
  //      ++i)
  //   if (i->getLabel() == trigsChanList->currentText()) {
  //     trigChannel = i;
  //     break;
  //   }
  // if (trigChannel == scopeWindow->getChannelsEnd())
  //   trigDirection = Scope::NONE;

  // scopeWindow->setTrigger(
  //     trigDirection, trigThreshold, trigChannel, trigWindow);

  adjustDataSize();
  scopeWindow->replot();
  showDisplayTab();
}

void Oscilloscope::Panel::buildBlockList()
{
  Event::Object event(Event::Type::IO_BLOCK_QUERY_EVENT);
  this->getRTXIEventManager()->postEvent(&event);
  auto blocklist =
      std::any_cast<std::vector<IO::Block*>>(event.getParam("blockList"));
  blocksListDropdown->clear();
  for (auto* block : blocklist) {
    this->blocksListDropdown->addItem(QString::fromStdString(block->getName())
                                      + " " + QString::number(block->getID()), QVariant::fromValue(block));
  }
}

QWidget* Oscilloscope::Panel::createChannelTab(QWidget* parent)
{
  setWhatsThis(
      "<p><b>Oscilloscope: Channel Options</b><br>"
      "Use the dropdown boxes to select the signal streams you want to plot "
      "from "
      "any loaded modules or your DAQ device. You may change the plotting "
      "scale for "
      "the signal, apply a DC offset, and change the color and style of the "
      "line.</p>");

  auto* page = new QWidget(parent);

  // Create group and layout for buttons at bottom of scope
  auto* bttnLayout = new QGridLayout(page);

  // Create Channel box
  auto* row1Layout = new QHBoxLayout;
  auto* channelLabel = new QLabel(tr("Channel:"), page);
  row1Layout->addWidget(channelLabel);
  blocksListDropdown = new QComboBox(page);
  blocksListDropdown->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  blocksListDropdown->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  QObject::connect(blocksListDropdown,
                   SIGNAL(activated(int)),
                   this,
                   SLOT(buildChannelList()));
  row1Layout->addWidget(blocksListDropdown);

  // Create Type box
  typesList = new QComboBox(page);
  typesList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  typesList->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  typesList->addItem("Output", QVariant::fromValue(IO::OUTPUT));
  typesList->addItem("Input", QVariant::fromValue(IO::INPUT));
  row1Layout->addWidget(typesList);
  QObject::connect(
      typesList, SIGNAL(activated(int)), this, SLOT(buildChannelList()));

  // Create Channels box
  channelsList = new QComboBox(page);
  channelsList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  channelsList->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  QObject::connect(
      channelsList, SIGNAL(activated(int)), this, SLOT(showChannelTab()));
  row1Layout->addWidget(channelsList);

  // Create elements for display box
  row1Layout->addSpacerItem(
      new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
  auto* scaleLabel = new QLabel(tr("Scale:"), page);
  row1Layout->addWidget(scaleLabel);
  scalesList = new QComboBox(page);
  row1Layout->addWidget(scalesList);
  const QFont scalesListFont("DejaVu Sans Mono");
  QString postfix = "/div";
  std::array<std::string, 6> unit_array = {"V", "mV", "µV", "nV", "pV", "fV"};
  size_t unit_array_index = 0;
  std::array<double, 4> fixed_values = {10, 5, 2.5, 2};
  double value_scale = 1.0;
  scalesList->setFont(scalesListFont);
  std::string formatting = "{:.1f} {}/div";
  double temp_value = 0.0;
  while(unit_array_index < unit_array.size()){
    for(auto current_fixed_value : fixed_values){
      temp_value = current_fixed_value*std::pow(1e3,unit_array_index)*value_scale;
      if(temp_value < 1){
        unit_array_index++;
        if(unit_array_index >= 6) { break; }
        temp_value = current_fixed_value*std::pow(1e3,unit_array_index)*value_scale;
      }
      scalesList->addItem(QString::fromStdString(fmt::format(formatting, temp_value, unit_array.at(unit_array_index))),
                          current_fixed_value*value_scale);
    }
    value_scale = value_scale/10.0;
  }
  // Offset items
  auto* offsetLabel = new QLabel(tr("Offset:"), page);
  row1Layout->addWidget(offsetLabel);
  offsetsEdit = new QLineEdit(page);
  offsetsEdit->setMaximumWidth(offsetsEdit->minimumSizeHint().width() * 2);
  offsetsEdit->setValidator(new QDoubleValidator(offsetsEdit));
  row1Layout->addWidget(offsetsEdit);  //, Qt::AlignRight);
  offsetsList = new QComboBox(page);
  row1Layout->addWidget(offsetsList);  //, Qt::AlignRight);
  offsetsList->addItem("V", 1.0);
  offsetsList->addItem("mV", 1e-3);
  offsetsList->addItem(QString::fromUtf8("µV"), 1e-6);
  offsetsList->addItem("nV", 1e-9);
  offsetsList->addItem("pV", 1e-12);

  // Create elements for graphic
  auto* row2Layout = new QHBoxLayout;  //(page);
  row2Layout->setAlignment(Qt::AlignLeft);
  auto* colorLabel = new QLabel(tr("Color:"), page);
  row2Layout->addWidget(colorLabel);
  colorsList = new QComboBox(page);
  colorsList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  colorsList->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  row2Layout->addWidget(colorsList);
  QPixmap tmp(25, 25);
  std::string color_name;
  for(size_t i=0; i<penColors.size(); i++) {
    tmp.fill(penColors.at(i));
    color_name = Oscilloscope::color2string.at(i);
    colorsList->addItem(tmp, 
                        QString::fromStdString(color_name));
  }

  auto* widthLabel = new QLabel(tr("Width:"), page);
  row2Layout->addWidget(widthLabel);
  widthsList = new QComboBox(page);
  widthsList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  widthsList->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  row2Layout->addWidget(widthsList);
  tmp.fill(Qt::white);
  QPainter painter(&tmp);
  for (int i = 1; i < 6; i++) {
    painter.setPen(QPen(Oscilloscope::penColors.at(Oscilloscope::ColorID::Black), i));
    painter.drawLine(0, 12, 25, 12);
    widthsList->addItem(tmp, QString::number(i) + QString(" Pixels"), i);
  }

  // Create styles list
  auto* styleLabel = new QLabel(tr("Style:"), page);
  row2Layout->addWidget(styleLabel);
  stylesList = new QComboBox(page);
  stylesList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  stylesList->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  row2Layout->addWidget(stylesList);
  std::string temp_name;
  for(size_t i=0; i<Oscilloscope::penStyles.size(); i++){
    temp_name = Oscilloscope::penstyles2string.at(i);
    tmp.fill(Qt::white);
    painter.setPen(QPen(Oscilloscope::penColors.at(Oscilloscope::ColorID::Black), 
                        3, 
                        Oscilloscope::penStyles.at(i)));
    painter.drawLine(0, 12, 25, 12);
    stylesList->addItem(tmp, QString::fromStdString(temp_name));
  }

  // Activate button
  row2Layout->addSpacerItem(
      new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
  activateButton = new QPushButton("Enable Channel", page);
  row2Layout->addWidget(activateButton);
  activateButton->setCheckable(true);
  activateButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QObject::connect(
      activateButton, SIGNAL(toggled(bool)), this, SLOT(activateChannel(bool)));
  activateChannel(false);

  bttnLayout->addLayout(row1Layout, 0, 0);
  bttnLayout->addLayout(row2Layout, 1, 0);

  return page;
}

QWidget* Oscilloscope::Panel::createDisplayTab(QWidget* parent)
{
  setWhatsThis(
      "<p><b>Oscilloscope: Display Options</b><br>"
      "Use the dropdown box to select the time scale for the Oscilloscope. "
      "This "
      "scaling is applied to all signals plotted in the same window. You may "
      "also "
      "set a trigger on any signal that is currently plotted in the window. A "
      "yellow "
      "line will appear at the trigger threshold.</p>");

  auto* page = new QWidget(parent);

  // Scope properties
  auto* displayTabLayout = new QGridLayout(page);

  // Create elements for time settings
  auto* row1Layout = new QHBoxLayout;
  row1Layout->addWidget(new QLabel(tr("Time/Div:"), page));
  timesList = new QComboBox(page);
  row1Layout->addWidget(timesList);
  const QFont timeListFont("DejaVu Sans Mono");
  timesList->setFont(timeListFont);
  timesList->addItem("5 s/div", 5.0);
  timesList->addItem("2 s/div", 2.0);
  timesList->addItem("1 s/div", 1.0);
  timesList->addItem("500 ms/div", 0.5);
  timesList->addItem("200 ms/div", 0.2);
  timesList->addItem("100 ms/div", 0.1);
  timesList->addItem("50 ms/div", 0.05);
  timesList->addItem("20 ms/div", 0.02);
  timesList->addItem("10 ms/div", 0.01);
  timesList->addItem("5 ms/div", 5e-3);
  timesList->addItem("2 ms/div", 2e-3);
  timesList->addItem("1 ms/div", 1e-3);
  timesList->addItem(QString::fromUtf8("500 µs/div"), 500e-6);
  timesList->addItem(QString::fromUtf8("200 µs/div"), 200e-6);
  timesList->addItem(QString::fromUtf8("100 µs/div"), 100e-6);
  timesList->addItem(QString::fromUtf8("50 µs/div"), 50e-6);
  timesList->addItem(QString::fromUtf8("20 µs/div"), 20e-6);
  timesList->addItem(QString::fromUtf8("10 µs/div"), 10e-6);
  timesList->addItem(QString::fromUtf8("5 µs/div"), 5e-6);
  timesList->addItem(QString::fromUtf8("2 µs/div"), 2e-6);
  timesList->addItem(QString::fromUtf8("1 µs/div"), 1e-6);
  timesList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  timesList->setSizeAdjustPolicy(QComboBox::AdjustToContents);

  auto* refreshLabel = new QLabel(tr("Refresh:"), page);
  row1Layout->addWidget(refreshLabel);
  refreshDropdown = new QComboBox(page);
  row1Layout->addWidget(refreshDropdown);
  refreshDropdown->addItem("60 Hz");
  refreshDropdown->addItem("120 Hz");
  refreshDropdown->addItem("240 Hz");

  // Display box for Buffer bit. Push it to the right.
  row1Layout->addSpacerItem(
      new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
  auto* bufferLabel = new QLabel(tr("Buffer Size (MB):"), page);
  row1Layout->addWidget(bufferLabel);
  sizesEdit = new QLineEdit(page);
  sizesEdit->setMaximumWidth(sizesEdit->minimumSizeHint().width() * 3);
  sizesEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  row1Layout->addWidget(sizesEdit);
  sizesEdit->setText(QString::number(scopeWindow->getDataSize()));
  sizesEdit->setEnabled(false);

  // Trigger box
  auto* row2Layout = new QHBoxLayout;
  row2Layout->addWidget(new QLabel(tr("Edge:"), page));
  trigsGroup = new QButtonGroup(page);

  auto* off = new QRadioButton(tr("Off"), page);
  trigsGroup->addButton(off, Oscilloscope::Trigger::NONE);
  row2Layout->addWidget(off);
  auto* plus = new QRadioButton(tr("+"), page);
  trigsGroup->addButton(plus, Oscilloscope::Trigger::POS);
  row2Layout->addWidget(plus);
  auto* minus = new QRadioButton(tr("-"), page);
  trigsGroup->addButton(minus, Oscilloscope::Trigger::NEG);
  row2Layout->addWidget(minus);

  row2Layout->addWidget(new QLabel(tr("Channel:"), page));
  trigsChanList = new QComboBox(page);
  trigsChanList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  trigsChanList->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  row2Layout->addWidget(trigsChanList);

  row2Layout->addWidget(new QLabel(tr("Threshold:"), page));
  trigsThreshEdit = new QLineEdit(page);
  trigsThreshEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  trigsThreshEdit->setMaximumWidth(trigsThreshEdit->minimumSizeHint().width()
                                   * 3);
  row2Layout->addWidget(trigsThreshEdit);
  trigsThreshEdit->setValidator(new QDoubleValidator(trigsThreshEdit));
  trigsThreshList = new QComboBox(page);
  trigsThreshList->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  row2Layout->addWidget(trigsThreshList);
  trigsThreshList->addItem("V", 1.0);
  trigsThreshList->addItem("mV", 1e-3);
  trigsThreshList->addItem(QString::fromUtf8("µV"), 1e-6);
  trigsThreshList->addItem("nV", 1e-9);
  trigsThreshList->addItem("pV", 1e-12);

  // TODO: determine the proper implementation of trigger windows
  // row2Layout->addWidget(new QLabel(tr("Window:"), page));
  // trigWindowEdit = new QLineEdit(page);
  // trigWindowEdit->setText(QString::number(scopeWindow->getWindowTimewidth()));
  // trigWindowEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  // trigWindowEdit->setMaximumWidth(trigWindowEdit->minimumSizeHint().width() *
  // 3);

  // trigWindowEdit->setValidator(new QDoubleValidator(trigWindowEdit));
  // row2Layout->addWidget(trigWindowEdit);
  // trigWindowList = new QComboBox(page);
  // trigWindowList->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  // row2Layout->addWidget(trigWindowList);
  // trigWindowList->addItem("s");
  // trigWindowList->addItem("ms");
  // trigWindowList->addItem(QString::fromUtf8("µs"));
  // trigWindowList->setCurrentIndex(1);

  displayTabLayout->addLayout(row1Layout, 0, 0);
  displayTabLayout->addLayout(row2Layout, 1, 0);

  return page;
}

void Oscilloscope::Panel::syncBlockInfo()
{
  this->buildBlockList();
}

// Aggregates all channel information to show for configuration
// in the display tab
void Oscilloscope::Panel::showChannelTab()
{
  auto type = static_cast<IO::flags_t>(this->typesList->currentData().toInt());

  auto* block = this->blocksListDropdown->currentData().value<IO::Block*>();
  auto port = this->channelsList->currentData().value<size_t>();
  const IO::endpoint chan {block, port, type};
  const double scale = this->scopeWindow->getChannelScale(chan);
  double offset = this->scopeWindow->getChannelOffset(chan);
  scalesList->setCurrentIndex(
      static_cast<int>(round(4 * (log10(1 / scale) + 1))));
  int offsetUnits = 0;
  if (offset * std::pow(10, -3 * offsetsList->count() - 3) < 1) {
    offset = 0;
    offsetUnits = 0;
  } else {
    while (fabs(offset) < 1 && offsetUnits < offsetsList->count()) {
      offset *= 1000;
      offsetUnits++;
    }
  }
  offsetsEdit->setText(QString::number(offset));
  offsetsList->setCurrentIndex(offsetUnits);

  // set pen characteristics
  QPen* pen = this->scopeWindow->getChannelPen(chan);
  if(pen == nullptr) { 
    colorsList->setCurrentIndex(0);
    widthsList->setCurrentIndex(0);
    stylesList->setCurrentIndex(0);
  } else {
    const auto* color_loc = std::find(Oscilloscope::penColors.begin(),
                                      Oscilloscope::penColors.end(),
                                      pen->color());
    colorsList->setCurrentIndex(static_cast<int>(color_loc - Oscilloscope::penColors.begin()));
    widthsList->setCurrentIndex(pen->width());
    // set style
    switch (pen->style()) {
      case Qt::SolidLine:
        stylesList->setCurrentIndex(0);
        break;
      case Qt::DashLine:
        stylesList->setCurrentIndex(1);
        break;
      case Qt::DotLine:
        stylesList->setCurrentIndex(2);
        break;
      case Qt::DashDotLine:
        stylesList->setCurrentIndex(3);
        break;
      case Qt::DashDotDotLine:
        stylesList->setCurrentIndex(4);
        break;
      default:
        stylesList->setCurrentIndex(0);
    }
  }
}

void Oscilloscope::Panel::showDisplayTab()
{
  timesList->setCurrentIndex(
      static_cast<int>(round(3 * log10(1 / scopeWindow->getDivT()) + 11)));

  // refreshsSpin->setValue(scopeWindow->getRefresh());

  // Find current trigger value and update gui
  Oscilloscope::Trigger::Info trigInfo;
  auto* oscilloscope_plugin =
      dynamic_cast<Oscilloscope::Plugin*>(this->getHostPlugin());
  if (oscilloscope_plugin != nullptr) {
    trigInfo = oscilloscope_plugin->getTriggerInfo();
  }
  this->trigsGroup->button(static_cast<int>(trigInfo.trigger_direction))
      ->setChecked(true);

  trigsChanList->clear();
  std::vector<Oscilloscope::channel_info> channelList;
  if (oscilloscope_plugin != nullptr) {
    channelList = oscilloscope_plugin->getChannelsList();
  }
  std::string direction_str;
  for (const auto& chanInfo : channelList) {
    direction_str = chanInfo.probe.direction == IO::INPUT ? "INPUT" : "OUTPUT";
    trigsChanList->addItem(chanInfo.name + " "
                           + QString::fromStdString(direction_str) + " "
                           + QString::number(chanInfo.probe.port));
  }
  trigsChanList->addItem("<None>");
  auto trig_list_iter =
      std::find_if(channelList.begin(),
                   channelList.end(),
                   [&](const Oscilloscope::channel_info& chan_info)
                   {
                     return chan_info.probe.block == trigInfo.block
                         && chan_info.probe.port == trigInfo.port
                         && chan_info.probe.direction == trigInfo.io_direction;
                   });
  if (trig_list_iter != channelList.end()) {
    trigsChanList->setCurrentIndex(
        static_cast<int>(trig_list_iter - channelList.begin() + 1));
  }

  int trigThreshUnits = 0;
  double trigThresh = trigInfo.threshold;
  if (trigThresh * std::pow(10, -3 * this->trigsThreshList->count() - 1) < 1) {
    trigThreshUnits = 0;
    trigThresh = 0;
  } else {
    while (fabs(trigThresh) < 1
           && trigThreshUnits < this->trigsThreshList->count()) {
      trigThresh *= 1000;
      ++trigThreshUnits;
    }
  }
  trigsThreshList->setCurrentIndex(trigThreshUnits);
  trigsThreshEdit->setText(QString::number(trigThresh));

  sizesEdit->setText(QString::number(scopeWindow->getDataSize()));
}

Oscilloscope::Panel::Panel(QMainWindow* mw, Event::Manager* ev_manager)
    : Modules::Panel(std::string(Oscilloscope::MODULE_NAME), mw, ev_manager)
    , tabWidget(new QTabWidget)
    , scopeWindow(new Scope(this))
    , layout(new QVBoxLayout)
    , scopeGroup(new QWidget(this))
    , setBttnGroup(new QGroupBox(this))
{
  setWhatsThis(
      "<p><b>Oscilloscope:</b><br>The Oscilloscope allows you to plot any "
      "signal "
      "in your workspace in real-time, including signals from your DAQ card "
      "and those "
      "generated by user modules. Multiple signals are overlaid in the window "
      "and "
      "different line colors and styles can be selected. When a signal is "
      "added, a legend "
      "automatically appears in the bottom of the window. Multiple "
      "oscilloscopes can "
      "be instantiated to give you multiple data windows. To select signals "
      "for plotting, "
      "use the right-click context \"Panel\" menu item. After selecting a "
      "signal, you must "
      "click the \"Enable\" button for it to appear in the window. To change "
      "signal settings, "
      "you must click the \"Apply\" button. The right-click context \"Pause\" "
      "menu item "
      "allows you to start and stop real-time plotting.</p>");

  // Create tab widget
  tabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QObject::connect(
      tabWidget, SIGNAL(currentChanged(int)), this, SLOT(showTab(int)));

  auto* scopeLayout = new QHBoxLayout(this);
  scopeLayout->addWidget(scopeWindow);
  scopeGroup->setLayout(scopeLayout);
  auto* setBttnLayout = new QHBoxLayout(this);

  // Creat buttons
  pauseButton = new QPushButton("Pause");
  pauseButton->setCheckable(true);
  QObject::connect(pauseButton, SIGNAL(released()), this, SLOT(togglePause()));
  setBttnLayout->addWidget(pauseButton);
  applyButton = new QPushButton("Apply");
  QObject::connect(applyButton, SIGNAL(released()), this, SLOT(apply()));
  setBttnLayout->addWidget(applyButton);
  settingsButton = new QPushButton("Screenshot");
  QObject::connect(
      settingsButton, SIGNAL(released()), this, SLOT(screenshot()));
  setBttnLayout->addWidget(settingsButton);

  // Attach layout
  setBttnGroup->setLayout(setBttnLayout);

  // Create tabs
  tabWidget->setTabPosition(QTabWidget::North);
  tabWidget->addTab(createChannelTab(this), "Channel");
  tabWidget->addTab(createDisplayTab(this), "Display");

  // Setup main layout
  layout->addWidget(scopeGroup);
  layout->addWidget(tabWidget);
  layout->addWidget(setBttnGroup);

  // Set
  setLayout(layout);

  // Show stuff
  adjustDataSize();
  showDisplayTab();
  getMdiWindow()->setMinimumSize(this->minimumSizeHint().width(), 450);
  getMdiWindow()->resize(this->minimumSizeHint().width() + 50, 600);

  // Initialize vars
  setWindowTitle(tr(std::string(Oscilloscope::MODULE_NAME).c_str()));

  auto* otimer = new QTimer;
  otimer->setTimerType(Qt::PreciseTimer);
  QObject::connect(otimer, SIGNAL(timeout()), this, SLOT(timeoutEvent()));
  otimer->start(Oscilloscope::FrameRates::HZ60);

  QObject::connect(
      this, SIGNAL(updateBlockInfo()), this, SLOT(syncBlockInfo()));
  this->updateBlockInfo();
  this->buildChannelList();
  scopeWindow->replot();
}

Oscilloscope::Component::Component(Modules::Plugin* hplugin,
                                   const std::string& probe_name)
    : Modules::Component(hplugin,
                         probe_name,
                         Oscilloscope::get_default_channels(),
                         Oscilloscope::get_default_vars())
{
}

// TODO: Handle trigger synchronization bettween oscilloscope components
void Oscilloscope::Component::execute()
{
  Oscilloscope::sample sample {};
  auto state =
      getValue<Modules::Variable::state_t>(Oscilloscope::PARAMETER::STATE);
  switch (state) {
    case Modules::Variable::EXEC: {
      sample.time = RT::OS::getTime();
      sample.value = this->readinput(0)[0];
      this->fifo->writeRT(&sample, sizeof(Oscilloscope::sample));
      break;
    }
    case Modules::Variable::INIT: 
    case Modules::Variable::UNPAUSE: 
      this->setValue(Oscilloscope::PARAMETER::STATE, Modules::Variable::EXEC);
      break;
    case Modules::Variable::PAUSE:
    case Modules::Variable::MODIFY:
    case Modules::Variable::EXIT:
    case Modules::Variable::PERIOD:
      break;
  }
}

void Oscilloscope::Panel::screenshot()
{
  QwtPlotRenderer renderer;
  renderer.exportTo(scopeWindow, "screenshot.pdf");
}

void Oscilloscope::Panel::togglePause()
{
  Event::Type event_type = Event::Type::NOOP;
  if (this->pauseButton->isChecked()) {
    event_type = Event::Type::RT_THREAD_PAUSE_EVENT;
  } else {
    event_type = Event::Type::RT_THREAD_UNPAUSE_EVENT;
  }
  auto* oscilloscope_plugin =
      dynamic_cast<Oscilloscope::Plugin*>(this->getHostPlugin());
  const std::vector<Oscilloscope::channel_info> channelList =
      oscilloscope_plugin->getChannelsList();
  std::vector<Event::Object> events;
  for (const auto& channel : channelList) {
    events.emplace_back(event_type);
    events.back().setParam(
        "thread", static_cast<RT::Thread*>(channel.measuring_component));
  }
  this->getRTXIEventManager()->postEvent(events);
}

void Oscilloscope::Component::flushFifo()
{
  Oscilloscope::sample sample;
  while (this->fifo->read(&sample, sizeof(Oscilloscope::sample)) > 0) {
  }
}

void Oscilloscope::Panel::adjustDataSize()
{
  Event::Object event(Event::Type::RT_GET_PERIOD_EVENT);
  this->getRTXIEventManager()->postEvent(&event);
  auto period = std::any_cast<int64_t>(event.getParam("period"));
  const double timedivs = scopeWindow->getDivT();
  const double xdivs =
      static_cast<double>(scopeWindow->getDivX()) / static_cast<double>(period);
  const size_t size = static_cast<size_t>(ceil(timedivs + xdivs)) + 1;
  scopeWindow->setDataSize(size);
  sizesEdit->setText(QString::number(scopeWindow->getDataSize()));
}

void Oscilloscope::Panel::updateTrigger() {}

void Oscilloscope::Panel::timeoutEvent()
{
  Oscilloscope::sample sample;
  std::vector<Oscilloscope::sample> sample_vector;
  const size_t sample_count = this->scopeWindow->getDataSize();
  auto* oscilloscope_plugin =
      dynamic_cast<Oscilloscope::Plugin*>(this->getHostPlugin());
  for (auto channel : oscilloscope_plugin->getChannelsList()) {
    while (channel.fifo->read(&sample, sizeof(Oscilloscope::sample)) > 0) {
      sample_vector.push_back(sample);
    }
    this->scopeWindow->setData(channel.probe, sample_vector);
    sample_vector.assign(sample_count, {0.0, 0});
  }
  this->scopeWindow->drawCurves();
  // size_t size;
  // while (fifo.read(&size, sizeof(size), false)) {
  //   double data[size];
  //   if (fifo.read(data, sizeof(data)))
  //     scopeWindow->setData(data, size);
  // }
}

Oscilloscope::Plugin::Plugin(Event::Manager* ev_manager)
    : Modules::Plugin(ev_manager, std::string(Oscilloscope::MODULE_NAME))
{
}

Oscilloscope::Plugin::~Plugin()
{
  std::vector<Event::Object> unloadEvents;
  for (auto& oscilloscope_component : this->componentList) {
    unloadEvents.emplace_back(Event::Type::RT_THREAD_REMOVE_EVENT);
    unloadEvents.back().setParam(
        "thread", std::any(static_cast<RT::Thread*>(&oscilloscope_component)));
  }
  this->getEventManager()->postEvent(unloadEvents);
}

bool Oscilloscope::Plugin::addProbe(IO::endpoint probe_info)
{
  Oscilloscope::channel_info chan_info;
  chan_info.name = QString::number(probe_info.block->getID()) + " "
      + QString::fromStdString(probe_info.direction == IO::OUTPUT ? "Output "
                                                                  : "Input ")
      + QString::fromStdString(probe_info.block->getName())
      + " port: " + QString::number(probe_info.port);
  this->componentList.emplace_back(this, chan_info.name.toStdString());
  chan_info.probe = probe_info;
  chan_info.measuring_component = &this->componentList.back();
  Event::Object event(Event::Type::RT_THREAD_INSERT_EVENT);
  event.setParam(
      "thread",
      std::any(static_cast<RT::Thread*>(chan_info.measuring_component)));
  this->getEventManager()->postEvent(&event);
  return true;
  // TODO: complete proper handling of errors if not able to register probe
  // thread
}

void Oscilloscope::Plugin::removeProbe(IO::endpoint probe_info)
{
  auto probe_loc =
      std::find_if(this->chanInfoList.begin(),
                   this->chanInfoList.end(),
                   [&](const Oscilloscope::channel_info& chann)
                   {
                     return chann.probe.block == probe_info.block
                         && chann.probe.direction == probe_info.direction
                         && chann.probe.port == probe_info.port;
                   });
  if (probe_loc == this->chanInfoList.end()) {
    return;
  }
  Event::Object event(Event::Type::RT_THREAD_REMOVE_EVENT);
  event.setParam(
      "thread",
      std::any(static_cast<RT::Thread*>(probe_loc->measuring_component)));
  this->getEventManager()->postEvent(&event);
}

std::unique_ptr<Modules::Plugin> Oscilloscope::createRTXIPlugin(
    Event::Manager* ev_manager)
{
  return std::make_unique<Oscilloscope::Plugin>(ev_manager);
}

Modules::Panel* Oscilloscope::createRTXIPanel(QMainWindow* main_window,
                                              Event::Manager* ev_manager)
{
  return static_cast<Modules::Panel*>(
      new Oscilloscope::Panel(main_window, ev_manager));
}

std::unique_ptr<Modules::Component> Oscilloscope::createRTXIComponent(
    Modules::Plugin* /*host_plugin*/)
{
  return std::unique_ptr<Oscilloscope::Component>(nullptr);
}

Modules::FactoryMethods Oscilloscope::getFactories()
{
  Modules::FactoryMethods fact;
  fact.createPanel = &Oscilloscope::createRTXIPanel;
  fact.createComponent = &Oscilloscope::createRTXIComponent;
  fact.createPlugin = &Oscilloscope::createRTXIPlugin;
  return fact;
}
