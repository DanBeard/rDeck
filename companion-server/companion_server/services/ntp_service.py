"""NTP service - provides time synchronization over Reticulum."""

import time
from typing import Any

from .base_service import BaseService
from ..protocol import NTPRequestPayload, NTPResponsePayload


class NTPService(BaseService):
    """NTP-like time service.

    Provides server time to devices for synchronization.
    Echoes back client timestamp for RTT calculation.
    """

    @property
    def name(self) -> str:
        return "ntp"

    def handle_request(self, payload: NTPRequestPayload) -> NTPResponsePayload:
        """Handle NTP request and return current server time.

        Args:
            payload: NTP request with client timestamp

        Returns:
            NTP response with server time and echoed client timestamp
        """
        return NTPResponsePayload(
            server_timestamp=int(time.time()),
            client_timestamp=payload.client_timestamp,
        )
