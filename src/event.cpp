/*
         The Real-Time eXperiment Interface (RTXI)
         Copyright (C) 2011 Georgia Institute of Technology, University of Utah,
   Will Cornell Medical College

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

#include <algorithm>
#include <future>

#include "event.hpp"

#include "debug.hpp"

std::string Event::type_to_string(Event::Type event_type)
{
  std::string return_string;
  switch (event_type) {
    case Event::Type::RT_PERIOD_EVENT:
      return_string = "SYSTEM : period";
      break;
    case Event::Type::RT_PREPERIOD_EVENT:
      return_string = "SYSTEM : pre period";
      break;
    case Event::Type::RT_POSTPERIOD_EVENT:
      return_string = "SYSTEM : post period";
      break;
    case Event::Type::RT_GET_PERIOD_EVENT:
      return_string = "SYSTEM : Period value requested";
      break;
    case Event::Type::RT_THREAD_INSERT_EVENT:
      return_string = "SYSTEM : thread insert";
      break;
    case Event::Type::RT_THREAD_REMOVE_EVENT:
      return_string = "SYSTEM : thread remove";
      break;
    case Event::Type::RT_DEVICE_INSERT_EVENT:
      return_string = "SYSTEM : device insert";
      break;
    case Event::Type::RT_SHUTDOWN_EVENT:
      return_string = "SYSTEM : shutdown";
      break;
    case Event::Type::RT_DEVICE_REMOVE_EVENT:
      return_string = "SYSTEM : device remove";
      break;
    case Event::Type::IO_LINK_INSERT_EVENT:
      return_string = "SYSTEM : link insert";
      break;
    case Event::Type::IO_LINK_REMOVE_EVENT:
      return_string = "SYSTEM : link remove";
      break;
    case Event::Type::RT_THREAD_PAUSE_EVENT:
      return_string = "SYSTEM : Thread paused";
      break;
    case Event::Type::RT_THREAD_UNPAUSE_EVENT:
      return_string = "SYSTEM : Thread unpaused";
      break;
    case Event::Type::RT_DEVICE_PAUSE_EVENT:
      return_string = "SYSTEM : Device paused";
      break;
    case Event::Type::RT_DEVICE_UNPAUSE_EVENT:
      return_string = "SYSTEM : Device unpaused";
      break;
    case Event::Type::RT_MODULE_PARAMETER_CHANGE_EVENT:
      return_string = "SYSTEM : parameter change";
      break;
    case Event::Type::RT_MODULE_STATE_CHANGE_EVENT:
      return_string = "SYSTEM : Module State change";
      break;
    case Event::Type::PLUGIN_INSERT_EVENT:
      return_string = "SYSTEM : plugin insert";
      break;
    case Event::Type::PLUGIN_REMOVE_EVENT:
      return_string = "SYSTEM : plugin remove";
      break;
    case Event::Type::SETTINGS_OBJECT_INSERT_EVENT:
      return_string = "SYSTEM : settings object insert";
      break;
    case Event::Type::SETTINGS_OBJECT_REMOVE_EVENT:
      return_string = "SYSTEM : settings object remove";
      break;
    case Event::Type::OPEN_FILE_EVENT:
      return_string = "SYSTEM : open file";
      break;
    case Event::Type::START_RECORDING_EVENT:
      return_string = "SYSTEM : start recording";
      break;
    case Event::Type::STOP_RECORDING_EVENT:
      return_string = "SYSTEM : stop recording";
      break;
    case Event::Type::ASYNC_DATA_EVENT:
      return_string = "SYSTEM : async data";
      break;
    case Event::Type::THRESHOLD_CROSSING_EVENT:
      return_string = "SYSTEM : threshold crossing event";
      break;
    case Event::Type::START_GENICAM_RECORDING_EVENT:
      return_string = "SYSTEM : start genicam recording";
      break;
    case Event::Type::PAUSE_GENICAM_RECORDING_EVENT:
      return_string = "SYSTEM : pause genicam recording";
      break;
    case Event::Type::STOP_GENICAM_RECORDING_EVENT:
      return_string = "SYSTEM : stop genicam recording";
      break;
    case Event::Type::GENICAM_SNAPSHOT_EVENT:
      return_string = "SYSTEM : genicam snap";
      break;
    case Event::Type::GENERIC_MODULE_EVENT:
      return_string = "MODULE : Generic Module Event";
      break;
    case Event::Type::NOOP:
      return_string = "SYSTEM : no operation";
      break;
    default:
      return_string = "UNKNOWN EVENT!";
  }
  return return_string;
}

Event::Object::Object(Event::Type et)
    : event_type(et)
    , processed(false)
{
}

Event::Object::Object(const Event::Object& obj)
    : params(obj.params)
    , event_type(obj.event_type)
    , processed(false)
{
}

std::string Event::Object::getName()
{
  return Event::type_to_string(this->event_type);
}

std::any Event::Object::getParam(const std::string& param_name)
{
  for (auto& parameter : params) {
    if (parameter.name == param_name) {
      return parameter.value;
    }
  }
  return std::any();
}

bool Event::Object::paramExists(const std::string& param_name)
{
  bool result = false;
  for (auto& parameter : params) {
    if (parameter.name == param_name) {
      result = true;
    }
  }
  return result;
}

void Event::Object::setParam(const std::string& param_name,
                             const std::any& param_value)
{
  for (auto& parameter : params) {
    if (parameter.name == param_name) {
      parameter.value = param_value;
      return;
    }
  }

  param temp = {};
  temp.name = param_name;
  temp.value = param_value;
  params.push_back(temp);
}

void Event::Object::wait()
{
  std::unique_lock<std::mutex> done_lock(this->processing_done_mut);
  this->processing_done_cond.wait(done_lock,
                                  [this]() { return this->processed; });
  // done_lock.unlock();
}

void Event::Object::done()
{
  std::unique_lock<std::mutex> done_lock(this->processing_done_mut);
  this->processed = true;
  done_lock.unlock();
  this->processing_done_cond.notify_one();
}

bool Event::Object::isdone() const
{
  return this->processed;
}

Event::Type Event::Object::getType()
{
  return this->event_type;
}

Event::Manager::Manager()
{
  this->event_thread = std::thread(&Event::Manager::processEvents, this);
}

Event::Manager::~Manager()
{
  // clear pointers and terminate event thread. We don't have to wait for
  // NOOP event to be handled as there won't be anything to do so.
  this->running = false;
  Event::Object event(Event::Type::RT_SHUTDOWN_EVENT);
  this->postEvent(&event);
  if (this->event_thread.joinable()) {
    this->event_thread.join();
  };
}

void Event::Manager::postEvent(Event::Object* event)
{
  std::unique_lock<std::mutex> lk(this->event_mut);
  this->event_q.push(event);
  this->available_event_cond.notify_all();
  lk.unlock();
  event->wait();
}

void Event::Manager::postEvent(const std::vector<Event::Object*>& events)
{
  // For performance provide postEvent that accepts multiple events
  std::unique_lock<std::mutex> lk(this->event_mut);
  for (auto* event : events) {
    this->event_q.push(event);
  }
  this->available_event_cond.notify_all();
  lk.unlock();
  for (auto* event : events) {
    event->wait();
  }
}

void Event::Manager::processEvents()
{
  Event::Object* tmp_event = nullptr;
  auto event_processor = [this](Event::Object* event)
  {
    if (event == nullptr) {
      ERROR_MSG("Null pointer event received by event processor. Ignoring");
      return;
    }
    std::shared_lock<std::shared_mutex> handlerlist_lock(this->handlerlist_mut);
    for (auto* handler : this->handlerList) {
      handler->receiveEvent(event);
    }
    handlerlist_lock.unlock();
    event->done();
  };
  std::unique_lock<std::mutex> event_lock(this->event_mut);
  // TODO: Turn this into a thread pool implementation for performance
  while (this->running) {
    this->available_event_cond.wait(
        event_lock, [this] { return !(this->event_q.empty()); });
    while (!event_q.empty()) {
      tmp_event = event_q.front();
      std::thread(event_processor, tmp_event).detach();
      this->event_q.pop();
      tmp_event = nullptr;
    }
  }
}

void Event::Manager::registerHandler(Event::Handler* handler)
{
  std::unique_lock<std::shared_mutex> write_lock(this->handlerlist_mut);
  auto location = std::find(handlerList.begin(), handlerList.end(), handler);
  if (location == handlerList.end()) {
    handlerList.push_back(handler);
  }
  write_lock.unlock();
}

void Event::Manager::unregisterHandler(Event::Handler* handler)
{
  std::unique_lock<std::shared_mutex> write_lock(this->handlerlist_mut);
  auto location = std::find(handlerList.begin(), handlerList.end(), handler);
  if (location != handlerList.end()) {
    handlerList.erase(location);
  }
  write_lock.unlock();
}

bool Event::Manager::isRegistered(Event::Handler* handler)
{
  std::shared_lock<std::shared_mutex> read_lock(this->handlerlist_mut);
  auto location = std::find(handlerList.begin(), handlerList.end(), handler);
  bool result = location != handlerList.end();
  read_lock.unlock();
  return result;
}
