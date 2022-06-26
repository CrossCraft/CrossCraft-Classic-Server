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
#include <Utilities/Types.hpp>
#include <glm.hpp>
#include <map>
#include <memory>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include "../Config.hpp"
#include "../Utility.hpp"

namespace ClassicServer
{

    using namespace Stardust_Celeste;

  /**
 * @brief The world
 *
 */
  class World
  {
  public:
    /**
     * @brief Construct a new World object
     *
     * @param p Player to use
     */
    World();

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

    inline auto getIdx(int x, int y, int z) -> int
    {
      if (x >= 0 && x < 256 && y >= 0 && y < 64 && z >= 0 && z < 256)
        return (y * 256 * 256) + (z * 256) + x + 4;

      return -1;
    }

    uint8_t *worldData;
    float *hmap;
    Config cfg;

    auto spawn() -> void;
    auto save() -> void;

    static auto auto_save(World* wrld) -> void;

  private:
    friend class CrossCraftGenerator;
    friend class ClassicGenerator;

    RefPtr<std::thread> autosave_thread;
  };

} // namespace ClassicServer