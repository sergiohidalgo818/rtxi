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

#include "debug.hpp"
#include "rtos.hpp"
#include "event.hpp"
#include "rt.hpp"

RT::System::System(Event::Manager* em, IO::Connector* ioc) 
  : event_manager(em), io_connector(ioc)
{
  if (RT::OS::createTask<RT::System*>(this->task.get(), &RT::System::execute, this) != 0) {
    ERROR_MSG("RT::System::System : failed to create realtime thread\n");
    return;
  }
  this->event_manager->registerHandler(this);
}

RT::System::~System()
{
  RT::OS::deleteTask(this->task.get());
  this->event_manager->unregisterHandler(this);
}

int64_t RT::System::getPeriod()
{
  return this->task->period;
}

void RT::System::postTelemitry(const RT::Telemitry::Response* telemitry)
{
  this->eventFifo->writeRT(&telemitry, sizeof(RT::Telemitry::Response*));
}

void RT::System::receiveEvent(Event::Object* event)
{
  switch(event->getType()){
    case Event::Type::RT_PERIOD_EVENT :
      this->setPeriod(event);
      break;
    case Event::Type::RT_THREAD_INSERT_EVENT :
      this->insertThread(event);
      break;
    case Event::Type::RT_THREAD_REMOVE_EVENT :
      this->removeThread(event);
      break;
    case Event::Type::RT_DEVICE_INSERT_EVENT :
      this->insertDevice(event);
      break;
    case Event::Type::RT_DEVICE_REMOVE_EVENT :
      this->removeDevice(event);
      break;
    default:
      // Don't leave the caller waiting bro
      event->done();
  }
}

void RT::System::setPeriod(Event::Object* event)
{
  auto period = std::any_cast<int64_t>(event->getParam("period"));
  if (period == this->task->period) { return; }
  RT::System::CMD cmd(event->getType());
  cmd.setParam("period", std::any(period));
  this->eventFifo->write(&cmd, sizeof(RT::System::CMD*));
  cmd.wait();
  event->done();
}

void RT::System::setPeriod(RT::System::CMD* cmd)
{
  auto period = std::any_cast<int64_t>(cmd->getParam("period"));
  this->task->period = period;
  this->postTelemitry(&RT::Telemitry::RT_PERIOD_UPDATE);
  cmd->done();
}

void RT::System::updateDeviceList(RT::System::CMD* cmd)
{
  this->devices = std::any_cast<std::vector<RT::Device*>>(cmd->getParam("deviceList"));
  this->postTelemitry(&RT::Telemitry::RT_DEVICE_LIST_UPDATE);
  cmd->done();
}

void RT::System::updateThreadList(RT::System::CMD* cmd)
{
  this->threads = std::any_cast<std::vector<RT::Thread*>>(cmd->getParam("threadList"));
  this->postTelemitry(&RT::Telemitry::RT_THREAD_LIST_UPDATE);
  cmd->done();
}

void RT::System::executeCMD(RT::System::CMD* cmd)
{
  switch(cmd->getType()){
    case Event::Type::RT_PERIOD_EVENT :
      RT::System::setPeriod(cmd);
      break;
    case Event::Type::RT_DEVICE_INSERT_EVENT : 
    case Event::Type::RT_DEVICE_REMOVE_EVENT :
      RT::System::updateDeviceList(cmd);
      break;
    case Event::Type::RT_THREAD_INSERT_EVENT :
    case Event::Type::RT_THREAD_REMOVE_EVENT :
      RT::System::updateThreadList(cmd);
      break;
    default:
      RT::System::postTelemitry(&RT::Telemitry::RT_NOOP);
      // make sure the command is handled so caller can continue
      cmd->done();
  }
}

void RT::System::insertDevice(Event::Object* event)
{
  auto* device = std::any_cast<RT::Device*>(event->getParam("device"));
  if (device == nullptr) {
    ERROR_MSG("RT::System::insertDevice : invalid device pointer\n");
    return;
  }
  this->io_connector->insertBlock(device);
  std::vector<IO::Block*> device_list = this->io_connector->getDevices();
  RT::System::CMD cmd(event->getType());
  cmd.setParam("deviceList", std::any(device_list));
  this->eventFifo->write(&cmd, sizeof(RT::System::CMD*));
  cmd.wait();
  event->done();
}

void RT::System::removeDevice(Event::Object* event)
{
  auto* device = std::any_cast<RT::Device*>(event->getParam("device"));
  if (device == nullptr) {
    ERROR_MSG("RT::System::removeDevice : invalid device pointer\n");
    return;
  }
  // We have to make sure to deactivate device before removing
  device->setActive(false);
  this->io_connector->removeBlock(device);
  std::vector<IO::Block*> device_list = this->io_connector->getDevices();
  RT::System::CMD cmd(event->getType());
  cmd.setParam("deviceList", std::any(device_list));
  this->eventFifo->write(&cmd, sizeof(RT::System::CMD*));
  cmd.wait();
  event->done();
}

void RT::System::insertThread(Event::Object* event)
{
  auto* thread = std::any_cast<RT::Thread*>(event->getParam("thread"));
  if (thread == nullptr) {
    ERROR_MSG("RT::System::removeDevice : invalid device pointer\n");
    return;
  }
  this->io_connector->insertBlock(thread);
  std::vector<IO::Block*> thread_list = this->io_connector->getThreads();
  RT::System::CMD cmd(event->getType());
  cmd.setParam("threadList", std::any(thread_list));
  this->eventFifo->write(&cmd, sizeof(RT::System::CMD*));
  cmd.wait();
  event->done();
}

void RT::System::removeThread(Event::Object* event)
{
  auto* thread = std::any_cast<RT::Thread*>(event->getParam("thread"));
  if (thread == nullptr) {
    ERROR_MSG("RT::System::removeDevice : invalid device pointer\n");
    return;
  }
  // We have to make sure to deactivate thread before removing
  thread->setActive(false);
  this->io_connector->removeBlock(thread);
  std::vector<IO::Block*> thread_list = this->io_connector->getThreads();
  RT::System::CMD cmd(event->getType());
  cmd.setParam("threadList", std::any(thread_list));
  this->eventFifo->write(&cmd, sizeof(RT::System::CMD*));
  cmd.wait();
  event->done();
}

void RT::System::execute(RT::System* system)
{
  RT::System::CMD* cmd = nullptr;
  std::vector<Device*>::iterator iDevice;
  std::vector<Thread*>::iterator iThread;
  auto devicesBegin = system->devices.begin();
  auto devicesEnd = system->devices.end();
  auto threadListBegin = system->threads.begin();
  auto threadListEnd = system->threads.end();

  if (RT::OS::setPeriod(system->task.get(), RT::OS::DEFAULT_PERIOD) != 0) {
    ERROR_MSG(
        "RT::System::execute : failed to set the initial period of the "
        "realtime thread\n");
    return;
  }

  while (!system->task->task_finished) {
    RT::OS::sleepTimestep(system->task.get());

    for (iDevice = devicesBegin; iDevice != devicesEnd; ++iDevice){
      if ((*iDevice)->getActive()){
        (*iDevice)->read();
      }
    }

    for (iThread = threadListBegin; iThread != threadListEnd; ++iThread){
      if ((*iThread)->getActive()){
        (*iThread)->execute();
      }
    }

    for (iDevice = devicesBegin; iDevice != devicesEnd; ++iDevice){
      if ((*iDevice)->getActive()){
        (*iDevice)->write();
      }
    }

    if (system->eventFifo->readRT(&cmd, sizeof(RT::System::CMD*)) != 0) {
      do {
        system->executeCMD(cmd);
      } while (system->eventFifo->readRT(&cmd, sizeof(RT::System::CMD*)) != 0);

      cmd = nullptr;
      devicesBegin = system->devices.begin();
      threadListBegin = system->threads.begin();
    }
  }
}
