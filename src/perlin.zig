pub fn noise(x: f64, y: f64, z: f64) f64 {
    const x_int = @intCast(usize, @floatToInt(isize, @floor(x)) & 255);
    const y_int = @intCast(usize, @floatToInt(isize, @floor(y)) & 255);
    const z_int = @intCast(usize, @floatToInt(isize, @floor(z)) & 255);

    const x_float = x - @floor(x);
    const y_float = y - @floor(y);
    const z_float = z - @floor(z);

    const u = fade(x_float);
    const v = fade(y_float);
    const w = fade(z_float);

    const a  = permutation[x_int]   + y_int;
    const aa = permutation[a]       + z_int;
    const ab = permutation[a+1]     + z_int;
    const b  = permutation[x_int+1] + y_int;
    const ba = permutation[b]       + z_int;
    const bb = permutation[b+1]     + z_int;

    return lerp(w, lerp(v, lerp(u, grad(permutation[aa], x_float, y_float, z_float),
        grad(permutation[ba], x_float-1, y_float, z_float)),
        lerp(u, grad(permutation[ab], x_float, y_float-1, z_float),
            grad(permutation[bb], x_float-1, y_float-1, z_float))),
        lerp(v, lerp(u, grad(permutation[aa+1], x_float, y_float, z_float-1),
            grad(permutation[ba+1], x_float-1, y_float, z_float-1)),
                lerp(u, grad(permutation[ab+1], x_float, y_float-1, z_float-1),
                     grad(permutation[bb+1], x_float-1, y_float-1, z_float-1))));
}

pub fn pnoise(x: f64, y: f64, seed: u32) f64 {
    return noise(x, y, @intToFloat(f64, seed));
}

pub fn onoise(octaves: usize, x: f64, y: f64, seed: u32) f64 {
    var sum : f64 = 0;
    var amp : f64 = 1;
    var freq : f64 = 1.3;

    var i : usize = 0;
    while(i < octaves) : (i += 1) {
        sum += pnoise(x * freq, y * freq, seed) * amp;

        amp *= 2;
        freq /= 2;
    }
    return sum;
}

fn lerp(t: f64, a: f64, b: f64) f64 {
    return a + t * (b - a);
}

fn fade(t: f64) f64 {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

export fn grad(h: usize, x: f64, y: f64, z: f64) f64 {
    switch (h & 15) {
        0, 12 => { return  x + y; },
        1, 14 => { return  y - x; },
        2     => { return  x - y; },
        3     => { return -x - y; },
        4     => { return  x + z; },
        5     => { return  z - x; },
        6     => { return  x - z; },
        7     => { return -x - z; },
        8     => { return  y + z; },
        9, 13 => { return  z - y; },
        10    => { return  y - z; },
        else  => { return -y - z; }, // 11, 16
    }
}


const permutation = [_]usize{
    151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225,
    140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,  190, 6,   148,
    247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,
    57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136, 171, 168, 68,  175,
    74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158, 231, 83,  111, 229, 122,
    60,  211, 133, 230, 220, 105, 92,  41,  55,  46,  245, 40,  244, 102, 143, 54,
    65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,  169,
    200, 196, 135, 130, 116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,
    52,  217, 226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212,
    207, 206, 59,  227, 47,  16,  58,  17,  182, 189, 28,  42,  223, 183, 170, 213,
    119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,
    129, 22,  39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104,
    218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241,
    81,  51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157,
    184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,
    222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180,

    151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225,
    140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,  190, 6,   148,
    247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,
    57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136, 171, 168, 68,  175,
    74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158, 231, 83,  111, 229, 122,
    60,  211, 133, 230, 220, 105, 92,  41,  55,  46,  245, 40,  244, 102, 143, 54,
    65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,  169,
    200, 196, 135, 130, 116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,
    52,  217, 226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212,
    207, 206, 59,  227, 47,  16,  58,  17,  182, 189, 28,  42,  223, 183, 170, 213,
    119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,
    129, 22,  39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104,
    218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241,
    81,  51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157,
    184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,
    222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180,
};