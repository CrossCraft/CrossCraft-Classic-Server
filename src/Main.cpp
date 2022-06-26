#include <Stardust-Celeste.hpp>
#include "Server.hpp"

using namespace Stardust_Celeste;

/**
 * @brief Class to handle the server updates
 */
class MainState : public Core::ApplicationState
{
public:
    MainState() = default;
    ~MainState() = default;

    void on_start()
    {
        server = create_scopeptr<ClassicServer::Server>();
    }

    void on_cleanup()
    {
    }

    void on_update(Core::Application *app, double dt)
    {
        server->update(dt);
    }
    void on_draw(Core::Application *app, double dt) {}

private:
    ScopePtr<ClassicServer::Server> server;
};

/**
 * @brief Class to run the game state
 *
 */
class GameApplication : public Core::Application
{
public:
    /**
     * @brief On Start override
     */
    void on_start() override
    {
        SC_APP_INFO("CrossCraft Classic Server Starting!");

        state = create_refptr<MainState>();
        this->set_state(state);
    }

private:
    RefPtr<MainState> state;
};

/**
 * @brief Create a New Stardust Celeste Application object
 *
 * @return Core::Application*
 */
Core::Application *CreateNewSCApp()
{

    // Configure the engine
    Core::AppConfig config;
    config.headless = true;
    config.networking = true;

    Core::PlatformLayer::get().initialize(config);
    Stardust_Celeste::Utilities::Logger::get_app_log()->setCutoff(Utilities::LogLevel::Debug);

    // Return the game
    return new GameApplication();
}