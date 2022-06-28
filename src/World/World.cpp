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

#include "../OutgoingPackets.hpp"

namespace ClassicServer {

inline uint32_t HostToNetwork4(const void *a_Value) {
    uint32_t buf;
    memcpy(&buf, a_Value, sizeof(buf));
    buf = ntohl(buf);
    return buf;
}

auto World::auto_save(World *wrld) -> void {
    while (true) {
        SC_APP_INFO("Server: Autosaving...");
        wrld->save();
        SC_APP_INFO("Server: Done!");

        // Save every minute
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 60));
    }
}

World::World(Server *server) {
    worldData = reinterpret_cast<uint8_t *>(
        calloc(256 * 64 * 256 + 4, sizeof(uint8_t)));

    uint32_t size = 256 * 64 * 256;
    size = HostToNetwork4(&size);
    memcpy(worldData, &size, 4);

    NoiseUtil::initialize();
    serv = server;
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

int rtick_count = 0;
void World::update(double dt) {
    // We are ticking 20 times per second. Rtick = 4 times.
    rtick_count++;
    if (rtick_count == 5) {
        rtick_count = 0;

        rtick();
    }
}

auto World::setBlock(int x, int y, int z, uint8_t block) -> void {
    auto idx = getIdx(x, y, z);
    worldData[idx] = block;

    // Send Packet
    auto ptr = create_refptr<Outgoing::SetBlock>();
    ptr->PacketID = Outgoing::OutPacketTypes::eSetBlock;
    ptr->X = x;
    ptr->Y = y;
    ptr->Z = z;
    ptr->BlockType = block;

    serv->broadcast_mutex.lock();
    serv->broadcast_list.push_back(Outgoing::createOutgoingPacket(ptr.get()));
    serv->broadcast_mutex.unlock();
}
auto World::rtick() -> void {
    for (int cX = 0; cX < 16; cX++)
        for (int cY = 0; cY < 4; cY++)
            for (int cZ = 0; cZ < 16; cZ++) {
                int x = rand() % 16 + cX * 16;
                int y = rand() % 16 + cY * 16;
                int z = rand() % 16 + cZ * 16;

                bool is_dark = false;

                for (int i = 63; i > y; i--) {
                    auto idx = getIdx(x, i, z);
                    auto blk = worldData[idx];

                    is_dark =
                        blk != Block::Air && blk != Block::Flower1 &&
                        blk != Block::Flower2 && blk != Block::Mushroom1 &&
                        blk != Block::Mushroom2 && blk != Block::Sapling &&
                        blk != Block::Glass && blk != Block::Leaves;

                    if (is_dark)
                        break;
                }
                auto idx = getIdx(x, y, z);

                y += 1;
                if (y >= 64)
                    return;

                auto idx2 = (x * 256 * 64) + (z * 64) + y;
                auto blk2 = Block::Air;
                if (idx2 >= 0 && idx2 < (256 * 64 * 256))
                    blk2 = worldData[idx2];

                auto blk = Block::Air;
                if (idx >= 0 && idx2 < (256 * 64 * 256))
                    blk = worldData[idx];

                auto blk2_is_valid_grass =
                    (blk2 == Block::Air || blk2 == Block::Sapling ||
                     blk2 == Block::Flower1 || blk2 == Block::Flower2 ||
                     blk2 == Block::Mushroom1 || blk2 == Block::Mushroom2 ||
                     blk2 == Block::Leaves);

                if (blk == Block::Grass && (!blk2_is_valid_grass || is_dark)) {
                    setBlock(x, y, z, Block::Dirt);
                }

                if (blk == Block::Dirt && blk2_is_valid_grass && !is_dark) {
                    setBlock(x, y, z, Block::Grass);
                }

                if (blk == Block::Sapling) {
                    if (is_dark) {
                        setBlock(x, y, z, Block::Air);
                    } else {
                        WorldGenUtil::make_tree2(this, x, z, y - 1);
                    }
                }

                if (blk == Block::Flower1 || blk == Block::Flower2) {
                    if (is_dark) {
                        setBlock(x, y, z, Block::Air);
                    }
                }

                if (blk == Block::Mushroom1 || blk == Block::Mushroom2) {
                    int idxl = ((x)*256 * 4) + ((z * 16) * 4) + cY;

                    if (!is_dark) {
                        setBlock(x, y, z, Block::Air);
                    }
                }
            }
}

} // namespace ClassicServer
