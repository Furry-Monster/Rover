"""Check lint source selection and subprocess behavior using isolated databases."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from rover.cli import _build_parser
from rover.commands import lint
from rover.config import BuildType, ProjectPaths


class LintTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.paths = ProjectPaths(Path(self.directory.name))
        self.build = self.paths.root / "build with spaces"
        self.build.mkdir()
        self.database = self.build / "compile_commands.json"
        self.files = ["core/a.cpp", "services/graphics/b.cpp", "vendor/core/c.cpp", "build/generated/d.cpp"]
        for name in self.files:
            path = self.paths.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("int value;", encoding="utf-8")
        records = [{"directory": str(self.paths.root), "file": name, "arguments": ["c++", "-c", name]}
                   for name in self.files]
        self.database.write_text(json.dumps(records + records[:1]), encoding="utf-8")

    def args(self, **values):
        options = dict(build_dir=Path("build with spaces"), paths=None, checks=None,
                       warnings_as_errors=False, fix=False)
        options.update(values)
        return argparse.Namespace(**options)

    def test_only_first_party_translation_units_are_selected_and_deduplicated(self):
        files = lint._translation_units(self.paths, self.database, None)
        self.assertEqual(files, [self.paths.root / name for name in self.files[:2]])
        files = lint._translation_units(self.paths, self.database, [Path("services")])
        self.assertEqual(files, [self.paths.root / self.files[1]])
        self.assertEqual(lint._translation_units(self.paths, self.database, [Path("vendor")]), [])

    def test_relative_database_directory_is_resolved_from_database_location(self):
        self.database.write_text(json.dumps([{"directory": "..", "file": self.files[0]}]), encoding="utf-8")
        self.assertEqual(lint._translation_units(self.paths, self.database, None), [self.paths.root / self.files[0]])

    def test_flags_are_passed_as_arguments_and_failed_checks_propagate(self):
        with patch.object(lint.shell, "require", return_value=Path("/tools/clang-tidy")), \
             patch.object(lint.shell, "run", side_effect=[subprocess.CompletedProcess([], 2), subprocess.CompletedProcess([], 0)]) as run:
            code = lint.run(self.paths, BuildType.DEBUG, self.args(checks="-*,bugprone-*", warnings_as_errors=True, fix=True))
        self.assertEqual(code, 2)
        self.assertEqual(run.call_count, 2)
        command = run.call_args_list[0].args[0]
        self.assertIn(str(self.build), command)
        self.assertIn("--checks=-*,bugprone-*", command)
        self.assertIn("--warnings-as-errors=*", command)
        self.assertIn("--fix", command)
        self.assertTrue(any(arg.startswith("--header-filter=^") for arg in command))
        self.assertEqual(run.call_args_list[0].kwargs["cwd"], self.paths.root)

    def test_invalid_database_and_empty_selection_return_failure_without_running_tool(self):
        with patch.object(lint.shell, "run") as run:
            self.assertEqual(lint.run(self.paths, BuildType.DEBUG, self.args(paths=[Path("vendor")])), 1)
            self.database.write_text("{", encoding="utf-8")
            self.assertEqual(lint.run(self.paths, BuildType.DEBUG, self.args()), 1)
            self.database.unlink()
            self.assertEqual(lint.run(self.paths, BuildType.DEBUG, self.args()), 1)
            run.assert_not_called()

    def test_cli_registers_lint(self):
        args = _build_parser().parse_args(["lint", "--build-dir", "build", "--paths", "services"])
        self.assertEqual(args._handler, lint.run)
        self.assertEqual(args.paths, [Path("services")])


if __name__ == "__main__":
    unittest.main()
