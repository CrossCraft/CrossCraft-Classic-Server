#include "World.hpp"
#include "../Utility.hpp"
#include "Generation/NoiseUtil.hpp"
#include "Generation/WorldGenUtil.hpp"
#include <Platform/Platform.hpp>
#include <Rendering/Rendering.hpp>
#include <Utilities/Input.hpp>
#include <gtx/rotate_vector.hpp>
#include <iostream>
#include <zlib.h>

namespace ClassicServer {

inline uint32_t HostToNetwork4(const void *a_Value) {
    uint32_t buf;
    memcpy(&buf, a_Value, sizeof(buf));
    buf = ntohl(buf);
    return buf;
}

auto World::auto_save(World* wrld) -> void {
    while (true) {
        SC_APP_INFO("Server: Autosaving...");
        wrld->save();
        SC_APP_INFO("Server: Done!");

        //Save every minute
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 60));
    }
}

World::World() {
    worldData = reinterpret_cast<uint8_t *>(
        calloc(256 * 64 * 256 + 4, sizeof(uint8_t)));

    uint32_t size = 256 * 64 * 256;
    size = HostToNetwork4(&size);
    memcpy(worldData, &size, 4);

    NoiseUtil::initialize();
}

auto World::start_autosave() -> void {
    autosave_thread = create_refptr<std::thread>(World::auto_save, this);
}

auto World::spawn() -> void {}

auto World::load_world() -> bool {
    gzFile save_file = gzopen("save.ccc", "rb");
    gzrewind(save_file);

    int version = 0;
    gzread(save_file, &version, sizeof(int) * 1);

    if (version != 1)
        return false;

    gzread(save_file, &worldData[4], 256 * 64 * 256);
    gzclose(save_file);

    return true;
}

auto World::save() -> void {
    gzFile save_file = gzopen("save.ccc", "wb");
    if (save_file != nullptr) {
        const int save_version = 1;
        gzwrite(save_file, &save_version, 1 * sizeof(int));
        gzwrite(save_file, &worldData[4], 256 * 64 * 256);
        gzclose(save_file);
    }
}

World::~World() {
    autosave_thread->join();

    free(worldData);

    // Destroy height map
    if (hmap != nullptr)
        free(hmap);
}

void World::update(double dt) {}

} // namespace ClassicServer
