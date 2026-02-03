"""Search service - proxies web search queries."""

import logging
from typing import Optional

import httpx

from .base_service import BaseService
from ..protocol import SearchRequestPayload, SearchResponsePayload, SearchResult
from ..config import Config

logger = logging.getLogger(__name__)


class SearchService(BaseService):
    """Web search proxy service.

    Proxies search queries to DuckDuckGo and returns results.
    """

    # DuckDuckGo HTML API (lite version for simpler parsing)
    DUCKDUCKGO_URL = "https://html.duckduckgo.com/html/"

    def __init__(self, config: Config):
        self.config = config
        self._client = httpx.Client(
            timeout=30.0,
            headers={
                "User-Agent": "Mozilla/5.0 (compatible; rDeck Companion Server)"
            },
        )

    @property
    def name(self) -> str:
        return "search"

    def handle_request(self, payload: SearchRequestPayload) -> SearchResponsePayload:
        """Handle search request and return results.

        Args:
            payload: Search request with query

        Returns:
            Search response with results or error
        """
        try:
            max_results = min(payload.max_results, self.config.search_max_results)
            results = self._search_duckduckgo(payload.query, max_results)

            return SearchResponsePayload(
                query=payload.query,
                results=results,
                error=None,
            )

        except Exception as e:
            logger.exception(f"Search error: {e}")
            return SearchResponsePayload(
                query=payload.query,
                results=[],
                error=str(e),
            )

    def _search_duckduckgo(self, query: str, max_results: int) -> list[SearchResult]:
        """Perform search via DuckDuckGo HTML API."""
        try:
            response = self._client.post(
                self.DUCKDUCKGO_URL,
                data={"q": query, "b": ""},
            )
            response.raise_for_status()

            return self._parse_duckduckgo_html(response.text, max_results)

        except httpx.HTTPError as e:
            logger.error(f"DuckDuckGo HTTP error: {e}")
            raise RuntimeError(f"Search failed: {e}")

    def _parse_duckduckgo_html(self, html: str, max_results: int) -> list[SearchResult]:
        """Parse DuckDuckGo HTML response to extract results.

        This is a simple parser that doesn't require BeautifulSoup.
        """
        results = []

        # Find result blocks - they're in divs with class "result"
        # Look for the pattern: class="result__a" href="..."
        import re

        # Pattern for result links
        link_pattern = re.compile(
            r'class="result__a"[^>]*href="([^"]+)"[^>]*>([^<]+)</a>',
            re.IGNORECASE,
        )

        # Pattern for snippets
        snippet_pattern = re.compile(
            r'class="result__snippet"[^>]*>([^<]+(?:<[^>]+>[^<]*</[^>]+>)*[^<]*)</[^>]+>',
            re.IGNORECASE | re.DOTALL,
        )

        # Find all result divs
        result_div_pattern = re.compile(
            r'<div[^>]*class="[^"]*result[^"]*"[^>]*>(.*?)</div>\s*(?=<div|$)',
            re.IGNORECASE | re.DOTALL,
        )

        # Alternative simpler approach: find result__a links and nearby snippets
        # Split by result markers
        parts = re.split(r'<div[^>]*class="[^"]*result\s', html)

        for part in parts[1:]:  # Skip first part (before first result)
            if len(results) >= max_results:
                break

            # Extract URL and title
            link_match = link_pattern.search(part)
            if not link_match:
                continue

            url = link_match.group(1)
            title = link_match.group(2).strip()

            # Clean up DuckDuckGo redirect URL
            if url.startswith("//duckduckgo.com/l/?"):
                # Extract actual URL from DDG redirect
                url_match = re.search(r"uddg=([^&]+)", url)
                if url_match:
                    from urllib.parse import unquote
                    url = unquote(url_match.group(1))

            # Extract snippet
            snippet = ""
            snippet_match = re.search(
                r'class="result__snippet"[^>]*>(.*?)</(?:a|span|div)',
                part,
                re.IGNORECASE | re.DOTALL,
            )
            if snippet_match:
                # Strip HTML tags from snippet
                snippet = re.sub(r"<[^>]+>", "", snippet_match.group(1))
                snippet = snippet.strip()

            if title and url:
                results.append(SearchResult(
                    title=title[:100],  # Limit title length
                    url=url[:500],  # Limit URL length
                    snippet=snippet[:300],  # Limit snippet length
                ))

        return results
