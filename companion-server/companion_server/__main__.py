"""Entry point for companion server."""

import argparse
import asyncio
import signal
import sys
import time
import logging
from pathlib import Path

from .config import Config
from .reticulum_service import ReticulumService
from .trust_manager import TrustManager


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(description="Reticulum companion server for rDeck")
    parser.add_argument(
        "--headless",
        action="store_true",
        help="Run without TUI (for testing/daemon mode)",
    )
    parser.add_argument(
        "--tcp-port",
        type=int,
        help="TCP port for Reticulum TCP interface (enables TCP mode)",
    )
    parser.add_argument(
        "--data-dir",
        type=str,
        help="Override data directory path",
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Enable verbose logging",
    )
    args = parser.parse_args()

    # Configure logging
    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(
        level=log_level,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%H:%M:%S",
    )

    # Load configuration
    if args.data_dir:
        config = Config(data_dir=Path(args.data_dir))
    else:
        config = Config()

    # TODO: If tcp-port is specified, configure Reticulum TCP interface
    # This would require modifying the Reticulum config or using a custom interface

    # Initialize trust manager
    trust_manager = TrustManager(config.data_dir)

    # Initialize Reticulum service
    rns_service = ReticulumService(config, trust_manager)

    # Handle shutdown gracefully
    def shutdown_handler(signum, frame):
        rns_service.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown_handler)
    signal.signal(signal.SIGTERM, shutdown_handler)

    # Start Reticulum in background
    rns_service.start()

    if args.headless:
        # Headless mode: just run the service without TUI
        print(f"Companion server running in headless mode")
        print(f"Server name: {config.server_name}")
        print(f"Data directory: {config.data_dir}")
        if rns_service.destination_hash:
            print(f"LXMF destination: {rns_service.destination_hash}")

        # Log callback for headless mode
        def log_handler(msg: str):
            print(f"[LOG] {msg}")

        rns_service.on_log(log_handler)

        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
    else:
        # TUI mode
        from .tui.app import CompanionServerApp
        app = CompanionServerApp(rns_service, trust_manager)
        app.run()

    # Cleanup
    rns_service.stop()


if __name__ == "__main__":
    main()
