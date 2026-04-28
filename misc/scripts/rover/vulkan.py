"""Vulkan SDK / validation-layer discovery helpers.

Used by the ``run`` and ``debug`` commands to enable validation when the
caller passes ``--validation``. Falls back gracefully when the SDK is
not installed.
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import Optional

from rover import log
from rover.shell import which


def find_vulkan_sdk() -> Optional[Path]:
    """Locate the Vulkan SDK install root.

    Resolution order:
      1. ``VULKAN_SDK`` environment variable.
      2. The directory above ``glslangValidator`` on PATH (typical SDK layout).
      3. None.
    """
    sdk_env = os.environ.get("VULKAN_SDK")
    if sdk_env:
        p = Path(sdk_env)
        if p.is_dir():
            log.trace(f"VULKAN_SDK env var: {p}")
            return p

    validator = which("glslangValidator")
    if validator is not None:
        # SDK layout: <root>/x86_64/bin/glslangValidator -> root is two levels up.
        candidate = validator.parent.parent
        if (candidate / "share" / "vulkan").is_dir():
            log.trace(f"Discovered Vulkan SDK via glslangValidator: {candidate}")
            return candidate

    return None


def validation_env() -> dict[str, str]:
    """Return env vars enabling the Khronos validation layer.

    Always sets ``VK_INSTANCE_LAYERS``; additionally sets ``VK_LAYER_PATH``
    when an SDK install with explicit_layer.d manifests is found.
    """
    env: dict[str, str] = {"VK_INSTANCE_LAYERS": "VK_LAYER_KHRONOS_validation"}

    sdk = find_vulkan_sdk()
    if sdk is not None:
        layer_dir = sdk / "share" / "vulkan" / "explicit_layer.d"
        if layer_dir.is_dir():
            env["VK_LAYER_PATH"] = str(layer_dir)
            log.trace(f"Using validation layer manifests at {layer_dir}")
        else:
            log.warn(
                "Vulkan SDK found but explicit_layer.d is missing; relying on "
                "system layer paths."
            )
    else:
        log.warn(
            "Vulkan SDK not found; validation may be unavailable. Set "
            "VULKAN_SDK or install vulkan-validationlayers."
        )

    return env
