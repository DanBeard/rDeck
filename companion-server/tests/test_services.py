"""Tests for NTP and Search services."""

import time
import pytest
from unittest.mock import Mock, patch, MagicMock

from companion_server.services.ntp_service import NTPService
from companion_server.services.search_service import SearchService
from companion_server.protocol.messages import (
    NTPRequestPayload,
    NTPResponsePayload,
    SearchRequestPayload,
    SearchResponsePayload,
)
from companion_server.config import Config


class TestNTPService:
    """Test NTP service."""

    @pytest.fixture
    def service(self):
        return NTPService()

    def test_name(self, service):
        assert service.name == "ntp"

    def test_handle_request_returns_response(self, service):
        request = NTPRequestPayload(client_timestamp=12345)

        response = service.handle_request(request)

        assert isinstance(response, NTPResponsePayload)

    def test_handle_request_echoes_client_timestamp(self, service):
        request = NTPRequestPayload(client_timestamp=99999)

        response = service.handle_request(request)

        assert response.client_timestamp == 99999

    def test_handle_request_returns_current_time(self, service):
        before = int(time.time())
        request = NTPRequestPayload(client_timestamp=0)

        response = service.handle_request(request)

        after = int(time.time())

        assert before <= response.server_timestamp <= after

    def test_handle_request_server_timestamp_is_epoch_seconds(self, service):
        request = NTPRequestPayload(client_timestamp=0)

        response = service.handle_request(request)

        # Should be a reasonable Unix timestamp (after 2020)
        assert response.server_timestamp > 1577836800


class TestSearchService:
    """Test Search service."""

    @pytest.fixture
    def config(self):
        config = Mock(spec=Config)
        config.search_max_results = 5
        return config

    @pytest.fixture
    def service(self, config):
        return SearchService(config)

    def test_name(self, service):
        assert service.name == "search"

    def test_handle_request_returns_response(self, service):
        request = SearchRequestPayload(query="test")

        with patch.object(service, '_search_duckduckgo', return_value=[]):
            response = service.handle_request(request)

        assert isinstance(response, SearchResponsePayload)
        assert response.query == "test"

    def test_handle_request_includes_results(self, service):
        from companion_server.protocol.messages import SearchResult

        mock_results = [
            SearchResult(title="Result 1", url="http://example.com/1", snippet="Snippet 1"),
            SearchResult(title="Result 2", url="http://example.com/2", snippet="Snippet 2"),
        ]

        request = SearchRequestPayload(query="python", max_results=5)

        with patch.object(service, '_search_duckduckgo', return_value=mock_results):
            response = service.handle_request(request)

        assert len(response.results) == 2
        assert response.results[0].title == "Result 1"
        assert response.error is None

    def test_handle_request_limits_results(self, config, service):
        from companion_server.protocol.messages import SearchResult

        # Config limits to 5, request asks for 3
        config.search_max_results = 5
        mock_results = [SearchResult(f"R{i}", f"http://ex.com/{i}", f"S{i}") for i in range(10)]

        request = SearchRequestPayload(query="test", max_results=3)

        with patch.object(service, '_search_duckduckgo', return_value=mock_results[:3]):
            response = service.handle_request(request)

        # Should respect the minimum of config and request
        assert len(response.results) <= 3

    def test_handle_request_error_handling(self, service):
        request = SearchRequestPayload(query="test")

        with patch.object(service, '_search_duckduckgo', side_effect=RuntimeError("Network error")):
            response = service.handle_request(request)

        assert response.results == []
        assert response.error is not None
        assert "Network error" in response.error

    def test_parse_duckduckgo_html_extracts_results(self, service):
        """Test HTML parsing with sample DuckDuckGo HTML."""
        sample_html = '''
        <div class="result results_links results_links_deep web-result">
            <div class="links_main links_deep result__body">
                <a class="result__a" href="//duckduckgo.com/l/?uddg=https%3A%2F%2Fexample.com%2Fpage1">
                    Example Page 1
                </a>
                <a class="result__snippet">This is the first result snippet.</a>
            </div>
        </div>
        <div class="result results_links results_links_deep web-result">
            <div class="links_main links_deep result__body">
                <a class="result__a" href="//duckduckgo.com/l/?uddg=https%3A%2F%2Fexample.com%2Fpage2">
                    Example Page 2
                </a>
                <a class="result__snippet">This is the second result.</a>
            </div>
        </div>
        '''

        results = service._parse_duckduckgo_html(sample_html, 5)

        assert len(results) >= 1
        # Check that URLs are decoded from DDG redirect format
        assert "example.com" in results[0].url


class TestSearchServiceIntegration:
    """Integration-style tests for search service (may be slow/flaky)."""

    @pytest.fixture
    def config(self):
        config = Mock(spec=Config)
        config.search_max_results = 3
        return config

    @pytest.fixture
    def service(self, config):
        return SearchService(config)

    @pytest.mark.skip(reason="Integration test - requires network")
    def test_real_search(self, service):
        """Test actual DuckDuckGo search (skip by default)."""
        request = SearchRequestPayload(query="python programming", max_results=3)

        response = service.handle_request(request)

        assert response.query == "python programming"
        assert len(response.results) > 0
        assert response.error is None
