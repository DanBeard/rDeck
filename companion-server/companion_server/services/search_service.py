"""Search service - proxies web search queries with optional AI summaries."""

import logging
from typing import Optional

import httpx

from .base_service import BaseService
from ..protocol import SearchRequestPayload, SearchResponsePayload, SearchResult
from ..config import Config

logger = logging.getLogger(__name__)

# Optional llama-cpp-python import for AI summary
try:
    from llama_cpp import Llama
    LLAMA_AVAILABLE = True
except ImportError:
    LLAMA_AVAILABLE = False
    Llama = None


class SearchService(BaseService):
    """Web search proxy service with optional AI summary.

    Proxies search queries to DuckDuckGo and optionally generates
    AI summaries using a local LLM.
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
        self._llm: Optional[Llama] = None
        self._init_llm()

    def _init_llm(self):
        """Initialize LLM for AI summaries if configured."""
        if not self.config.ai_summary_enabled:
            logger.info("AI summary disabled in config")
            return

        if not LLAMA_AVAILABLE:
            logger.warning("AI summary enabled but llama-cpp-python not installed")
            return

        if not self.config.ai_summary_model_path:
            logger.warning("AI summary enabled but no model path configured")
            return

        try:
            logger.info(f"Loading LLM from {self.config.ai_summary_model_path}")
            self._llm = Llama(
                model_path=self.config.ai_summary_model_path,
                n_ctx=self.config.ai_summary_context_size,
                n_threads=4,
                verbose=False,
            )
            logger.info("LLM loaded successfully")
        except Exception as e:
            logger.error(f"Failed to load LLM: {e}")
            self._llm = None

    @property
    def name(self) -> str:
        return "search"

    @property
    def ai_summary_available(self) -> bool:
        """Check if AI summary is available."""
        return self._llm is not None

    def handle_request(self, payload: SearchRequestPayload) -> SearchResponsePayload:
        """Handle search request and return results.

        Args:
            payload: Search request with query

        Returns:
            Search response with results and optional AI summary
        """
        try:
            max_results = min(payload.max_results, self.config.search_max_results)
            results = self._search_duckduckgo(payload.query, max_results)

            # Generate AI summary if requested and available
            summary = None
            if payload.ai_summary and self._llm is not None:
                summary = self._generate_summary(payload.query, results)

            return SearchResponsePayload(
                query=payload.query,
                results=results,
                error=None,
                summary=summary,
            )

        except Exception as e:
            logger.exception(f"Search error: {e}")
            return SearchResponsePayload(
                query=payload.query,
                results=[],
                error=str(e),
                summary=None,
            )

    def _generate_summary(self, query: str, results: list[SearchResult]) -> Optional[str]:
        """Generate AI summary from search results.

        Args:
            query: The search query
            results: Search results to summarize

        Returns:
            AI-generated summary or None if failed
        """
        if not self._llm or not results:
            return None

        try:
            # Build context from search results
            context_parts = []
            for i, r in enumerate(results[:5], 1):  # Limit to 5 results for context
                context_parts.append(f"{i}. {r.title}\n{r.snippet}")

            context = "\n\n".join(context_parts)

            # Create prompt for summarization
            prompt = f"""Based on the following search results, provide a concise answer to the question: "{query}"

Search Results:
{context}

Provide a direct, helpful answer in 2-3 sentences. Focus on the most relevant information.

Answer:"""

            # Generate response
            response = self._llm(
                prompt,
                max_tokens=self.config.ai_summary_max_tokens,
                temperature=0.3,
                stop=["\n\n", "Search Results:", "Question:"],
            )

            summary = response["choices"][0]["text"].strip()

            # Clean up the summary
            if summary:
                # Remove any leading/trailing whitespace and ensure it's not empty
                summary = summary.strip()
                if len(summary) < 10:  # Too short, probably garbage
                    return None

            logger.info(f"Generated AI summary for query: {query[:50]}...")
            return summary

        except Exception as e:
            logger.error(f"AI summary generation failed: {e}")
            return None

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
