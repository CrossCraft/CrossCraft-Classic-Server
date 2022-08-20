#include "World.hpp"
#include "../OutgoingPackets.hpp"
#include "../Utility.hpp"
#include "Generation/NoiseUtil.hpp"
#include "Generation/WorldGenUtil.hpp"
#include <Platform/Platform.hpp>
#include <Rendering/Rendering.hpp>
#include <Utilities/Input.hpp>
#include <gtx/rotate_vector.hpp>
#include <iostream>
#include <random>
#include <zlib.h>

namespace ClassicServer {

inline uint32_t HostToNetwork4(const void *a_Value) {
    uint32_t buf;
    memcpy(&buf, a_Value, sizeof(buf));
    buf = ntohl(buf);
    return buf;
}

auto World::auto_save(World *wrld) -> void {
    while (!wrld->serv->stopping) {
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

glm::vec3 world_size;
auto World::load_world() -> bool {
    gzFile save_file = gzopen("save.ccc", "rb");
    gzrewind(save_file);

    int version = 0;
    gzread(save_file, &version, sizeof(int) * 1);

    if (version == 1) {
        uint8_t* temp = (uint8_t*)malloc(256 * 64 * 256);

        gzread(save_file, temp, 256 * 64 * 256);
        gzclose(save_file);

        for (auto x = 0; x < 256; x++)
            for (auto y = 0; y < 64; y++)
                for (auto z = 0; z < 256; z++) {
                    auto idx_destiny = (y * 256 * 256) + (z * 256) + x + 4;

                    auto idx_source = (x * 256 * 64) + (z * 64) + y;
                    worldData[idx_destiny] = temp[idx_source];
                }

        free(temp);
    }
    else if (version == 3)
    {
        gzread(save_file, &world_size, sizeof(world_size));

        worldData = (uint8_t*)realloc(
            worldData,
            world_size.x * world_size.y * world_size.z + 4);

        gzread(save_file, worldData + 4,
            world_size.x * world_size.y * world_size.z);
        gzclose(save_file);

        (*(int*)&worldData[0]) = world_size.x * world_size.y * world_size.z;
    }
    return true;
}

auto World::save() -> void {
    gzFile save_file = gzopen("save.ccc", "wb");

    if (save_file != nullptr) {
        const int save_version = 3;

        gzwrite(save_file, &world_size, sizeof(world_size));
        gzwrite(save_file, &save_version, 1 * sizeof(int));
        gzwrite(save_file, worldData + 4, world_size.x * world_size.y * world_size.z);
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

auto is_valid(glm::ivec3 ivec) -> bool {
    return ivec.x >= 0 && ivec.x < 256 && ivec.y >= 0 && ivec.y < 256 &&
           ivec.z >= 0 && ivec.z < 256;
}

auto World::blockUpdate() -> void {
    std::vector<glm::ivec3> newV;

    for (auto &v : posUpdate) {
        newV.push_back(v);
    }

    posUpdate.clear();

    for (auto &pos : newV) {
        auto blk = worldData[getIdx(pos.x, pos.y, pos.z)];

        if (blk == Block::Water) {
            update_check(this, blk, {pos.x, pos.y - 1, pos.z});
            update_check(this, blk, {pos.x - 1, pos.y, pos.z});
            update_check(this, blk, {pos.x + 1, pos.y, pos.z});
            update_check(this, blk, {pos.x, pos.y, pos.z + 1});
            update_check(this, blk, {pos.x, pos.y, pos.z - 1});
        } else if (blk == Block::Sapling || blk == Block::Flower1 ||
                   blk == Block::Flower2 || blk == Block::Mushroom1 ||
                   blk == Block::Mushroom2) {
            update_check(this, blk, {pos.x, pos.y - 1, pos.z});
        } else if (blk == Block::Sand || blk == Block::Gravel) {
            update_check(this, blk, {pos.x, pos.y - 1, pos.z});
        }
    }

    for (auto &v : updated) {
        update_nearby_blocks(v);
    }

    updated.clear();
}

void World::update(double dt) {
    // We are ticking 20 times per second. Rtick = 4 times.
    rtick_count++;
    if (rtick_count == 5) {
        rtick_count = 0;
        for (int i = 0; i < 16 * 16 * 4; i++)
            rtick();
    }

    blockUpdate();
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

std::random_device dev;
std::mt19937 rng(dev());
std::uniform_int_distribution<std::mt19937::result_type> coord(0, 255);

auto World::rtick() -> void {
    unsigned int x = coord(rng);
    unsigned int y = coord(rng) % 64;
    unsigned int z = coord(rng);
    auto idx = getIdx(x, y, z);
    auto blk = worldData[idx];

    if (blk == Block::Sapling) {
        SC_APP_INFO("TREE!");
        WorldGenUtil::make_tree2(this, x, z, y - 1);
    }

    bool is_dark = false;
    for (int i = 63; i > y; i--) {
        auto blk2 = worldData[getIdx(x, i, z)];

        if (blk2 != Block::Air && blk2 != Block::Sapling &&
            blk2 != Block::Flower1 && blk2 != Block::Flower2 &&
            blk2 != Block::Mushroom1 && blk2 != Block::Mushroom2 &&
            blk2 != Block::Leaves) {
            is_dark = true;
            break;
        }
    }

    if (blk == Block::Flower1 || blk == Block::Flower2) {
        if (is_dark) {
            setBlock(x, y, z, Block::Air);
        }
        return;
    }

    if (blk == Block::Mushroom1 || blk == Block::Mushroom2) {
        if (!is_dark) {
            setBlock(x, y, z, Block::Air);
        }
        return;
    }

    if ((y + 1) >= 64)
        return;

    auto blk2 = worldData[getIdx(x, y + 1, z)];
    auto blk2_is_valid_grass =
        (blk2 == Block::Air || blk2 == Block::Sapling ||
         blk2 == Block::Flower1 || blk2 == Block::Flower2 ||
         blk2 == Block::Mushroom1 || blk2 == Block::Mushroom2 ||
         blk2 == Block::Leaves);

    if (blk == Block::Dirt && blk2_is_valid_grass) {
        setBlock(x, y, z, Block::Grass);
        return;
    }

    if (blk == Block::Grass && !blk2_is_valid_grass) {
        setBlock(x, y, z, Block::Dirt);
        return;
    }
}

auto World::update_check(World *wrld, int blkr, glm::ivec3 chk) -> void {
    if (is_valid(chk)) {
        auto blk = wrld->worldData[getIdx(chk.x, chk.y, chk.z)];
        if (blk == Block::Air) {

            if (blkr == Block::Water) {
                setBlock(chk.x, chk.y, chk.z, Block::Water);
                updated.push_back(chk);
            } else if (blkr == Block::Sapling || blkr == Block::Flower1 ||
                       blkr == Block::Flower2 || blkr == Block::Mushroom1 ||
                       blkr == Block::Mushroom2) {
                setBlock(chk.x, chk.y + 1, chk.z, 0);
                updated.push_back(chk);
            } else if (blkr == Block::Gravel || blkr == Block::Sand) {
                setBlock(chk.x, chk.y + 1, chk.z, Block::Air);
                setBlock(chk.x, chk.y, chk.z, blkr);

                update_nearby_blocks({chk.x, chk.y + 1, chk.z});
                updated.push_back(chk);
            }
        }
        else if (blk == Block::Still_Lava || blk == Block::Lava || blk == Block::Water || blk == Block::Still_Water) {
            if (blkr == Block::Gravel || blkr == Block::Sand) {
                setBlock(chk.x, chk.y + 1, chk.z, Block::Air);
                setBlock(chk.x, chk.y, chk.z, blkr);

                update_nearby_blocks({ chk.x, chk.y + 1, chk.z });
                updated.push_back(chk);
            }
        }
    }
}

auto World::add_update(glm::ivec3 ivec) -> void {
    if (ivec.x >= 0 && ivec.x < 256 && ivec.y >= 0 && ivec.y < 256 &&
        ivec.z >= 0 && ivec.z < 256)
        posUpdate.push_back(ivec);
}

auto World::update_nearby_blocks(glm::ivec3 ivec) -> void {
    add_update({ivec.x, ivec.y, ivec.z});
    add_update({ivec.x, ivec.y + 1, ivec.z});
    add_update({ivec.x, ivec.y - 1, ivec.z});
    add_update({ivec.x - 1, ivec.y, ivec.z});
    add_update({ivec.x + 1, ivec.y, ivec.z});
    add_update({ivec.x, ivec.y, ivec.z + 1});
    add_update({ivec.x, ivec.y, ivec.z - 1});
}

} // namespace ClassicServer
