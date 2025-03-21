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

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QMdiSubWindow>
#include <QPushButton>

#include "connector.hpp"

Connector::Panel::Panel(QMainWindow* mw, Event::Manager* ev_manager)
    : Widgets::Panel(std::string(Connector::MODULE_NAME), mw, ev_manager)
    , buttonGroup(new QGroupBox)
    , inputBlock(new QComboBox)
    , inputChannel(new QComboBox)
    , outputBlock(new QComboBox)
    , outputFlag(new QComboBox)
    , outputChannel(new QComboBox)
    , connectionBox(new QListWidget)
{
  setWhatsThis(
      "<p><b>Connector:</b><br>The Connector panel allows you to make "
      "connections between "
      "signals and slots in your workspace. Signals are generated by the DAQ "
      "card (associated "
      "with input channels) and by user modules. Available signals are listed "
      "in the \"Output "
      "Block\" drop-down box and available slots are listed in the \"Input "
      "Block\" drop-down box. "
      "The arrow button is a toggle button that turns connections on and off. "
      "Clicking the toggle "
      "button immediately makes a connection active or inactive in real-time. "
      "Current connections "
      "are listed in the \"Connections\" box.</p>");

  // Create main layout
  auto* layout = new QGridLayout;

  // Create child widget and layout for output block
  outputGroup = new QGroupBox(tr("Source"));
  auto* outputLayout = new QVBoxLayout;

  // Create elements for output
  outputLayout->addWidget(new QLabel(tr("Block:")), 1, Qt::Alignment());
  outputLayout->addWidget(outputBlock);
  outputLayout->addWidget(new QLabel(tr("Flag:")), 2, Qt::Alignment());
  outputLayout->addWidget(outputFlag);
  buildOutputFlagList();

  QObject::connect(outputBlock,
                   QOverload<int>::of(&QComboBox::activated),
                   this,
                   &Connector::Panel::buildOutputChannelList);
  QObject::connect(outputBlock,
                   QOverload<int>::of(&QComboBox::activated),
                   this,
                   &Connector::Panel::updateConnectionButton);
  QObject::connect(outputFlag,
                   QOverload<int>::of(&QComboBox::activated),
                   this,
                   &Connector::Panel::buildOutputChannelList);
  QObject::connect(outputFlag,
                   QOverload<int>::of(&QComboBox::activated),
                   this,
                   &Connector::Panel::updateConnectionButton);

  outputLayout->addWidget(new QLabel(tr("Channel:")), 3, Qt::Alignment());
  outputLayout->addWidget(outputChannel);
  QObject::connect(outputChannel,
                   QOverload<int>::of(&QComboBox::activated),
                   this,
                   &Connector::Panel::updateConnectionButton);

  // Assign layout to child widget
  outputGroup->setLayout(outputLayout);

  // Create child widget and layout for connection button
  auto* buttonLayout = new QVBoxLayout;

  // Create elements for button
  connectionButton = new QPushButton("Connect");
  connectionButton->setCheckable(true);
  buttonLayout->addWidget(connectionButton);
  QObject::connect(connectionButton,
                   &QPushButton::clicked,
                   this,
                   &Connector::Panel::toggleConnection);

  // Assign layout to child widget
  buttonGroup->setLayout(buttonLayout);

  // Create child widget and layout for input block
  inputGroup = new QGroupBox(tr("Destination"));
  auto* inputLayout = new QVBoxLayout;

  // Create elements for output
  inputLayout->addWidget(new QLabel(tr("Block:")), 1, Qt::Alignment());
  inputLayout->addWidget(inputBlock);
  QObject::connect(inputBlock,
                   QOverload<int>::of(&QComboBox::activated),
                   this,
                   &Connector::Panel::buildInputChannelList);
  QObject::connect(inputBlock,
                   QOverload<int>::of(&QComboBox::activated),
                   this,
                   &Connector::Panel::updateConnectionButton);

  inputLayout->addWidget(new QLabel(tr("Channel:")), 2, Qt::Alignment());
  inputLayout->addWidget(inputChannel);
  QObject::connect(inputChannel,
                   QOverload<int>::of(&QComboBox::activated),
                   this,
                   &Connector::Panel::updateConnectionButton);

  // Assign layout to child widget
  inputGroup->setLayout(inputLayout);

  // Create child widget and layout for connections box
  connectionGroup = new QGroupBox(tr("Connections"));
  auto* connectionLayout = new QVBoxLayout;

  // Create elements for connection box
  connectionLayout->addWidget(connectionBox);

  // Assign layout to child widget
  connectionGroup->setLayout(connectionLayout);

  // Attach child widget to parent widget
  layout->addWidget(outputGroup, 1, 0, 1, 2);
  layout->addWidget(buttonGroup, 2, 0, 1, 4);
  layout->addWidget(inputGroup, 1, 2, 1, 2);
  layout->addWidget(connectionGroup, 3, 0, 1, 4);

  // Set layout so that only connectionGroup expands when resized
  layout->setRowStretch(0, 0);
  layout->setRowStretch(2, 0);
  layout->setRowStretch(3, 1);

  // Attach layout to widget
  setLayout(layout);
  setWindowTitle(QString(this->getName().c_str()));

  // Set layout to Mdi
  this->getMdiWindow()->resize(500, this->sizeHint().height());

  // populate field with block and connection info
  this->syncBlockInfo();

  QObject::connect(this,
                   &Connector::Panel::updateBlockInfo,
                   this,
                   &Connector::Panel::syncBlockInfo);

  // a change to any of the connection parameters should highlight box or not
  QObject::connect(inputBlock,
                   &QComboBox::currentTextChanged,
                   this,
                   &Connector::Panel::highlightConnectionBox);
  QObject::connect(outputBlock,
                   &QComboBox::currentTextChanged,
                   this,
                   &Connector::Panel::highlightConnectionBox);
  QObject::connect(inputChannel,
                   &QComboBox::currentTextChanged,
                   this,
                   &Connector::Panel::highlightConnectionBox);
  QObject::connect(outputChannel,
                   &QComboBox::currentTextChanged,
                   this,
                   &Connector::Panel::highlightConnectionBox);
  QObject::connect(outputFlag,
                   &QComboBox::currentTextChanged,
                   this,
                   &Connector::Panel::highlightConnectionBox);
  QObject::connect(connectionBox,
                   &QListWidget::itemClicked,
                   this,
                   &Connector::Panel::reverseHighlightConnectionBox);
}

void Connector::Panel::buildBlockList()
{
  auto prev_input_block = inputBlock->currentData();
  auto prev_output_block = outputBlock->currentData();
  inputBlock->clear();
  outputBlock->clear();
  Event::Object event(Event::Type::IO_BLOCK_QUERY_EVENT);
  this->getRTXIEventManager()->postEvent(&event);
  this->blocks =
      std::any_cast<std::vector<IO::Block*>>(event.getParam("blockList"));
  for (auto* block : this->blocks) {
    if (block->getName().find("Probe") != std::string::npos
        || block->getName().find("Recording") != std::string::npos)
    {
      continue;
    }
    this->inputBlock->addItem(QString(block->getName().c_str()) + " "
                                  + QString::number(block->getID()),
                              QVariant::fromValue(block));
    this->outputBlock->addItem(QString(block->getName().c_str()) + " "
                                   + QString::number(block->getID()),
                               QVariant::fromValue(block));
  }
  inputBlock->setCurrentIndex(inputBlock->findData(prev_input_block));
  outputBlock->setCurrentIndex(outputBlock->findData(prev_output_block));
}

void Connector::Panel::buildConnectionList()
{
  Event::Object event(Event::Type::IO_ALL_CONNECTIONS_QUERY_EVENT);
  this->getRTXIEventManager()->postEvent(&event);
  this->links = std::any_cast<std::vector<RT::block_connection_t>>(
      event.getParam("connections"));
}

void Connector::Plugin::receiveEvent(Event::Object* event)
{
  switch (event->getType()) {
    case Event::Type::RT_THREAD_INSERT_EVENT:
    case Event::Type::RT_THREAD_REMOVE_EVENT:
    case Event::Type::RT_DEVICE_INSERT_EVENT:
    case Event::Type::RT_DEVICE_REMOVE_EVENT:
    case Event::Type::IO_LINK_INSERT_EVENT:
    case Event::Type::IO_LINK_REMOVE_EVENT:
      this->updatePanelInfo();
      break;
    default:
      break;
  }
}

void Connector::Plugin::updatePanelInfo()
{
  dynamic_cast<Connector::Panel*>(this->getPanel())->updateBlockInfo();
}

// This slot will be called by the plugin to update block list and connection
// info after a block has been inserted. The update will show up in the panel
void Connector::Panel::syncBlockInfo()
{
  this->buildBlockList();
  this->buildInputChannelList();
  this->buildOutputChannelList();
  this->buildConnectionList();

  connectionBox->clear();
  QString temp_list_text;
  QListWidgetItem* temp_list_item = nullptr;
  for (auto conn : this->links) {
    if (conn.dest->getName().find("Probe") != std::string::npos) {
      continue;
    }
    temp_list_text =
        QString::number(conn.src->getID()) + " "
        + QString(conn.src->getName().c_str()) + " "
        + QString(
            conn.src->getChannelName(conn.src_port_type, conn.src_port).c_str())
        + " ==> " + QString::number(conn.dest->getID()) + " "
        + QString(conn.dest->getName().c_str()) + " "
        + QString(conn.dest->getChannelName(IO::INPUT, conn.dest_port).c_str());
    temp_list_item = new QListWidgetItem(temp_list_text);
    temp_list_item->setData(Qt::UserRole, QVariant::fromValue(conn));
    connectionBox->addItem(temp_list_item);
  }
}

void Connector::Panel::buildInputChannelList()
{
  inputChannel->clear();
  if (inputBlock->count() == 0) {
    return;
  }

  // Get specific block
  if (!inputBlock->currentData().isValid()) {
    return;
  }
  auto* block = inputBlock->currentData().value<IO::Block*>();

  // Get list of channels from specific block
  for (size_t i = 0; i < block->getCount(IO::INPUT); ++i) {
    inputChannel->addItem(QString(block->getChannelName(IO::INPUT, i).c_str()),
                          QVariant::fromValue(i));
  }
  updateConnectionButton();
}

void Connector::Panel::buildOutputChannelList()
{
  outputChannel->clear();
  if (outputBlock->count() == 0) {
    return;
  }

  // Get specific block
  if (!outputBlock->currentData().isValid()
      || !outputFlag->currentData().isValid())
  {
    return;
  }
  auto* block = outputBlock->currentData().value<IO::Block*>();
  auto direction = outputFlag->currentData().value<IO::flags_t>();

  // Get list of channels from specific block
  for (size_t i = 0; i < block->getCount(direction); ++i) {
    outputChannel->addItem(QString(block->getChannelName(direction, i).c_str()),
                           QVariant::fromValue(i));
  }
  updateConnectionButton();
}

void Connector::Panel::buildOutputFlagList()
{
  outputFlag->addItem(QString("OUTPUT"), QVariant::fromValue(IO::OUTPUT));
  outputFlag->addItem(QString("INPUT"), QVariant::fromValue(IO::INPUT));
}

void Connector::Panel::highlightConnectionBox(const QString& /*item*/)
{
  // build info in the output group
  const QVariant src_variant = this->outputBlock->currentData();
  const QVariant src_type_variant = this->outputFlag->currentData();
  const QVariant src_chan_variant = this->outputChannel->currentData();
  const QVariant dest_variant = this->inputBlock->currentData();
  const QVariant dest_chan_variant = this->inputChannel->currentData();
  if (!src_variant.isValid() || !src_type_variant.isValid()
      || !dest_variant.isValid() || !src_chan_variant.isValid()
      || !dest_chan_variant.isValid())
  {
    connectionBox->setCurrentRow(-1);
    return;
  }

  RT::block_connection_t current_connection;
  current_connection.src = src_variant.value<IO::Block*>();
  current_connection.src_port_type = src_type_variant.value<IO::flags_t>();
  current_connection.src_port = src_chan_variant.value<size_t>();
  current_connection.dest = dest_variant.value<IO::Block*>();
  current_connection.dest_port = dest_chan_variant.value<size_t>();
  QVariant temp_conn_variant;
  // update connection button state
  for (int row = 0; row < connectionBox->count(); ++row) {
    temp_conn_variant = this->connectionBox->item(row)->data(Qt::UserRole);
    if (temp_conn_variant.value<RT::block_connection_t>() == current_connection)
    {
      connectionBox->setCurrentRow(row);
      return;
    };
  }
  connectionBox->setCurrentRow(-1);
}

void Connector::Panel::reverseHighlightConnectionBox(
    const QListWidgetItem* item)
{
  const auto connection =
      item->data(Qt::UserRole).value<RT::block_connection_t>();
  outputBlock->setCurrentIndex(
      outputBlock->findData(QVariant::fromValue(connection.src)));
  outputFlag->setCurrentIndex(
      outputFlag->findData(QVariant::fromValue(connection.src_port_type)));
  buildInputChannelList();
  outputChannel->setCurrentIndex(
      outputChannel->findData(QVariant::fromValue(connection.src_port)));
  inputBlock->setCurrentIndex(
      inputBlock->findData(QVariant::fromValue(connection.dest)));
  buildOutputChannelList();
  inputChannel->setCurrentIndex(
      inputChannel->findData(QVariant::fromValue(connection.dest_port)));
  updateConnectionButton();
}

void Connector::Panel::toggleConnection(bool down)
{
  RT::block_connection_t connection;
  const QVariant src_block = outputBlock->currentData();
  const QVariant src_type = outputFlag->currentData();
  const QVariant src_port = outputChannel->currentData();
  const QVariant dest_block = inputBlock->currentData();
  const QVariant dest_port = inputChannel->currentData();
  if (!src_block.isValid() || !src_type.isValid() || !src_port.isValid()
      || !dest_block.isValid() || !dest_port.isValid())
  {
    connectionButton->setDown(false);
    // Somehow the user was able to click the button when it should be
    // disabled... let's fix that
    connectionButton->setEnabled(false);
    return;
  }
  connection.src = src_block.value<IO::Block*>();
  connection.src_port_type = src_type.value<IO::flags_t>();
  connection.src_port = src_port.value<size_t>();
  connection.dest = dest_block.value<IO::Block*>();
  connection.dest_port = dest_port.value<size_t>();

  if (down) {
    Event::Object event(Event::Type::IO_LINK_INSERT_EVENT);
    event.setParam("connection", std::any(connection));
    this->getRTXIEventManager()->postEvent(&event);
  } else {
    Event::Object event(Event::Type::IO_LINK_REMOVE_EVENT);
    event.setParam("connection", std::any(connection));
    this->getRTXIEventManager()->postEvent(&event);
  }
  connectionButton->setDown(down);
  connectionButton->setChecked(down);
  syncBlockInfo();
}

void Connector::Panel::updateConnectionButton()
{
  if (inputChannel->count() == 0 || outputChannel->count() == 0) {
    connectionButton->setEnabled(false);
    return;
  }
  // QVariant connection_variant =
  // this->connectionBox->currentItem()->data(Qt::UserRole);
  const QVariant src_block = outputBlock->currentData();
  const QVariant src_type = outputFlag->currentData();
  const QVariant src_port = outputChannel->currentData();
  const QVariant dest_block = inputBlock->currentData();
  const QVariant dest_port = inputChannel->currentData();
  if (!src_block.isValid() || !src_type.isValid() || !src_port.isValid()
      || !dest_block.isValid() || !dest_port.isValid())
  {
    connectionButton->setDown(false);
    connectionButton->setChecked(false);
    connectionButton->setEnabled(false);
    return;
  }
  connectionButton->setEnabled(true);
  RT::block_connection_t connection;
  connection.src = src_block.value<IO::Block*>();
  connection.src_port_type = src_type.value<IO::flags_t>();
  connection.src_port = src_port.value<size_t>();
  connection.dest = dest_block.value<IO::Block*>();
  connection.dest_port = dest_port.value<size_t>();

  QListWidgetItem* temp_item = nullptr;
  for (int i = 0; i < connectionBox->count(); i++) {
    temp_item = connectionBox->item(i);
    if (temp_item->data(Qt::UserRole).value<RT::block_connection_t>()
        == connection)
    {
      connectionButton->setDown(true);
      connectionButton->setChecked(true);
      return;
    }
  }
  connectionButton->setDown(false);
  connectionButton->setChecked(false);
}

Connector::Plugin::Plugin(Event::Manager* ev_manager)
    : Widgets::Plugin(ev_manager, std::string(Connector::MODULE_NAME))
{
}

std::unique_ptr<Widgets::Plugin> Connector::createRTXIPlugin(
    Event::Manager* ev_manager)
{
  return std::make_unique<Connector::Plugin>(ev_manager);
}

Widgets::Panel* Connector::createRTXIPanel(QMainWindow* main_window,
                                           Event::Manager* ev_manager)
{
  return static_cast<Widgets::Panel*>(
      new Connector::Panel(main_window, ev_manager));
}

std::unique_ptr<Widgets::Component> Connector::createRTXIComponent(
    Widgets::Plugin* /*unused*/)
{
  return {nullptr};
}

Widgets::FactoryMethods Connector::getFactories()
{
  Widgets::FactoryMethods fact;
  fact.createPanel = &Connector::createRTXIPanel;
  fact.createComponent = &Connector::createRTXIComponent;
  fact.createPlugin = &Connector::createRTXIPlugin;
  return fact;
}
