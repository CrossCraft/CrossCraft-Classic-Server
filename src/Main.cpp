#include <Stardust-Celeste.hpp>

using namespace Stardust_Celeste;

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
    }

private:
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

    // Return the game
    return new GameApplication();
}