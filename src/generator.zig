const std = @import("std");
const perlin = @import("perlin.zig");
const world = @import("world.zig");

var heightMap : [256 * 256]i32 = undefined;
const waterLevel : i32 = 32;

var map_seed: u32 = 0;

fn noise1(x: f32, y: f32) f64 {
    var n1 = perlin.onoise(8, x, y, map_seed + 1);
    return perlin.onoise(8, x + n1, y, map_seed + 2);
}

fn noise2(x: f32, y: f32) f64 {
    var n1 = perlin.onoise(8, x, y, map_seed + 3);
    return perlin.onoise(8, x + n1, y, map_seed + 4);
}

const RndGen = std.rand.DefaultPrng;
var rnd : RndGen = undefined;
    
pub fn generate(seed: u32) void {
    map_seed = seed;

    rnd = RndGen.init(seed);

    //"Raising..."
    generate_heightmap();
    //"Eroding..."
    smooth_heightmap();
    //"Soiling..."
    create_strata();

    //"Carving..."
    make_caves();
    // create ore veins
    make_ores();

    //"Watering..."
    fill_water();

    //"Melting..."
    fill_lava();

    //"Growing..."
    create_surface();

    //"Planting..."
    create_plants();
}

fn generate_heightmap() void {

    var z : usize = 0;
    while(z < 256) : (z += 1) {

    var x : usize = 0;
    while(x < 256) : (x += 1) {

    var xf = @intToFloat(f32, x);
    var zf = @intToFloat(f32, z);

    var heightLow = noise1(xf * 1.3, zf * 1.3) / 6.0 - 4.0;
    var heightHigh = noise2(xf * 1.3, zf * 1.3) / 5.0 + 6.0;

    var heightResult : f64 = 0.0;

    if(perlin.onoise(6, xf, zf, map_seed + 5) / 8.0 > 0) {
        heightResult = heightLow;
    } else {
        heightResult = if (heightHigh > heightLow) heightHigh else heightLow;
    }

    heightResult /= 2.0;    

    if(heightResult < 0) 
        heightResult *= 0.8;

    heightMap[x + z * 256] = waterLevel + @floatToInt(i32, heightResult);
    }
    }
}

fn smooth_heightmap() void {
    var z : usize = 0;
    while(z < 256) : (z += 1) {

    var x : usize = 0;
    while(x < 256) : (x += 1) {

    var xf = @intToFloat(f32, x);
    var zf = @intToFloat(f32, z);

    var a = noise1(xf * 2, zf * 2) / 8.0;
    var b : i32 = if(noise2(xf * 2, zf * 2) > 0) 1 else 0;

    if(a > 2) {
        heightMap[x + z * 256] = @floatToInt(i32, @intToFloat(f32, heightMap[x + z * 256] - b) / 2.0) * 2 + b;
    }

    }
    }
}

fn create_strata() void {
    
    var z : usize = 0;
    while(z < 256) : (z += 1) {

    var x : usize = 0;
    while(x < 256) : (x += 1) {

    var dirtThickness = perlin.onoise(8, @intToFloat(f32, x), @intToFloat(f32, z), map_seed + 1) / 24.0 - 4.0;
    var dirtTransition = heightMap[x + z * 256];
    var stoneTransition = dirtTransition + @floatToInt(i32, dirtThickness);

    var y : usize = 0;
    while(y < 64) : (y += 1) {
        var bType : u8 = 0;

        if(y == 0) {
            bType = 10; // LAVA
        } else if(y <= stoneTransition) {
            bType = 1; // STONE
        } else if(y <= dirtTransition) {
            bType = 3; // DIRT
        }

        world.worldData[world.getIdx(@intCast(u32, x), @intCast(u32, y), @intCast(u32, z))] = bType;
    }

    }
    }
}

fn fill_water() void {
    
    var z : usize = 0;
    while(z < 256) : (z += 1) {

    var x : usize = 0;
    while(x < 256) : (x += 1) {

    var y : usize = @intCast(usize, heightMap[x + z * 256]);
    while(y < waterLevel) : (y += 1) {
        var idx = world.getIdx(@intCast(u32, x), @intCast(u32, y), @intCast(u32, z));
        var blk = world.worldData[idx];

        if(blk == 0) {
            world.worldData[idx] = 8;
        }
    }

    }
    }
}

fn fill_lava() void {
    var z : usize = 0;
    while(z < 256) : (z += 1) {

    var x : usize = 0;
    while(x < 256) : (x += 1) {

    var y : usize = 0;
    while(y < 10) : (y += 1) {
        var idx = world.getIdx(@intCast(u32, x), @intCast(u32, y), @intCast(u32, z));
        var blk = world.worldData[idx];

        if(blk == 0) {
            world.worldData[idx] = 8;
        }
    }

    }
    }
}

fn create_surface() void {
    
    var z : usize = 0;
    while(z < 256) : (z += 1) {

    var x : usize = 0;
    while(x < 256) : (x += 1) {

        var xf = @intToFloat(f32, x);
        var zf = @intToFloat(f32, z);

        var is_sand = perlin.onoise(8, xf, zf, map_seed + 1) > 8.0;
        var is_gravel = perlin.onoise(8, xf, zf, map_seed + 2) > 12.0;

        var y = heightMap[x + z * 256];

        var blkA = world.worldData[world.getIdx(@intCast(u32, x), @intCast(u32, y + 1), @intCast(u32, z))];

        var idx = world.getIdx(@intCast(u32, x), @intCast(u32, y), @intCast(u32, z));

        if(blkA == 8 and is_gravel) {
            world.worldData[idx] = 13;
        } 

        if(blkA == 0) {
            if(y <= waterLevel and is_sand) {
                world.worldData[idx] = 12;
            } else {
                world.worldData[idx] = 2;
            }
        }

    }
    }
}

fn fillOblateSpheroid(cx: i32, cy: i32, cz: i32, radius: f32, blkID: u8) void {
    var r = @floatToInt(isize, radius);
    var z: isize = cz - r;
    while(z < cz + r) : (z += 1) {
    var y: isize = cy - r;
    while(y < cy + r) : (y += 1) {
    var x: isize = cx - r;
    while(x < cx + r) : (x += 1) {

        var dx = x - cx;
        var dy = y - cy;
        var dz = z - cz;

        if(dx * dx + 2 * dy * dy + dz * dz < r * r) {
            if(x >= 0 and x < 256 and y >= 0 and y < 64 and z >= 0 and z < 256) {
                var idx = world.getIdx(@intCast(u32, x), @intCast(u32, y), @intCast(u32, z));
                var blk = world.worldData[idx];

                if(blk == 1) {
                    world.worldData[idx] = blkID;
                }
            }
        }

    }
    }
    }
}

fn create_vein(abundance: f32, oreBlockID: u8) void {
    var vx = @intToFloat(f32, rnd.random().int(u32) % 256);
    var vy = @intToFloat(f32, rnd.random().int(u32) % 64);
    var vz = @intToFloat(f32, rnd.random().int(u32) % 256);

    var vl = rnd.random().float(f32) * rnd.random().float(f32) * 150 * abundance;

    var theta = rnd.random().float(f32) * 3.14159 * 2.0;
    var deltaTheta : f32 = 0;

    var phi = rnd.random().float(f32) * 3.14159 * 2.0;
    var deltaPhi : f32 = 0;

    var len : usize = 0;
    while(len < @floatToInt(usize, vl)) : (len += 1) {
        vx += std.math.sin(theta) * std.math.cos(phi);
        vy += std.math.cos(theta) * std.math.cos(phi);
        vz += std.math.sin(phi);

        theta = deltaTheta * 0.2;
        deltaTheta *= 0.9;
        deltaTheta += rnd.random().float(f32) - rnd.random().float(f32);

        phi /= 2.0;
        phi += deltaPhi / 4.0;

        // unlike deltaPhi for caves, previous value is multiplied by 0.9 instead of 0.75
        deltaPhi *= 0.9;
        deltaPhi += rnd.random().float(f32) - rnd.random().float(f32);

        var radius = abundance * std.math.sin(@intToFloat(f32,len) * 3.14159 / vl) + 1;

        fillOblateSpheroid(@floatToInt(i32, vx), @floatToInt(i32, vy), @floatToInt(i32, vz), radius, oreBlockID);
    }
}

fn make_ores() void {
    var numCoal = @floatToInt(usize, 256.0 * 64.0 * 256.0 * 0.9 / 16384.0);
    var numIron = @floatToInt(usize, 256.0 * 64.0 * 256.0 * 0.7 / 16384.0);
    var numGold = @floatToInt(usize, 256.0 * 64.0 * 256.0 * 0.5 / 16384.0);

    var i: usize = 0;
    while(i < numCoal) : (i += 1) {
        create_vein(0.9, 16);
    }

    i = 0;
    while(i < numIron) : (i += 1) {
        create_vein(0.7, 15);
    }

    i = 0;
    while(i < numGold) : (i += 1) {
        create_vein(0.5, 14);
    }
}

fn create_cave() void {
    var vx = @intToFloat(f32, rnd.random().int(u32) % 256);
    var vy = @intToFloat(f32, rnd.random().int(u32) % 64);
    var vz = @intToFloat(f32, rnd.random().int(u32) % 256);

    var vl = std.math.floor(rnd.random().float(f32) + rnd.random().float(f32)) * 200;

    var theta = rnd.random().float(f32) * 3.14159 * 2.0;
    var deltaTheta : f32 = 0;

    var phi = rnd.random().float(f32) * 3.14159 * 2.0;
    var deltaPhi : f32 = 0;

    var caveRadius = rnd.random().float(f32) * rnd.random().float(f32) * 2.0;

    var len : usize = 0;
    while(len < @floatToInt(usize, vl)) : (len += 1) {
        vx += std.math.sin(theta) * std.math.cos(phi);
        vy += std.math.cos(theta) * std.math.cos(phi);
        vz += std.math.sin(phi);

        theta += deltaTheta * 0.2;
        deltaTheta *= 0.9;
        deltaTheta += rnd.random().float(f32) - rnd.random().float(f32);

        phi /= 2.0;
        phi += deltaPhi / 4.0;

        deltaPhi *= 0.75;
        deltaPhi += rnd.random().float(f32) - rnd.random().float(f32);

        if(rnd.random().float(f32) >= 0.25) {
            var cx = vx + @intToFloat(f32, @mod(rnd.random().int(i32), 4) - 2) * 0.2;
            var cy = vy + @intToFloat(f32, @mod(rnd.random().int(i32), 4) - 2) * 0.2;
            var cz = vz + @intToFloat(f32, @mod(rnd.random().int(i32), 4) - 2) * 0.2;

            var radius = (64 - cy) / 64;
            radius = 1.2 + (radius * 3.5 + 1) * caveRadius;
            radius = radius * std.math.sin(@intToFloat(f32, len) * 3.14159 / vl);

            fillOblateSpheroid(@floatToInt(i32, cx), @floatToInt(i32, cy), @floatToInt(i32, cz), radius, 0);
        }

    }
}

fn make_caves() void {
    var numCaves : usize = 256 * 64 * 256 / 8192;

    var i : usize = 0;
    while(i < numCaves) : (i += 1) {
        create_cave();
    }
}

fn create_flowers() void {
    var flowerType = @mod(rnd.random().int(u8), 2) + 37;
    var px = @mod(rnd.random().int(usize), 256);
    var pz = @mod(rnd.random().int(usize), 256);

    var i : usize = 0;
    while(i < 10) : (i += 1) {
        
    var fx = px;
    var fz = pz;

    var c: usize = 0;
    while(c < 5) : (c += 1) {
        fx += @mod(rnd.random().int(usize), 6);

        var sub = @mod(rnd.random().int(usize), 6);
        if(fx > sub)
            fx -= sub;
            
        sub = @mod(rnd.random().int(usize), 6);
        fz += @mod(rnd.random().int(usize), 6);
        if(fz > sub)
            fz -= sub;

        if(fx >= 0 and fx < 256 and fz >= 0 and fz < 256) {
            var fy = heightMap[@intCast(usize, fx + fz * 256)] + 1;

            var blk = world.worldData[world.getIdx(@intCast(u32, fx), @intCast(u32, fy), @intCast(u32, fz))];
            var blkB = world.worldData[world.getIdx(@intCast(u32, fx), @intCast(u32, fy) - 1, @intCast(u32, fz))];

            if(blk == 0 and blkB == 2) {
                world.worldData[world.getIdx(@intCast(u32, fx), @intCast(u32, fy), @intCast(u32, fz))] = flowerType;
            }
        }
    }
    }

}

fn create_mushrooms() void {
    var mushType = @mod(rnd.random().int(u8), 2) + 39;
    var px = @mod(rnd.random().int(usize), 256);
    var py = @mod(rnd.random().int(usize), 64);
    var pz = @mod(rnd.random().int(usize), 256);

    var i : usize = 0;
    while(i < 20) : (i += 1) {
        
    var fx = px;
    var fy = py;
    var fz = pz;

    var c: usize = 0;
    while(c < 5) : (c += 1) {
        fx += @mod(rnd.random().int(usize), 6);

        var sub = @mod(rnd.random().int(usize), 6);
        if(fx > sub)
            fx -= sub;
            
        sub = @mod(rnd.random().int(usize), 6);
        fz += @mod(rnd.random().int(usize), 6);
        if(fz > sub)
            fz -= sub;
            
        sub = @mod(rnd.random().int(usize), 2);
        fy += @mod(rnd.random().int(usize), 2);
        if(fy > sub)
            fy -= sub;

        if(fx >= 0 and fx < 256 and fz >= 0 and fz < 256 and fy > 0 and fy < 64) {

            var blk = world.worldData[world.getIdx(@intCast(u32, fx), @intCast(u32, fy), @intCast(u32, fz))];

            var blkB = world.worldData[world.getIdx(@intCast(u32, fx), @intCast(u32, fy) - 1, @intCast(u32, fz))];

            if(blk == 0 and blkB == 1) {
                world.worldData[world.getIdx(@intCast(u32, fx), @intCast(u32, fy), @intCast(u32, fz))] = mushType;
            }
        }
    }
    }

}

pub fn growTree(x: u32, y: u32, z: u32, h: u32) void {
    if(x > 2 and z > 2 and y > 0 and y < 50 and x < 254 and z < 254){
    if(isSpaceForTree(x, y, z)){
    const max = y + h;

    var m : u32 = max;
    while(m >= y) : (m -= 1) {
        if(m == max) {
            world.worldData[world.getIdx(x - 1, m, z)] = 18;
            world.worldData[world.getIdx(x + 1, m, z)] = 18;
            world.worldData[world.getIdx(x, m, z - 1)] = 18;
            world.worldData[world.getIdx(x, m, z + 1)] = 18;
            world.worldData[world.getIdx(x, m, z)] = 18;
        } else if(m == max - 1) {
            world.worldData[world.getIdx(x - 1, m, z)] = 18;
            world.worldData[world.getIdx(x + 1, m, z)] = 18;
            world.worldData[world.getIdx(x, m, z - 1)] = 18;
            world.worldData[world.getIdx(x, m, z + 1)] = 18;

            if(@mod(rnd.random().int(usize), 2) == 0) 
                world.worldData[world.getIdx(x - 1, m, z - 1)] = 18;
                
            if(@mod(rnd.random().int(usize), 2) == 0) 
                world.worldData[world.getIdx(x + 1, m, z - 1)] = 18;
            
            if(@mod(rnd.random().int(usize), 2) == 0) 
                world.worldData[world.getIdx(x - 1, m, z + 1)] = 18;
            
            if(@mod(rnd.random().int(usize), 2) == 0) 
                world.worldData[world.getIdx(x + 1, m, z + 1)] = 18;

            world.worldData[world.getIdx(x, m, z)] = 17;

        } else if(m == max - 2 or m == max - 3) {
            world.worldData[world.getIdx(x - 1, m, z)] = 18;
            world.worldData[world.getIdx(x + 1, m, z)] = 18;
            world.worldData[world.getIdx(x, m, z - 1)] = 18;
            world.worldData[world.getIdx(x, m, z + 1)] = 18;
            world.worldData[world.getIdx(x - 1, m, z - 1)] = 18;
            world.worldData[world.getIdx(x + 1, m, z - 1)] = 18;
            world.worldData[world.getIdx(x - 1, m, z + 1)] = 18;
            world.worldData[world.getIdx(x + 1, m, z + 1)] = 18;
            
            world.worldData[world.getIdx(x - 2, m, z - 1)] = 18;
            world.worldData[world.getIdx(x - 2, m, z)] = 18;
            world.worldData[world.getIdx(x - 2, m, z + 1)] = 18;
            
            world.worldData[world.getIdx(x + 2, m, z - 1)] = 18;
            world.worldData[world.getIdx(x + 2, m, z)] = 18;
            world.worldData[world.getIdx(x + 2, m, z + 1)] = 18;

            world.worldData[world.getIdx(x - 1, m, z + 2)] = 18;
            world.worldData[world.getIdx(x, m, z + 2)] = 18;
            world.worldData[world.getIdx(x + 1, m, z + 2)] = 18;

            world.worldData[world.getIdx(x - 1, m, z - 2)] = 18;
            world.worldData[world.getIdx(x, m, z - 2)] = 18;
            world.worldData[world.getIdx(x + 1, m, z - 2)] = 18;

            
            if(@mod(rnd.random().int(usize), 2) == 0) 
                world.worldData[world.getIdx(x - 2, m, z - 2)] = 18;
                
            if(@mod(rnd.random().int(usize), 2) == 0) 
                world.worldData[world.getIdx(x + 2, m, z - 2)] = 18;
            
            if(@mod(rnd.random().int(usize), 2) == 0) 
                world.worldData[world.getIdx(x - 2, m, z + 2)] = 18;
            
            if(@mod(rnd.random().int(usize), 2) == 0) 
                world.worldData[world.getIdx(x + 2, m, z + 2)] = 18;

            world.worldData[world.getIdx(x, m, z)] = 17;

        } else {
            world.worldData[world.getIdx(x, m, z)] = 17;
        }
    }
    }
    }
}

fn isSpaceForTree(x: u32, y: u32, z: u32) bool {
    var i : u32 = 0;
    while(i < 6) : (i += 1) {
        var cx = x - 2;
        while(cx <= x + 2) : (cx += 1) {
            var cz = z - 2;
            while(cz <= z + 2) : (cz += 1) {
                var idx = world.getIdx(cx, y + i, cz);
                var blk = world.worldData[idx];

                if(blk != 0)
                    return false;
            }
        }
    }

    return true;
}

fn create_tree() void {
    var px = @mod(rnd.random().int(usize), 256);
    var pz = @mod(rnd.random().int(usize), 256);

    var i : usize = 0;
    while(i < 20) : (i += 1) {
        var tx = px;
        var tz = pz;

        var c: usize = 0; 
        while(c < 20) : (c += 1) {
            tx += @mod(rnd.random().int(usize), 6);

            var sub = @mod(rnd.random().int(usize), 6);
            if(tx > sub)
                tx -= sub;
            
            sub = @mod(rnd.random().int(usize), 6);
            tz += @mod(rnd.random().int(usize), 6);
            if(tz > sub)
                tz -= sub;
            
            if(tx >= 0 and tx < 256 and tz >= 0 and tz < 256 and rnd.random().float(f32) <= 0.25) {
                var ty = heightMap[@intCast(usize, tx + tz * 256)] + 1;

                var th = 4 + @mod(rnd.random().int(usize), 3);

                growTree(@intCast(u32, tx), @intCast(u32, ty), @intCast(u32, tz), @intCast(u32, th)); 
            }
        }
    }
}

fn create_plants() void {
    const numFlowers = 256 * 256 / 3000;

    var i : usize = 0;
    while(i < numFlowers) : (i += 1) {
        create_flowers();
    }
    
    const numMushrooms = 256 * 256 * 64 / 2000;

    i = 0;
    while(i < numMushrooms) : (i += 1) {
        create_mushrooms();
    }
    
    const numTrees = 256 * 256 / 4000;

    i = 0;
    while(i < numTrees) : (i += 1) {
        create_tree();
    }
}