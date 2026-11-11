#version 460
// ssrecomp: the widescreen view's polygons on the graphics chip (GPU3D_WideGpu.h).
// Vertices are read from a buffer (no vertex attributes to set up, the same in OpenGL and
// deko3D): each holds its place in output pixels, the polygon's W (for perspective-correct
// colours and texture coordinates), its depth, colour and texture coordinates.

struct WideVertex
{
    float x, y, w, z;
    float r, g, b, s;
    float t;
    uint poly;
    uint pad0, pad1;
};

#ifdef WIDE_ES
// OpenGL ES on a phone: its graphics chips give a vertex shader no storage buffers (Mali: 0),
// so there the vertices come in as attributes, from the same memory.
layout (location = 0) in vec4 aPos;   // x, y, w, z
layout (location = 1) in vec4 aColS;  // r, g, b, s
layout (location = 2) in float aT;
layout (location = 3) in uint aPoly;
#else
layout (std430, binding = 0) readonly buffer Vertices
{
    WideVertex vtx[];
};
#endif

struct WidePolygon
{
    uint attr, texParam, texPalette, flags;
};

#ifndef WIDE_ES
layout (std430, binding = 1) readonly buffer Polygons
{
    WidePolygon polys[];
};
#endif

layout (std140, binding = 0) uniform Frame
{
    uvec4 uSize;          // width, height, DISP3DCNT, alpha test reference
    vec4 uScale;          // 2 / width, 2 / height, direction of y (+1 deko3D, -1 OpenGL), depth range (0: 0 to 1, 1: -1 to 1)
    uvec4 uFog;           // fog colour, offset, shift, flags (bit 0 fog on, bit 1 fog colours, bit 2 no textures)
    uvec4 uToon[8];       // the toon table, four to an entry
    uvec4 uFogDensity[9]; // the fog density table (34 entries), four to an entry
};

// first vertex, which pixels (0 all, 1 opaque only, 2 translucent only), depth bias, W-buffering
#ifdef WIDE_ES
// (A plain uniform there: a buffer written again for each of a picture's few hundred draws makes
// a phone's driver set a new one aside every time.)
uniform uvec4 uDraw;
#else
layout (std140, binding = 1) uniform Draw
{
    uvec4 uDraw;
};
#endif

flat out uint vPoly;
// The polygon's attributes, for wide_fsh.glsl's WIDE_TEXBUF variant (read once a vertex, not
// once a pixel).
flat out uvec4 vPolyData;
out vec3 vColor;
out vec2 vTex;
#ifdef WIDE_ES
// (OpenGL ES has no noperspective: the value times W, which wide_fsh.glsl divides out again.)
out float vZLinear;
#else
noperspective out float vZLinear;
#endif
out float vZPersp;

void main()
{
#ifdef WIDE_ES
    WideVertex v;
    v.x = aPos.x; v.y = aPos.y; v.w = aPos.z; v.z = aPos.w;
    v.r = aColS.x; v.g = aColS.y; v.b = aColS.z; v.s = aColS.w;
    v.t = aT;
    v.poly = aPoly;
#else
    WideVertex v = vtx[uDraw.x + uint(gl_VertexID)];
#endif
    // Rows run from the top of the picture in memory with both APIs: OpenGL's start at the
    // bottom of the view, deko3D's at the top.
    vec2 ndc = vec2(v.x * uScale.x - 1.0, uScale.z * (1.0 - v.y * uScale.y));
    // The depth tested by the graphics chip's own depth test (written by the shader, the test
    // would come after the shading, and every hidden pixel of the city's buildings was shaded:
    // 15 ms a 720-line picture). It has to vary straight across the screen: the DS's Z does;
    // with W-buffering the DS stores W, which does not, but 1/W does, so 1/W is tested the
    // other way round (GPU3D_WideGpu.cpp). Equal-depth polygons are moved back by their
    // tolerance (uDraw.z).
    float bias = float(uDraw.z);
    float d = uDraw.w != 0u ? 1.0 / max(v.z - bias, 1.0) : max(v.z - bias, 0.0) / 16777216.0;
    d = clamp(d, 0.0, 1.0);
    if (uScale.w != 0.0) d = d * 2.0 - 1.0;
    gl_Position = vec4(ndc * v.w, d * v.w, v.w);
    vPoly = v.poly;
#ifdef WIDE_ES
    vPolyData = uvec4(0u); // (wide_fsh.glsl reads the polygon itself there)
#else
    WidePolygon p = polys[v.poly];
    vPolyData = uvec4(p.attr, p.texParam, p.texPalette, p.flags);
#endif
    vColor = vec3(v.r, v.g, v.b);
    vTex = vec2(v.s, v.t);
    // Z-buffered depth runs straight across the screen; W-buffered depth is W itself,
    // which perspective-correct interpolation gives (the vertices carry it scaled alike).
#ifdef WIDE_ES
    vZLinear = v.z * v.w;
#else
    vZLinear = v.z;
#endif
    vZPersp = v.z;
}
