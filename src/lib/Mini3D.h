#pragma once

#include "GFX.h"
#include "VGA_esp32s3p4.h"

#include <math.h>
#include <stdlib.h>

// На всякий случай, если M_PI не определён
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ==================== ВЕКТОРА / МАТРИЦЫ ====================

struct Vec3 {
    float x, y, z;
};

struct Vec4 {
    float x, y, z, w;
};

struct Mat4 {
    float m[16]; // column-major (как в OpenGL)

    static Mat4 identity() {
        Mat4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    static Mat4 translation(float x, float y, float z) {
        Mat4 r = identity();
        r.m[12] = x;
        r.m[13] = y;
        r.m[14] = z;
        return r;
    }

    static Mat4 scale(float sx, float sy, float sz) {
        Mat4 r{};
        r.m[0]  = sx;
        r.m[5]  = sy;
        r.m[10] = sz;
        r.m[15] = 1.0f;
        return r;
    }

    static Mat4 rotationY(float a) {
        Mat4 r = identity();
        float c = cosf(a);
        float s = sinf(a);
        r.m[0]  =  c;  r.m[2]  = s;
        r.m[8]  = -s;  r.m[10] = c;
        return r;
    }

    static Mat4 rotationX(float a) {
        Mat4 r = identity();
        float c = cosf(a);
        float s = sinf(a);
        r.m[5]  =  c;  r.m[6]  = -s;
        r.m[9]  =  s;  r.m[10] =  c;
        return r;
    }

    static Mat4 perspective(float fov_deg, float aspect, float zn, float zf) {
        float fov_rad = fov_deg * (float)M_PI / 180.0f;
        float f = 1.0f / tanf(fov_rad * 0.5f);
        Mat4 r{};
        r.m[0]  = f / aspect;
        r.m[5]  = f;
        r.m[10] = (zf + zn) / (zn - zf);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * zf * zn) / (zn - zf);
        return r;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        // forward (z)
        Vec3 z{
            eye.x - target.x,
            eye.y - target.y,
            eye.z - target.z
        };
        float lenZ = sqrtf(z.x*z.x + z.y*z.y + z.z*z.z);
        if (lenZ == 0) lenZ = 1;
        z.x /= lenZ; z.y /= lenZ; z.z /= lenZ;

        // right (x) = up × z
        Vec3 x{
            up.y * z.z - up.z * z.y,
            up.z * z.x - up.x * z.z,
            up.x * z.y - up.y * z.x
        };
        float lenX = sqrtf(x.x*x.x + x.y*x.y + x.z*x.z);
        if (lenX == 0) lenX = 1;
        x.x /= lenX; x.y /= lenX; x.z /= lenX;

        // real up (y) = z × x
        Vec3 y{
            z.y * x.z - z.z * x.y,
            z.z * x.x - z.x * x.z,
            z.x * x.y - z.y * x.x
        };

        Mat4 r = Mat4::identity();
        r.m[0] = x.x; r.m[4] = x.y; r.m[8]  = x.z;
        r.m[1] = y.x; r.m[5] = y.y; r.m[9]  = y.z;
        r.m[2] = z.x; r.m[6] = z.y; r.m[10] = z.z;

        r.m[12] = -(x.x*eye.x + x.y*eye.y + x.z*eye.z);
        r.m[13] = -(y.x*eye.x + y.y*eye.y + y.z*eye.z);
        r.m[14] = -(z.x*eye.x + z.y*eye.y + z.z*eye.z);
        return r;
    }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c) {
        for (int r_ = 0; r_ < 4; ++r_) {
            r.m[c*4 + r_] =
                a.m[0*4 + r_] * b.m[c*4 + 0] +
                a.m[1*4 + r_] * b.m[c*4 + 1] +
                a.m[2*4 + r_] * b.m[c*4 + 2] +
                a.m[3*4 + r_] * b.m[c*4 + 3];
        }
    }
    return r;
}

inline Vec4 operator*(const Mat4& m, const Vec4& v) {
    Vec4 r;
    r.x = m.m[0]*v.x + m.m[4]*v.y + m.m[8]*v.z  + m.m[12]*v.w;
    r.y = m.m[1]*v.x + m.m[5]*v.y + m.m[9]*v.z  + m.m[13]*v.w;
    r.z = m.m[2]*v.x + m.m[6]*v.y + m.m[10]*v.z + m.m[14]*v.w;
    r.w = m.m[3]*v.x + m.m[7]*v.y + m.m[11]*v.z + m.m[15]*v.w;
    return r;
}

// ==================== Z-BUFFER ====================

class ZBuffer {
public:
    ZBuffer() : w(0), h(0), data(nullptr) {}

    bool init(int width, int height) {
        if (width <= 0 || height <= 0) return false;
        if (data) {
            free(data);
            data = nullptr;
        }
        w = width;
        h = height;
        data = (uint16_t*) malloc(w * h * sizeof(uint16_t));
        if (!data) {
            w = h = 0;
            return false;
        }
        clear();
        return true;
    }

    void clear() {
        if (!data) return;
        int total = w * h;
        for (int i = 0; i < total; ++i)
            data[i] = 0xFFFF; // "очень далеко"
    }

    inline bool testAndSet(int x, int y, uint16_t z) {
        int idx = y * w + x;
        uint16_t old = data[idx];
        if (z < old) {
            data[idx] = z;
            return true;
        }
        return false;
    }

    ~ZBuffer() {
        if (data) {
            free(data);
            data = nullptr;
        }
    }

    int w, h;
    uint16_t* data;
};

// ==================== MINI 3D-ДВИЖОК ====================

struct Vertex {
    Vec3 pos;
};

struct TriangleIdx {
    uint16_t a, b, c;
};

class Mini3D {
public:
    Mini3D(GFX& gfx, VGA_esp32s3p4& vga)
    : _gfx(gfx), _vga(vga), _width(0), _height(0)
    {
        // ZBuffer и размеры инициализируем позже, когда vga.init() уже отработал
    }

    void setCamera(const Vec3& eye, const Vec3& target, const Vec3& up,
                   float fov_deg = 60.0f, float zn = 0.1f, float zf = 100.0f)
    {
        updateSizeAndZ();

        float aspect = 1.0f;
        if (_height > 0) {
            aspect = (float)_width / (float)_height;
        }

        _view = Mat4::lookAt(eye, target, up);
        _proj = Mat4::perspective(fov_deg, aspect, zn, zf);
    }

    void setModel(const Mat4& m) {
        _model = m;
    }

    void beginFrame(uint16_t clearColor) {
        updateSizeAndZ();
        _zbuf.clear();
        _gfx.cls(clearColor);
    }

    void drawMesh(const Vertex* verts, int vertCount,
                  const TriangleIdx* tris, int triCount,
                  uint16_t col)
    {
        if (_width <= 0 || _height <= 0) return;
        if (!verts || !tris) return;

        Mat4 mvp = _proj * _view * _model;

        for (int i = 0; i < triCount; ++i) {
            const TriangleIdx& t = tris[i];
            if (t.a >= vertCount || t.b >= vertCount || t.c >= vertCount) continue;

            Vec4 v0 = transform(verts[t.a].pos, mvp);
            Vec4 v1 = transform(verts[t.b].pos, mvp);
            Vec4 v2 = transform(verts[t.c].pos, mvp);

            // за камерой
            if (v0.w <= 0 || v1.w <= 0 || v2.w <= 0) continue;

            Vec3 p0 = ndcToScreen(divideW(v0));
            Vec3 p1 = ndcToScreen(divideW(v1));
            Vec3 p2 = ndcToScreen(divideW(v2));

            rasterizeTriangle(p0, p1, p2, col);
        }
    }

    void drawMeshWireframe(const Vertex* verts, int vertCount,
                        const TriangleIdx* tris, int triCount,
                        uint16_t col)
    {
        Mat4 mvp = _proj * _view * _model;

        for (int i = 0; i < triCount; ++i) {
            const TriangleIdx& t = tris[i];

            Vec4 v0 = transform(verts[t.a].pos, mvp);
            Vec4 v1 = transform(verts[t.b].pos, mvp);
            Vec4 v2 = transform(verts[t.c].pos, mvp);

            // игнорируем треугольники за камерой
            if (v0.w <= 0 || v1.w <= 0 || v2.w <= 0) continue;

            Vec3 p0 = ndcToScreen(divideW(v0));
            Vec3 p1 = ndcToScreen(divideW(v1));
            Vec3 p2 = ndcToScreen(divideW(v2));

            // используем готовую быструю графику из GFX
            _gfx.triangle((int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y, col);
        }
    }

    void drawMeshWireframeFaces(const Vertex* verts, int vertCount,
                                    const TriangleIdx* tris, int triCount,
                                    const uint16_t* faceColors)
{
    Mat4 mvp = _proj * _view * _model;

    for (int i = 0; i < triCount; ++i) {
        const TriangleIdx& t = tris[i];

        Vec4 v0 = transform(verts[t.a].pos, mvp);
        Vec4 v1 = transform(verts[t.b].pos, mvp);
        Vec4 v2 = transform(verts[t.c].pos, mvp);

        // игнорируем треугольники за камерой
        if (v0.w <= 0 || v1.w <= 0 || v2.w <= 0) continue;

        Vec3 p0 = ndcToScreen(divideW(v0));
        Vec3 p1 = ndcToScreen(divideW(v1));
        Vec3 p2 = ndcToScreen(divideW(v2));

        // определяем "какая грань" по номеру треугольника
        int faceIndex = i / 2;          // 0..5 для куба
        uint16_t col  = faceColors[faceIndex];

        _gfx.fillTriangle((int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y, col);
    }
}

void drawMeshFaces(const Vertex* verts, int vertCount,
                   const TriangleIdx* tris, int triCount,
                   const uint16_t* faceColors)
{
    if (_width <= 0 || _height <= 0) return;
    if (!verts || !tris || !faceColors) return;

    Mat4 mvp = _proj * _view * _model;

    for (int i = 0; i < triCount; ++i) {
        const TriangleIdx& t = tris[i];
        if (t.a >= vertCount || t.b >= vertCount || t.c >= vertCount) continue;

        Vec4 v0 = transform(verts[t.a].pos, mvp);
        Vec4 v1 = transform(verts[t.b].pos, mvp);
        Vec4 v2 = transform(verts[t.c].pos, mvp);

        // за камерой
        if (v0.w <= 0 || v1.w <= 0 || v2.w <= 0) continue;

        Vec3 p0 = ndcToScreen(divideW(v0));
        Vec3 p1 = ndcToScreen(divideW(v1));
        Vec3 p2 = ndcToScreen(divideW(v2));

        // цвет грани: по 2 треугольника на грань
        int faceIndex = i / 2;     // 0..5 для куба
        uint16_t col  = faceColors[faceIndex];

        rasterizeTriangle(p0, p1, p2, col);  // тут уже есть Z-буфер
    }
}

private:
    GFX& _gfx;
    VGA_esp32s3p4& _vga;
    int _width, _height;
    ZBuffer _zbuf;

    Mat4 _model = Mat4::identity();
    Mat4 _view  = Mat4::identity();
    Mat4 _proj  = Mat4::identity();

    void updateSizeAndZ() {
        int w = _vga.Width();
        int h = _vga.Height();
        if (w <= 0 || h <= 0) return;

        // если размер изменился или буфер ещё не создан — пересоздаём
        if (w != _width || h != _height || _zbuf.data == nullptr) {
            _width  = w;
            _height = h;
            _zbuf.init(_width, _height);
        }
    }

    static Vec4 transform(const Vec3& v, const Mat4& m) {
        Vec4 r{v.x, v.y, v.z, 1.0f};
        return m * r;
    }

    static Vec3 divideW(const Vec4& v) {
        float invW = 1.0f / v.w;
        return Vec3{ v.x * invW, v.y * invW, v.z * invW };
    }

    Vec3 ndcToScreen(const Vec3& v) {
        // NDC: x,y,z ∈ [-1,1]
        float sx = (v.x * 0.5f + 0.5f) * (float)(_width  - 1);
        float sy = (-v.y * 0.5f + 0.5f) * (float)(_height - 1); // y вниз
        float sz = (v.z * 0.5f + 0.5f); // [0..1]
        return Vec3{ sx, sy, sz };
    }

    void rasterizeTriangle(const Vec3& v0, const Vec3& v1, const Vec3& v2, uint16_t col) {
        // bounding box
        float minXf = v0.x; if (v1.x < minXf) minXf = v1.x; if (v2.x < minXf) minXf = v2.x;
        float maxXf = v0.x; if (v1.x > maxXf) maxXf = v1.x; if (v2.x > maxXf) maxXf = v2.x;
        float minYf = v0.y; if (v1.y < minYf) minYf = v1.y; if (v2.y < minYf) minYf = v2.y;
        float maxYf = v0.y; if (v1.y > maxYf) maxYf = v1.y; if (v2.y > maxYf) maxYf = v2.y;

        int minX = (int) floorf(minXf);
        int maxX = (int) ceilf (maxXf);
        int minY = (int) floorf(minYf);
        int maxY = (int) ceilf (maxYf);

        if (maxX < 0 || maxY < 0 || minX >= _width || minY >= _height) return;

        if (minX < 0) minX = 0;
        if (minY < 0) minY = 0;
        if (maxX >= _width)  maxX = _width  - 1;
        if (maxY >= _height) maxY = _height - 1;

        // edge function
        auto edge = [](const Vec3& a, const Vec3& b, float x, float y) {
            return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
        };

        float area = edge(v0, v1, v2.x, v2.y);
        if (area == 0.0f) return; // вырожденный

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                float px = (float)x + 0.5f;
                float py = (float)y + 0.5f;

                float w0 = edge(v1, v2, px, py);
                float w1 = edge(v2, v0, px, py);
                float w2 = edge(v0, v1, px, py);

                // внутри треугольника (оба знака поддерживаем)
                if ((w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                    (w0 <= 0 && w1 <= 0 && w2 <= 0))
                {
                    w0 /= area;
                    w1 /= area;
                    w2 /= area;

                    float z = v0.z * w0 + v1.z * w1 + v2.z * w2; // [0..1]
                    if (z < 0.0f) z = 0.0f;
                    if (z > 1.0f) z = 1.0f;
                    uint16_t z16 = (uint16_t)(z * 65535.0f);

                    if (_zbuf.testAndSet(x, y, z16)) {
                        _gfx.putPixel(x, y, col);
                    }
                }
            }
        }
    }
};
