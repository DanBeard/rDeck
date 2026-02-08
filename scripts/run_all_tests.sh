#!/bin/bash
# Run All Integration Tests
#
# Runs all three tiers of integration tests:
# - Tier 1: Protocol Vectors (C++ and Python)
# - Tier 2: Headless Service Integration (C++)
# - Tier 3: Full E2E Tests (optional, requires display)
#
# Usage:
#   ./scripts/run_all_tests.sh              # Run all tiers
#   ./scripts/run_all_tests.sh --quick      # Skip Tier 3 E2E tests
#   ./scripts/run_all_tests.sh --tier1      # Only Tier 1
#   ./scripts/run_all_tests.sh --tier2      # Only Tier 2
#   ./scripts/run_all_tests.sh --tier3      # Only Tier 3

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse arguments
RUN_TIER1=1
RUN_TIER2=1
RUN_TIER3=1

for arg in "$@"; do
    case $arg in
        --quick)
            RUN_TIER3=0
            ;;
        --tier1)
            RUN_TIER1=1
            RUN_TIER2=0
            RUN_TIER3=0
            ;;
        --tier2)
            RUN_TIER1=0
            RUN_TIER2=1
            RUN_TIER3=0
            ;;
        --tier3)
            RUN_TIER1=0
            RUN_TIER2=0
            RUN_TIER3=1
            ;;
        *)
            ;;
    esac
done

echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║           rDeck Integration Test Suite                 ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
echo

TOTAL_PASSED=0
TOTAL_FAILED=0

# =============================================================================
# Tier 1: Protocol Vectors
# =============================================================================

if [[ "$RUN_TIER1" -eq 1 ]]; then
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}  TIER 1: Protocol Test Vectors (C++ and Python)${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo

    # Python protocol vectors
    echo -e "${YELLOW}Running Python protocol vector tests...${NC}"
    cd companion-server
    if [[ -f ".venv/bin/activate" ]]; then
        source .venv/bin/activate
    fi

    if pytest tests/test_protocol_vectors.py -v --tb=short; then
        echo -e "${GREEN}✓ Python protocol vectors passed${NC}"
        ((TOTAL_PASSED++))
    else
        echo -e "${RED}✗ Python protocol vectors failed${NC}"
        ((TOTAL_FAILED++))
    fi
    cd ..
    echo

    # C++ protocol vectors
    echo -e "${YELLOW}Running C++ protocol vector tests...${NC}"
    if pio test -e test_native --filter "test_protocol_vectors" -v; then
        echo -e "${GREEN}✓ C++ protocol vectors passed${NC}"
        ((TOTAL_PASSED++))
    else
        echo -e "${RED}✗ C++ protocol vectors failed${NC}"
        ((TOTAL_FAILED++))
    fi
    echo
fi

# =============================================================================
# Tier 2: Headless Integration
# =============================================================================

if [[ "$RUN_TIER2" -eq 1 ]]; then
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}  TIER 2: Headless Service Integration Tests${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo

    echo -e "${YELLOW}Running C++ service integration tests...${NC}"
    # Integration tests are now part of test_native environment
    if pio test -e test_native --filter "integration/*" -v; then
        echo -e "${GREEN}✓ C++ service integration tests passed${NC}"
        ((TOTAL_PASSED++))
    else
        echo -e "${RED}✗ C++ service integration tests failed${NC}"
        ((TOTAL_FAILED++))
    fi
    echo
fi

# =============================================================================
# Tier 3: Full E2E
# =============================================================================

if [[ "$RUN_TIER3" -eq 1 ]]; then
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}  TIER 3: Full E2E Integration Tests${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo

    echo -e "${YELLOW}Running E2E tests (may require display or Xvfb)...${NC}"
    if "$SCRIPT_DIR/run_e2e_tests.sh"; then
        echo -e "${GREEN}✓ E2E tests passed${NC}"
        ((TOTAL_PASSED++))
    else
        echo -e "${RED}✗ E2E tests failed${NC}"
        ((TOTAL_FAILED++))
    fi
    echo
fi

# =============================================================================
# Summary
# =============================================================================

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}  Test Summary${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo

echo -e "  Passed: ${GREEN}$TOTAL_PASSED${NC}"
echo -e "  Failed: ${RED}$TOTAL_FAILED${NC}"
echo

if [[ "$TOTAL_FAILED" -eq 0 ]]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi
