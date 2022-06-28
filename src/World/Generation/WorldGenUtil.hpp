#include "../World.hpp"

namespace ClassicServer {
class World;
struct WorldGenUtil {
    static auto make_tree(World *wrld, int x, int z, int h) -> void;
    static auto make_tree2(World *wrld, int x, int z, int h) -> void;
};
} // namespace ClassicServer