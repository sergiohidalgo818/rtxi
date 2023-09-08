#include <QApplication>
#include <iostream>

#include <boost/stacktrace.hpp>
#include <signal.h>
#include <string.h>

#include "debug.hpp"
#include "main_window.hpp"
#include "module.hpp"
#include "rt.hpp"
#include "rtxiConfig.h"
#include "workspace.hpp"

static void signal_handler(int signum)
{
  ERROR_MSG("signal_handler : signal type {} received\n", ::strsignal(signum));
  std::cerr << boost::stacktrace::stacktrace();
  exit(-1);
}

int main(int argc, char* argv[])
{
  signal(SIGINT, signal_handler);
  signal(SIGABRT, signal_handler);
  signal(SIGSEGV, signal_handler);

  std::cout << "Welcome to RTXI Version ";
  std::cout << RTXI_VERSION_MAJOR << ".";
  std::cout << RTXI_VERSION_MINOR << ".";
  std::cout << RTXI_VERSION_PATCH << "\n";

  // Initializing core classes
  auto event_manager = std::make_unique<Event::Manager>();
  auto rt_connector = std::make_unique<RT::Connector>();
  auto rt_system =
      std::make_unique<RT::System>(event_manager.get(), rt_connector.get());
  rt_system->createTelemitryProcessor();
  // Initializing GUI
  QApplication::setDesktopSettingsAware(false);
  auto* app = new QApplication(argc, argv);
  QApplication::connect(app, SIGNAL(lastWindowClosed()), app, SLOT(quit()));

  auto* rtxi_window = new MainWindow(event_manager.get());
  auto mod_manager = std::make_unique<Workspace::Manager>(event_manager.get());
  rtxi_window->loadWindow();
  int retval = QApplication::exec();
  delete rtxi_window;
  delete app;
  return retval;
}
