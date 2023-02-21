#ifndef MODULE_HPP
#define MODULE_HPP

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QValidator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "event.hpp"
#include "io.hpp"
#include "main_window.hpp"
#include "rt.hpp"

namespace Modules
{

namespace Variable
{
enum variable_t : size_t
{
  INT_PARAMETER = 0,
  DOUBLE_PARAMETER,
  UINT_PARAMETER,
  STATE,
  COMMENT,
  UNKNOWN
};

/*!
 * Flag passed to DefaultGUIModel::update to signal the kind of update.
 *
 * \sa DefaultGUIModel::update()
 */
enum state_t : int64_t
{
  INIT, /*!< The parameters need to be initialized.         */
  EXEC, /*!< The module is in execution mode                */
  MODIFY, /*!< The parameters have been modified by the user. */
  PERIOD, /*!< The system period has changed.                 */
  PAUSE, /*!< The Pause button has been activated            */
  UNPAUSE, /*!< When the pause button has been deactivated     */
  EXIT, /*!< When the module has been told to exit        */
};

std::string state2string(state_t state);

/*!
 * Structure used to store information about module upon creation.
 * It is a structure describing module specific constants and
 * variables.
 *
 * \param name The name of the channel
 * \param description short description of the channel
 * \param vartype type of variable that is stored
 *
 * \sa IO::Block::Block()
 */
struct Info
{
  size_t id=0;
  std::string name;
  std::string description;
  Modules::Variable::variable_t vartype;
  std::variant<int64_t, double, uint64_t, std::string, state_t> value;
};

}  // namespace Variable

class DefaultGUILineEdit : public QLineEdit
{
  Q_OBJECT

public:
  DefaultGUILineEdit(QWidget* parent);

  void blacken();
  QPalette palette;

public slots:
  void redden();
};  // class DefaultGUILineEdi

// Forward declare plugin class for the component and panel private pointers
class Plugin;

class Component : public RT::Thread
{
public:
  Component(Modules::Plugin* hplugin,
            const std::string& mod_name,
            const std::vector<IO::channel_t>& channels,
            const std::vector<Modules::Variable::Info>& variables);

  template<typename T>
  T getValue(const size_t& var_id)
  {
    return std::get<T>(this->parameters.at(var_id).value);
  }

  template<typename T>
  void setValue(const size_t& var_id, T value)
  {
    this->parameters.at(var_id).value = value;
  }

  std::string getDescription(const size_t& var_id);
  std::string getValueString(const size_t& var_id);

  // Here are a list of functions inherited from RT::Thread

  void execute() override;

  // virtual void input(size_t channel, const std::vector<double>& data)
  // override; virtual const std::vector<double>& output(size_t channel)
  // override;

private:
  std::vector<Modules::Variable::Info> parameters;
  Modules::Plugin* hostPlugin;
  bool active;
};

class Panel : public QWidget
{
  Q_OBJECT
public:
  Panel(const std::string& mod_name,
        MainWindow* mw,
        Event::Manager* event_manager);

  /*
   * Getter function go allow customization of
   * user interface
   */
  QGridLayout* getLayout() { return layout; };

  /*!
   * Callback function that is called when the system state changes.
   *
   * \param flag The kind of update to signal.
   */
  virtual void update(Modules::Variable::state_t flag);

  /*!
   * Function that builds the Qt GUI.
   *
   * \param var The structure defining the module's parameters, states, inputs,
   * and outputs. \param size The size of the structure vars.
   *
   * \sa DefaultGUIModel::update_flags_t
   */
  virtual void createGUI(const std::vector<Modules::Variable::Info>& vars,
                         MainWindow* mw);

  void setHostPlugin(Modules::Plugin* hplugin) { this->hostPlugin = hplugin; }

public slots:

  /*!
   * Function that resizes widgets to properly fit layouts after overloading
   */
  void resizeMe();

  /*!
   * Function that allows the object to safely delete and unload itself.
   */
  virtual void exit();

  /*!
   * Function that updates the GUI with new parameter values.
   *
   * \sa DefaultGUIModel::update_flags_t
   */
  virtual void refresh();

  /*!
   * Function that calls DefaultGUIModel::update with the MODIFY flag
   *
   * \sa DefaultGUIModel::update_flags_t
   */
  virtual void modify();

  /*!
   * Function that pauses/unpauses the model.
   */
  virtual void pause(bool);

protected:
  /*!
   * Get the value of the parameter in the GUI, and update the value
   *   within the Workspace.
   *
   * \param name The parameter's name.
   * \return The value of the parameter.
   */
  QString getParameter(const QString& var_name);

  /*!
   * Set the value of this parameter within the Workspace and GUI.
   *
   * \param name The name of the parameter.
   * \param ref A reference to the parameter.
   *
   */
  void setParameter(const QString& var_name, double value);
  void setParameter(const QString& var_name, uint64_t value);
  void setParameter(const QString& var_name, int value);

  /*!
   *
   */
  QString getComment(const QString& name);

  /*!
   *
   */
  void setComment(const QString& var_name, const QString& comment);

  /*!
   * Set the reference to this state within the Workspace
   *   via Workspace::setData().
   *
   * \param name The state's name.
   * \param ref A reference to the state.
   *
   * \sa Workspace::setData()
   */
  void setState(const QString& name, Modules::Variable::state_t ref);

  void closeEvent(QCloseEvent* event) override;
  std::string getName() { return this->name; }
  Modules::Plugin* getHostPlugin() { return this->hostPlugin; }

  MainWindow* getMainWindowPtr() { return this->main_window; }
  Event::Manager* getRTXIEventManager() { return this->event_manager; }

private:
  MainWindow* main_window = nullptr;
  QWidget* gridBox = nullptr;
  QGroupBox* buttonGroup = nullptr;
  std::string name;
  QMdiSubWindow* subWindow = nullptr;
  Modules::Plugin* hostPlugin = nullptr;
  QGridLayout* layout = nullptr;
  Event::Manager* event_manager = nullptr;

  // Default buttons
  QPushButton* pauseButton = nullptr;
  QPushButton* modifyButton = nullptr;
  QPushButton* unloadButton = nullptr;

  struct param_t
  {
    QLabel* label;
    QString str_value;
    DefaultGUILineEdit* edit;
    Modules::Variable::variable_t type = Modules::Variable::UNKNOWN;
    Modules::Variable::Info info;
  };
  std::unordered_map<QString, param_t> parameter;
  QPalette palette;
};

class Plugin : public Event::Handler
{
public:
  Plugin(Event::Manager* ev_manager,
         MainWindow* mw,
         const std::string& mod_name);
  Plugin(const Plugin& plugin) = delete;  // copy constructor
  Plugin& operator=(const Plugin& plugin) =
      delete;  // copy assignment noperator
  Plugin(Plugin&&) = delete;  // move constructor
  Plugin& operator=(Plugin&&) = delete;  // move assignment operator
  virtual ~Plugin();

  void attachComponent(std::unique_ptr<Modules::Component> component);
  void attachPanel(Modules::Panel* panel);
  int64_t getComponentIntParameter(const size_t& parameter_id);
  uint64_t getComponentUIntParameter(const size_t& parameter_id);
  double getComponentDoubleParameter(const size_t& parameter_id);

  int setComponentIntParameter(const size_t& parameter_id,
                               int64_t value);
  int setComponentDoubleParameter(const size_t& parameter_id,
                                  double value);
  int setComponentUintParameter(const size_t& parameter_id,
                                uint64_t value);
  int setComponentComment(const size_t& parameter_id, std::string value);
  int setComponentState(const size_t& parameter_id,
                        Modules::Variable::state_t value);

  std::string getName() const { return this->name; }
  bool getActive();
  int setActive(bool state);
  void receiveEvent(Event::Object* event) override;
  void* getHandle() { return this->handle; }

  /*!
   * Get the name of the library from which the object was loaded.
   *
   * \return The library file the object from which the object was created.
   */
  std::string getLibrary() const { return this->library; }

  std::unique_ptr<Modules::Plugin> load();

  /*!
   * A mechanism which an object can use to unload itself. Should only be
   *   called from within the GUI thread.
   */
  void unload();

  // These functions are here in order to have backwards compatibility
  // with previous versions of RTXI that used DefaultGuiModel
  // (before RTXI 3.0.0)

  void registerComponent();

protected:
  // owned pointers
  std::unique_ptr<Modules::Component> plugin_component;

  // not owned pointers (managed by external objects)
  Event::Manager* event_manager = nullptr;
  MainWindow* main_window = nullptr;  // Qt handles this lifetime
  Modules::Panel* widget_panel = nullptr;  // Qt handles this lifetime

private:
  std::string library;
  void* handle =
      nullptr;  // if it is a shared object then this will not be null
  std::string name;
};

struct FactoryMethods
{
  std::unique_ptr<Modules::Plugin> (*createPlugin)(Event::Manager*,
                                                   MainWindow*) = nullptr;
  std::unique_ptr<Modules::Component> (*createComponent)(Modules::Plugin*) =
      nullptr;
  Modules::Panel* (*createPanel)(MainWindow*, Event::Manager*) = nullptr;
};

class Manager : public Event::Handler
{
public:
  Manager(Event::Manager* event_manager, MainWindow* mw);
  ~Manager();

  int loadPlugin(const std::string& library);
  void unloadPlugin(Modules::Plugin* plugin);
  void receiveEvent(Event::Object* event) override;
  bool isRegistered(const Modules::Plugin* plugin);

private:
  void registerModule(std::unique_ptr<Modules::Plugin> module);
  void unregisterModule(Modules::Plugin* plugin);

  void registerFactories(std::string module_name, Modules::FactoryMethods);
  void unregisterFactories(std::string module_name);

  std::unordered_map<std::string, std::vector<std::unique_ptr<Modules::Plugin>>>
      rtxi_modules_registry;
  std::unordered_map<std::string, Modules::FactoryMethods>
      rtxi_factories_registry;
  Event::Manager* event_manager;
  MainWindow* main_window;
};

}  // namespace Modules

#endif
