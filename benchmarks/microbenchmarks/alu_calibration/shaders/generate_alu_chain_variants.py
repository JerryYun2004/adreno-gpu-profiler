from pathlib import Path

out_dir = Path(".")
variants = {
    64: 16,    # 16 ALU_STEP blocks × 4 ops = 64 high-level ALU ops
    128: 32,   # 32 blocks × 4 ops = 128 ops
    256: 64,   # 64 blocks × 4 ops = 256 ops
    512: 128,  # 128 blocks × 4 ops = 512 ops
}

# Fixed pseudo-random constants. Reused cyclically if needed.
consts = [
    0x9e3779b9, 0x85ebca6b, 0xc2b2ae35, 0x27d4eb2f,
    0x165667b1, 0xd3a2646c, 0xfd7046c5, 0xb55a4f09,
    0x243f6a88, 0x85a308d3, 0x13198a2e, 0x03707344,
    0xa4093822, 0x299f31d0, 0x082efa98, 0xec4e6c89,
    0x452821e6, 0x38d01377, 0xbe5466cf, 0x34e90c6c,
    0xc0ac29b7, 0xc97c50dd, 0x3f84d5b5, 0xb5470917,
    0x9216d5d9, 0x8979fb1b, 0xd1310ba6, 0x98dfb5ac,
    0x2ffd72db, 0xd01adfb7, 0xb8e1afed, 0x6a267e96,
    0xba7c9045, 0xf12c7f99, 0x24a19947, 0xb3916cf7,
    0x0801f2e2, 0x858efc16, 0x636920d8, 0x71574e69,
    0xa458fea3, 0xf4933d7e, 0x0d95748f, 0x728eb658,
    0x718bcd58, 0x82154aee, 0x7b54a41d, 0xc25a59b5,
    0x9c30d539, 0x2af26013, 0xc5d1b023, 0x286085f0,
    0xca417918, 0xb8db38ef, 0x8e79dcb0, 0x603a180e,
    0x6c9e0e8b, 0xb01e8a3e, 0xd71577c1, 0xbd314b27,
    0x78af2fda, 0x55605c60, 0xe65525f3, 0xaa55aa55,
]

template_head = """#version 450

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, std430) readonly buffer InputBuffer {
    uint in_data[];
};

layout(set = 0, binding = 1, std430) writeonly buffer OutputBuffer {
    uint out_data[];
};

#define ALU_STEP(x, c)       \\
    x = x + c;               \\
    x = x ^ (x << 13);        \\
    x = x + (x >> 17);        \\
    x = x ^ (x << 5);

void main()
{
    uint gid = gl_GlobalInvocationID.x;
    uint x = in_data[gid];

"""

template_tail = """
    out_data[gid] = x;
}
"""

for alu_ops, blocks in variants.items():
    lines = [template_head]
    for i in range(blocks):
        c = consts[i % len(consts)]
        # For blocks beyond the first 64, slightly perturb the constant
        # so the 512-op version is not just an identical repeated text block.
        if i >= len(consts):
            c = (c ^ ((i * 0x45d9f3b) & 0xffffffff)) & 0xffffffff
        lines.append(f"    ALU_STEP(x, 0x{c:08x}u)\n")
    lines.append(template_tail)

    path = out_dir / f"alu_chain_{alu_ops}.comp"
    path.write_text("".join(lines))
    print(f"wrote {path} with {blocks} ALU_STEP blocks = {alu_ops} high-level ALU ops")
