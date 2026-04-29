#include "modules/serialization/primitive_meshes.h"

#include <cmath>

namespace rover
{

    MeshData make_cube(f32 size)
    {
        const f32 h = size * 0.5f;

        MeshData m;
        m.vertices.reserve(24);
        m.indices.reserve(36);

        // Each face is a quad (4 vertices) with its own normal so lighting works
        // without duplication tricks. UV ranges 0..1 per face.
        const Vector3 face_normals[6] = {
            {0.0f, 0.0f, 1.0f},  // +Z front
            {0.0f, 0.0f, -1.0f}, // -Z back
            {1.0f, 0.0f, 0.0f},  // +X right
            {-1.0f, 0.0f, 0.0f}, // -X left
            {0.0f, 1.0f, 0.0f},  // +Y top
            {0.0f, -1.0f, 0.0f}, // -Y bottom
        };

        const Vector3 face_corners[6][4] = {
            // +Z front (CCW when viewed from +Z)
            {{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}},
            // -Z back
            {{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}},
            // +X right
            {{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}},
            // -X left
            {{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}},
            // +Y top
            {{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}},
            // -Y bottom
            {{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}},
        };

        const Vector2 quad_uvs[4] = {
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {1.0f, 1.0f},
            {0.0f, 1.0f},
        };

        for (int f = 0; f < 6; ++f)
        {
            const u32 base = static_cast<u32>(m.vertices.size());
            for (int c = 0; c < 4; ++c)
            {
                m.vertices.push_back({face_corners[f][c], face_normals[f], quad_uvs[c]});
            }
            m.indices.push_back(base + 0);
            m.indices.push_back(base + 1);
            m.indices.push_back(base + 2);
            m.indices.push_back(base + 0);
            m.indices.push_back(base + 2);
            m.indices.push_back(base + 3);
        }
        return m;
    }

    MeshData make_quad(f32 size)
    {
        const f32 h = size * 0.5f;
        MeshData  m;
        m.vertices = {
            {{-h, -h, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{h, -h, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{h, h, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-h, h, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        };
        m.indices = {0, 1, 2, 0, 2, 3};
        return m;
    }

    MeshData make_sphere(f32 radius, u32 latitude_segments, u32 longitude_segments)
    {
        if (latitude_segments < 2)
        {
            latitude_segments = 2;
        }
        if (longitude_segments < 3)
        {
            longitude_segments = 3;
        }

        MeshData m;
        m.vertices.reserve((latitude_segments + 1) * (longitude_segments + 1));
        m.indices.reserve(latitude_segments * longitude_segments * 6);

        constexpr f32 kPi = 3.14159265358979323846f;
        for (u32 lat = 0; lat <= latitude_segments; ++lat)
        {
            const f32 v     = static_cast<f32>(lat) / static_cast<f32>(latitude_segments);
            const f32 theta = v * kPi;
            const f32 sin_t = std::sin(theta);
            const f32 cos_t = std::cos(theta);
            for (u32 lon = 0; lon <= longitude_segments; ++lon)
            {
                const f32 u     = static_cast<f32>(lon) / static_cast<f32>(longitude_segments);
                const f32 phi   = u * 2.0f * kPi;
                const f32 sin_p = std::sin(phi);
                const f32 cos_p = std::cos(phi);

                Vector3 normal{cos_p * sin_t, cos_t, sin_p * sin_t};
                Vector3 pos = normal * radius;
                Vector2 uv{u, v};
                m.vertices.push_back({pos, normal, uv});
            }
        }

        const u32 ring = longitude_segments + 1;
        for (u32 lat = 0; lat < latitude_segments; ++lat)
        {
            for (u32 lon = 0; lon < longitude_segments; ++lon)
            {
                const u32 a = lat * ring + lon;
                const u32 b = a + 1;
                const u32 c = a + ring;
                const u32 d = c + 1;
                m.indices.push_back(a);
                m.indices.push_back(c);
                m.indices.push_back(b);
                m.indices.push_back(b);
                m.indices.push_back(c);
                m.indices.push_back(d);
            }
        }
        return m;
    }

} // namespace rover
