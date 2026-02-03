"""Pytest configuration for companion-server tests."""

import pytest
import sys
from pathlib import Path

# Add the companion_server package to the path
sys.path.insert(0, str(Path(__file__).parent.parent))
