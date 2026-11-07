"""Shader embedding and source-discovery regression checks."""
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from spirv_to_header import make_header
from rover.commands.shaders import _touch_sources
from rover.config import ProjectPaths


def spirv(name=b"main\0"):
    name += b"\0" * (-len(name) % 4)
    instruction = [(3 + len(name) // 4) << 16 | 15, 0, 1]
    instruction.extend(struct.unpack(f"<{len(name) // 4}I", name))
    words = [0x07230203, 0x00010600, 0, 2, 0, *instruction]
    return struct.pack(f"<{len(words)}I", *words)


class ShaderToolsTest(unittest.TestCase):
    def test_embedding_is_deterministic_and_uses_named_uint32_array(self):
        header = make_header(spirv(), "mesh_vert_spv")
        self.assertEqual(header, make_header(spirv(), "mesh_vert_spv"))
        self.assertIn("static const std::uint32_t mesh_vert_spv[]", header)
        self.assertIn("0x07230203u", header)

    def test_invalid_identifier_header_and_instructions_are_rejected(self):
        malformed = [b"", spirv()[:-1], b"bad!" + spirv()[4:],
                     spirv() + struct.pack("<I", 0),
                     spirv() + struct.pack("<I", 2 << 16)]
        for data in malformed:
            with self.subTest(data=data), self.assertRaises(ValueError):
                make_header(data, "shader_spv")
        with self.assertRaises(ValueError):
            make_header(spirv(), "not-an-identifier")

    def test_only_one_main_entry_point_is_accepted(self):
        for data in [spirv(b"vertex_main\0"), spirv()[:20], spirv() + spirv()[20:]]:
            with self.subTest(data=data), self.assertRaises(ValueError):
                make_header(data, "shader_spv")

    def test_source_discovery_follows_builtin_assets_and_ignores_other_files(self):
        with tempfile.TemporaryDirectory() as directory:
            paths = ProjectPaths(Path(directory))
            nested = paths.shaders_dir / "nested"
            nested.mkdir(parents=True)
            for name in ["vertex.slang", "pixel.hlsl", "old.glsl", "README.md"]:
                (nested / name).write_text("", encoding="utf-8")
            (nested / "directory.slang").mkdir()
            self.assertEqual(_touch_sources(paths), 2)
            self.assertEqual(paths.shaders_dir, paths.root / "main/builtin_assets/shaders")


if __name__ == "__main__":
    unittest.main()
