/**
 * @file World.hpp
 * @author Nathan Bourgeois (iridescentrosesfall@gmail.com)
 * @brief The World
 * @version 0.1
 * @date 2022-01-12
 *
 * @copyright Copyright (c) 2022
 *
 */
#pragma once
#include "../BlockConst.hpp"
#include "../Config.hpp"
#include "../Server.hpp"
#include "../Utility.hpp"
#include <Utilities/Types.hpp>
#include <glm.hpp>
#include <map>
#include <memory>
#include <stdint.h>
#include <stdio.h>
#include <vector>

namespace ClassicServer {

using namespace Stardust_Celeste;
class Server;
/**
 * @brief The world
 *
 */
class World {
  public:
    /**
     * @brief Construct a new World object
     *
     * @param p Player to use
     */
    World(Server *server);

    /**
     * @brief Destroy the World object
     *
     */
    ~World();

    auto load_world() -> bool;

    /**
     * @brief Update (checks chunks, run chunk updates, tick updates);
     *
     * @param dt Delta Time
     */
    auto update(double dt) -> void;

    auto start_autosave() -> void;

    inline auto getIdx(int x, int y, int z) -> int {
        if (x >= 0 && x < 256 && y >= 0 && y < 64 && z >= 0 && z < 256)
            return (y * 256 * 256) + (z * 256) + x + 4;

        return -1;
    }

    uint8_t *worldData;
    float *hmap;
    Config cfg;

    auto spawn() -> void;
    auto save() -> void;

    auto blockUpdate() -> void;

    auto rtick() -> void;
    auto setBlock(int x, int y, int z, uint8_t block) -> void;

    static auto auto_save(World *wrld) -> void;

    /**
     * @brief Updates nearby block states
     *
     * @param ivec Block source from update
     */
    auto update_nearby_blocks(glm::ivec3 ivec) -> void;

    /**
     * @brief Calculate and add update to chunk
     *
     */
    auto add_update(glm::ivec3 ivec) -> void;

    std::vector<glm::ivec3> posUpdate;

  private:
    friend class CrossCraftGenerator;
    friend class ClassicGenerator;
    auto update_check(World *wrld, int blkr, glm::ivec3 chk) -> void;
    std::vector<glm::ivec3> updated;

    RefPtr<std::thread> autosave_thread;
    Server *serv;
};

} // namespace ClassicServer