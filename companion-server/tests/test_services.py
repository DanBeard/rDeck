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


class TestNTPServiceEdgeCases:
    """Edge case tests for NTP service."""

    @pytest.fixture
    def service(self):
        return NTPService()

    def test_handle_zero_timestamp(self, service):
        """Test with zero client timestamp."""
        request = NTPRequestPayload(client_timestamp=0)
        response = service.handle_request(request)

        assert response.client_timestamp == 0
        assert response.server_timestamp > 0

    def test_handle_large_timestamp(self, service):
        """Test with large client timestamp."""
        large_ts = 2147483647  # Max signed 32-bit
        request = NTPRequestPayload(client_timestamp=large_ts)
        response = service.handle_request(request)

        assert response.client_timestamp == large_ts

    def test_multiple_rapid_requests(self, service):
        """Test multiple rapid requests return consistent times."""
        responses = []
        for i in range(5):
            request = NTPRequestPayload(client_timestamp=i * 1000)
            responses.append(service.handle_request(request))

        # All server timestamps should be within 1 second of each other
        timestamps = [r.server_timestamp for r in responses]
        assert max(timestamps) - min(timestamps) <= 1

        # Client timestamps should be echoed correctly
        for i, resp in enumerate(responses):
            assert resp.client_timestamp == i * 1000


class TestNTPProtocolIntegration:
    """Test NTP service with full protocol serialization."""

    @pytest.fixture
    def service(self):
        return NTPService()

    def test_full_protocol_roundtrip(self, service):
        """Test NTP request/response through full msgpack serialization."""
        from companion_server.protocol.serialization import _encode_payload, _decode_payload
        from companion_server.protocol.messages import MessageType

        # Create request
        original_request = NTPRequestPayload(client_timestamp=123456)

        # Encode to msgpack (as would go over wire)
        encoded = _encode_payload(MessageType.NTP_REQUEST, original_request)

        # Decode (as server would receive)
        decoded_request = _decode_payload(MessageType.NTP_REQUEST, encoded)

        # Process through service
        response = service.handle_request(decoded_request)

        # Encode response
        response_encoded = _encode_payload(MessageType.NTP_RESPONSE, response)

        # Decode response (as device would receive)
        decoded_response = _decode_payload(MessageType.NTP_RESPONSE, response_encoded)

        # Verify
        assert decoded_response.client_timestamp == 123456
        assert decoded_response.server_timestamp > 1700000000  # After 2023

    def test_ntp_response_matches_vector_format(self, service):
        """Verify NTP response encoding matches shared protocol vectors."""
        import msgpack

        request = NTPRequestPayload(client_timestamp=1000)
        response = service.handle_request(request)

        # Manually set server_timestamp for deterministic test
        response.server_timestamp = 1706825600

        from companion_server.protocol.serialization import _encode_payload
        from companion_server.protocol.messages import MessageType

        encoded = _encode_payload(MessageType.NTP_RESPONSE, response)
        decoded_raw = msgpack.unpackb(encoded, raw=False)

        # Verify structure matches what C++ expects
        assert "server_timestamp" in decoded_raw
        assert "client_timestamp" in decoded_raw
        assert decoded_raw["server_timestamp"] == 1706825600
        assert decoded_raw["client_timestamp"] == 1000


class TestSearchServiceEdgeCases:
    """Edge case tests for search service."""

    @pytest.fixture
    def config(self):
        config = Mock(spec=Config)
        config.search_max_results = 5
        return config

    @pytest.fixture
    def service(self, config):
        return SearchService(config)

    def test_empty_query(self, service):
        """Test handling of empty query string."""
        request = SearchRequestPayload(query="", max_results=5)

        with patch.object(service, '_search_duckduckgo', return_value=[]):
            response = service.handle_request(request)

        assert response.query == ""
        assert response.results == []
        assert response.error is None

    def test_special_characters_in_query(self, service):
        """Test query with special characters."""
        from companion_server.protocol.messages import SearchResult

        query = "python \"best practices\" & tips"
        request = SearchRequestPayload(query=query, max_results=5)

        mock_results = [SearchResult("Title", "http://example.com", "Snippet")]
        with patch.object(service, '_search_duckduckgo', return_value=mock_results):
            response = service.handle_request(request)

        assert response.query == query
        assert len(response.results) == 1

    def test_unicode_in_query(self, service):
        """Test query with unicode characters."""
        from companion_server.protocol.messages import SearchResult

        query = "python  tutorial"
        request = SearchRequestPayload(query=query, max_results=5)

        mock_results = [SearchResult("Title", "http://example.com", "Snippet")]
        with patch.object(service, '_search_duckduckgo', return_value=mock_results):
            response = service.handle_request(request)

        assert response.query == query

    def test_max_results_zero(self, service):
        """Test with max_results=0."""
        request = SearchRequestPayload(query="test", max_results=0)

        with patch.object(service, '_search_duckduckgo', return_value=[]):
            response = service.handle_request(request)

        assert response.results == []

    def test_config_limits_max_results(self, config, service):
        """Test that config.search_max_results limits results."""
        from companion_server.protocol.messages import SearchResult

        config.search_max_results = 2
        mock_results = [SearchResult(f"R{i}", f"http://ex.com/{i}", f"S{i}") for i in range(5)]

        request = SearchRequestPayload(query="test", max_results=10)

        # The service should request min(10, 2) = 2 results
        with patch.object(service, '_search_duckduckgo') as mock_search:
            mock_search.return_value = mock_results[:2]
            response = service.handle_request(request)

            # Verify _search_duckduckgo was called with limited max_results
            mock_search.assert_called_once()
            call_args = mock_search.call_args
            assert call_args[0][1] == 2  # max_results argument


class TestSearchHTMLParsing:
    """Tests for DuckDuckGo HTML parsing."""

    @pytest.fixture
    def config(self):
        config = Mock(spec=Config)
        config.search_max_results = 10
        return config

    @pytest.fixture
    def service(self, config):
        return SearchService(config)

    def test_parse_empty_html(self, service):
        """Test parsing empty HTML."""
        results = service._parse_duckduckgo_html("", 5)
        assert results == []

    def test_parse_no_results_html(self, service):
        """Test parsing HTML with no results."""
        html = "<html><body>No results found</body></html>"
        results = service._parse_duckduckgo_html(html, 5)
        assert results == []

    def test_parse_ddg_redirect_url(self, service):
        """Test that DDG redirect URLs are properly decoded."""
        html = '''
        <div class="result ">
            <a class="result__a" href="//duckduckgo.com/l/?uddg=https%3A%2F%2Fwww.python.org%2F">
                Python.org
            </a>
            <a class="result__snippet">The official home of Python.</a>
        </div>
        '''

        results = service._parse_duckduckgo_html(html, 5)

        assert len(results) >= 1
        assert results[0].url == "https://www.python.org/"
        assert "Python.org" in results[0].title

    def test_parse_respects_max_results(self, service):
        """Test that parsing stops at max_results."""
        html = ""
        for i in range(10):
            html += f'''
            <div class="result ">
                <a class="result__a" href="http://example.com/{i}">Title {i}</a>
                <a class="result__snippet">Snippet {i}</a>
            </div>
            '''

        results = service._parse_duckduckgo_html(html, 3)

        assert len(results) == 3

    def test_parse_long_title_truncated(self, service):
        """Test that long titles are truncated."""
        long_title = "A" * 200
        html = f'''
        <div class="result ">
            <a class="result__a" href="http://example.com">{long_title}</a>
            <a class="result__snippet">Snippet</a>
        </div>
        '''

        results = service._parse_duckduckgo_html(html, 5)

        assert len(results) == 1
        assert len(results[0].title) <= 100

    def test_parse_long_snippet_truncated(self, service):
        """Test that long snippets are truncated."""
        long_snippet = "B" * 500
        html = f'''
        <div class="result ">
            <a class="result__a" href="http://example.com">Title</a>
            <a class="result__snippet">{long_snippet}</a>
        </div>
        '''

        results = service._parse_duckduckgo_html(html, 5)

        assert len(results) == 1
        assert len(results[0].snippet) <= 300


class TestSearchProtocolIntegration:
    """Test Search service with full protocol serialization."""

    @pytest.fixture
    def config(self):
        config = Mock(spec=Config)
        config.search_max_results = 5
        return config

    @pytest.fixture
    def service(self, config):
        return SearchService(config)

    def test_full_protocol_roundtrip(self, service):
        """Test search request/response through full msgpack serialization."""
        from companion_server.protocol.serialization import _encode_payload, _decode_payload
        from companion_server.protocol.messages import MessageType, SearchResult

        # Create request
        original_request = SearchRequestPayload(query="reticulum network", max_results=3)

        # Encode to msgpack
        encoded = _encode_payload(MessageType.SEARCH_REQUEST, original_request)

        # Decode
        decoded_request = _decode_payload(MessageType.SEARCH_REQUEST, encoded)

        # Mock the actual search
        mock_results = [
            SearchResult("Reticulum Network", "https://reticulum.network", "Official site"),
        ]
        with patch.object(service, '_search_duckduckgo', return_value=mock_results):
            response = service.handle_request(decoded_request)

        # Encode response
        response_encoded = _encode_payload(MessageType.SEARCH_RESPONSE, response)

        # Decode response
        decoded_response = _decode_payload(MessageType.SEARCH_RESPONSE, response_encoded)

        # Verify
        assert decoded_response.query == "reticulum network"
        assert len(decoded_response.results) == 1
        assert decoded_response.results[0].title == "Reticulum Network"
        assert decoded_response.error is None

    def test_error_response_serialization(self, service):
        """Test that error responses serialize correctly."""
        from companion_server.protocol.serialization import _encode_payload, _decode_payload
        from companion_server.protocol.messages import MessageType

        request = SearchRequestPayload(query="test", max_results=5)

        with patch.object(service, '_search_duckduckgo', side_effect=RuntimeError("Connection failed")):
            response = service.handle_request(request)

        # Serialize and deserialize
        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, response)
        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, encoded)

        assert decoded.query == "test"
        assert decoded.results == []
        assert "Connection failed" in decoded.error

    def test_search_response_matches_vector_format(self, service):
        """Verify search response encoding matches shared protocol vectors."""
        import msgpack
        from companion_server.protocol.messages import SearchResult

        mock_results = [
            SearchResult("Result 1", "https://example.com/1", "First result"),
        ]

        request = SearchRequestPayload(query="test", max_results=5)
        with patch.object(service, '_search_duckduckgo', return_value=mock_results):
            response = service.handle_request(request)

        from companion_server.protocol.serialization import _encode_payload
        from companion_server.protocol.messages import MessageType

        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, response)
        decoded_raw = msgpack.unpackb(encoded, raw=False)

        # Verify structure matches what C++ expects
        assert "query" in decoded_raw
        assert "results" in decoded_raw
        assert "error" in decoded_raw
        assert decoded_raw["query"] == "test"
        assert len(decoded_raw["results"]) == 1
        assert decoded_raw["results"][0]["title"] == "Result 1"
        assert decoded_raw["results"][0]["url"] == "https://example.com/1"
        assert decoded_raw["results"][0]["snippet"] == "First result"


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


class TestSearchServiceAISummary:
    """Tests for AI summary functionality in search service."""

    @pytest.fixture
    def config_no_ai(self):
        """Config with AI summary disabled."""
        config = Mock(spec=Config)
        config.search_max_results = 5
        config.ai_summary_enabled = False
        config.ai_summary_model_path = None
        config.ai_summary_max_tokens = 256
        config.ai_summary_context_size = 2048
        return config

    @pytest.fixture
    def config_ai_enabled(self):
        """Config with AI summary enabled but no model path."""
        config = Mock(spec=Config)
        config.search_max_results = 5
        config.ai_summary_enabled = True
        config.ai_summary_model_path = None  # No actual model
        config.ai_summary_max_tokens = 256
        config.ai_summary_context_size = 2048
        return config

    @pytest.fixture
    def service_no_ai(self, config_no_ai):
        return SearchService(config_no_ai)

    @pytest.fixture
    def service_ai_enabled(self, config_ai_enabled):
        return SearchService(config_ai_enabled)

    def test_ai_summary_not_available_when_disabled(self, service_no_ai):
        """Test that AI summary is not available when disabled in config."""
        assert service_no_ai.ai_summary_available is False

    def test_ai_summary_not_available_without_model(self, service_ai_enabled):
        """Test that AI summary is not available without model path."""
        assert service_ai_enabled.ai_summary_available is False

    def test_request_without_ai_summary_flag(self, service_no_ai):
        """Test search request without AI summary flag."""
        from companion_server.protocol.messages import SearchResult

        mock_results = [
            SearchResult("Result 1", "http://example.com/1", "Snippet 1"),
        ]

        request = SearchRequestPayload(query="test", max_results=5, ai_summary=False)

        with patch.object(service_no_ai, '_search_duckduckgo', return_value=mock_results):
            response = service_no_ai.handle_request(request)

        assert response.summary is None
        assert len(response.results) == 1

    def test_request_with_ai_summary_flag_but_unavailable(self, service_no_ai):
        """Test that requesting AI summary when unavailable returns None."""
        from companion_server.protocol.messages import SearchResult

        mock_results = [
            SearchResult("Result 1", "http://example.com/1", "Snippet 1"),
        ]

        request = SearchRequestPayload(query="test", max_results=5, ai_summary=True)

        with patch.object(service_no_ai, '_search_duckduckgo', return_value=mock_results):
            response = service_no_ai.handle_request(request)

        # Summary should be None since AI is not available
        assert response.summary is None
        assert len(response.results) == 1

    def test_ai_summary_protocol_serialization(self):
        """Test that AI summary field serializes correctly."""
        from companion_server.protocol.serialization import _encode_payload, _decode_payload
        from companion_server.protocol.messages import MessageType, SearchResult

        # Create response with summary
        response = SearchResponsePayload(
            query="what is python",
            results=[SearchResult("Python.org", "https://python.org", "Official site")],
            error=None,
            summary="Python is a high-level programming language known for readability.",
        )

        # Encode and decode
        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, response)
        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, encoded)

        assert decoded.query == "what is python"
        assert len(decoded.results) == 1
        assert decoded.summary == "Python is a high-level programming language known for readability."
        assert decoded.error is None

    def test_ai_summary_request_protocol_serialization(self):
        """Test that AI summary request field serializes correctly."""
        from companion_server.protocol.serialization import _encode_payload, _decode_payload
        from companion_server.protocol.messages import MessageType
        import msgpack

        # Create request with ai_summary=True
        request = SearchRequestPayload(query="test query", max_results=3, ai_summary=True)

        # Encode
        encoded = _encode_payload(MessageType.SEARCH_REQUEST, request)

        # Verify raw msgpack contains ai_summary
        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert "ai_summary" in decoded_raw
        assert decoded_raw["ai_summary"] is True

        # Decode through protocol
        decoded = _decode_payload(MessageType.SEARCH_REQUEST, encoded)
        assert decoded.ai_summary is True

    def test_ai_summary_default_false(self):
        """Test that ai_summary defaults to False."""
        request = SearchRequestPayload(query="test")
        assert request.ai_summary is False

    def test_response_summary_default_none(self):
        """Test that response summary defaults to None."""
        response = SearchResponsePayload(query="test")
        assert response.summary is None


class TestSearchServiceWithMockLLM:
    """Tests for AI summary with mocked LLM."""

    @pytest.fixture
    def config_with_llm(self):
        config = Mock(spec=Config)
        config.search_max_results = 5
        config.ai_summary_enabled = True
        config.ai_summary_model_path = "/path/to/model.gguf"
        config.ai_summary_max_tokens = 256
        config.ai_summary_context_size = 2048
        return config

    def test_generate_summary_with_mock_llm(self, config_with_llm):
        """Test AI summary generation with mocked LLM."""
        from companion_server.protocol.messages import SearchResult

        # Mock the Llama import and instance
        with patch.dict('sys.modules', {'llama_cpp': MagicMock()}):
            with patch('companion_server.services.search_service.LLAMA_AVAILABLE', True):
                with patch('companion_server.services.search_service.Llama') as mock_llama_class:
                    # Create mock LLM instance
                    mock_llm = MagicMock()
                    mock_llm.return_value = {
                        "choices": [{"text": "This is a generated summary about Python."}]
                    }
                    mock_llama_class.return_value = mock_llm

                    service = SearchService(config_with_llm)
                    service._llm = mock_llm

                    # Test results
                    results = [
                        SearchResult("Python.org", "https://python.org", "Official Python site"),
                        SearchResult("Python Tutorial", "https://tutorial.com", "Learn Python"),
                    ]

                    summary = service._generate_summary("what is python", results)

                    # Verify LLM was called
                    mock_llm.assert_called_once()
                    assert summary == "This is a generated summary about Python."

    def test_generate_summary_handles_llm_error(self, config_with_llm):
        """Test that LLM errors are handled gracefully."""
        from companion_server.protocol.messages import SearchResult

        with patch.dict('sys.modules', {'llama_cpp': MagicMock()}):
            with patch('companion_server.services.search_service.LLAMA_AVAILABLE', True):
                with patch('companion_server.services.search_service.Llama') as mock_llama_class:
                    mock_llm = MagicMock()
                    mock_llm.side_effect = RuntimeError("LLM inference failed")
                    mock_llama_class.return_value = mock_llm

                    service = SearchService(config_with_llm)
                    service._llm = mock_llm

                    results = [
                        SearchResult("Test", "https://test.com", "Test snippet"),
                    ]

                    summary = service._generate_summary("test", results)

                    # Should return None on error, not raise
                    assert summary is None

    def test_generate_summary_empty_results(self, config_with_llm):
        """Test that empty results return None summary."""
        with patch.dict('sys.modules', {'llama_cpp': MagicMock()}):
            with patch('companion_server.services.search_service.LLAMA_AVAILABLE', True):
                with patch('companion_server.services.search_service.Llama') as mock_llama_class:
                    mock_llm = MagicMock()
                    mock_llama_class.return_value = mock_llm

                    service = SearchService(config_with_llm)
                    service._llm = mock_llm

                    summary = service._generate_summary("test", [])

                    # Should return None for empty results
                    assert summary is None
                    # LLM should not be called
                    mock_llm.assert_not_called()
