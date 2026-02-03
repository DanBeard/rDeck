"""Entry point for companion server."""

import asyncio
import signal
import sys
from pathlib import Path

from .config import Config
from .reticulum_service import ReticulumService
from .trust_manager import TrustManager
from .tui.app import CompanionServerApp


def main():
    """Main entry point."""
    # Load configuration
    config = Config()

    # Initialize trust manager
    trust_manager = TrustManager(config.data_dir)

    # Initialize Reticulum service
    rns_service = ReticulumService(config, trust_manager)

    # Create and run TUI app
    app = CompanionServerApp(rns_service, trust_manager)

    # Handle shutdown gracefully
    def shutdown_handler(signum, frame):
        rns_service.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown_handler)
    signal.signal(signal.SIGTERM, shutdown_handler)

    # Start Reticulum in background
    rns_service.start()

    # Run the TUI
    app.run()

    # Cleanup
    rns_service.stop()


if __name__ == "__main__":
    main()
