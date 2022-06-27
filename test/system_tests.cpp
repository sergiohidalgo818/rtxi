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

#include <chrono>
#include <thread>

#include <gmock/gmock.h>

#include "system_tests.hpp"

TEST_F(SystemTest, checkTelemitry)
{
  Event::Object event(Event::Type::NOOP);
  this->system->receiveEvent(&event);
  event.wait();
  ASSERT_EQ(RT::Telemitry::RT_NOOP, this->system->getTelemitry());
}

TEST_F(SystemTest, shutdown)
{
  Event::Object ev(Event::Type::RT_SHUTDOWN_EVENT);
  this->system->receiveEvent(&ev);
  ev.wait();
  ASSERT_EQ(this->system->getTelemitry(), RT::Telemitry::RT_SHUTDOWN);
}

TEST_F(SystemTest, getPeriod)
{
  // Check with default period
  auto period = 1000000ll;
  ASSERT_EQ(period, system->getPeriod());
}

TEST_F(SystemTest, setPeriod)
{
  Event::Object ev(Event::Type::RT_PERIOD_EVENT);
  ev.setParam("period", RT::OS::DEFAULT_PERIOD/2);
  this->system->receiveEvent(&ev);
  ev.wait();
  EXPECT_EQ(RT::Telemitry::RT_PERIOD_UPDATE, this->system->getTelemitry());
  ASSERT_EQ(RT::OS::DEFAULT_PERIOD/2, system->getPeriod());
  ev.setParam("period", RT::OS::DEFAULT_PERIOD);
  this->system->receiveEvent(&ev);
  ev.wait();
  EXPECT_EQ(RT::Telemitry::RT_PERIOD_UPDATE, this->system->getTelemitry());
  ASSERT_EQ(RT::OS::DEFAULT_PERIOD, system->getPeriod());
}

TEST_F(SystemTest, insertDevice)
{
  std::string defaultInputChannelName = "CHANNEL INPUT";
  std::string defaultInputChannelDescription =
      "DEFAULT INPUT CHANNEL DESCRIPTION";
  std::string defaultOutputChannelName = "CHANNEL OUTPUT";
  std::string defaultOutputChannelDescription =
      "DEFAULT OUTPUT CHANNEL DESCRIPTION";
  std::vector<IO::channel_t> defaultChannelList;

  // Generates a default block with single input and output channel
  IO::channel_t defaultInputChannel = {};
  defaultInputChannel.name = defaultInputChannelName;
  defaultInputChannel.description = defaultInputChannelDescription;
  defaultInputChannel.flags = IO::INPUT;
  defaultInputChannel.data_size = 1;
  IO::channel_t defaultOutputChannel = {};
  defaultOutputChannel.name = defaultOutputChannelName;
  defaultOutputChannel.description = defaultOutputChannelDescription;
  defaultOutputChannel.flags = IO::OUTPUT;
  defaultOutputChannel.data_size = 1;
  defaultChannelList.push_back(defaultInputChannel);
  defaultChannelList.push_back(defaultOutputChannel);

  RT::Device* mock_device = new MockRTDevice("mockdevice", defaultChannelList);

  this->io_connector->insertBlock(mock_device);
  Event::Object ev(Event::Type::RT_DEVICE_INSERT_EVENT);
  RT::Device* device_ptr = mock_device;
  ev.setParam("device",  device_ptr);
  this->system->receiveEvent(&ev);
  ev.wait();
  ASSERT_EQ(this->system->getTelemitry(), RT::Telemitry::RT_DEVICE_LIST_UPDATE);
  delete mock_device;
  device_ptr = nullptr;
}