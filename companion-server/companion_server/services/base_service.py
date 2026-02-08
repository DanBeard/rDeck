"""Base service interface."""

from abc import ABC, abstractmethod
from typing import Any


class BaseService(ABC):
    """Abstract base class for services."""

    @property
    @abstractmethod
    def name(self) -> str:
        """Service name identifier."""
        pass

    @abstractmethod
    def handle_request(self, payload: Any) -> Any:
        """Handle a service request and return response payload."""
        pass
