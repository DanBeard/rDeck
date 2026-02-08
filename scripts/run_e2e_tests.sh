#!/bin/bash
# E2E Integration Tests Runner
#
# Runs full end-to-end tests with emulator and companion server.
# Requires: Xvfb, built emulator, companion server dependencies
#
# Usage:
#   ./scripts/run_e2e_tests.sh                    # Run with Xvfb
#   DISPLAY=:0 ./scripts/run_e2e_tests.sh         # Use existing display

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== rDeck E2E Integration Tests ===${NC}"
echo

# Check for emulator binary
EMULATOR_PATH=".pio/build/emulator_64bits/program"
if [[ ! -f "$EMULATOR_PATH" ]]; then
    echo -e "${YELLOW}Warning: Emulator not built at $EMULATOR_PATH${NC}"
    echo "Build with: pio run -e emulator_64bits"
    echo "Skipping emulator-dependent tests"
    SKIP_EMULATOR=1
fi

# Check for companion server venv
if [[ ! -d "companion-server/.venv" ]]; then
    echo -e "${YELLOW}Warning: Companion server venv not found${NC}"
    echo "Create with: cd companion-server && python3 -m venv .venv && source .venv/bin/activate && pip install -e ."
fi

# Determine if we need Xvfb
NEED_XVFB=0
if [[ -z "$DISPLAY" && -z "$SKIP_EMULATOR" ]]; then
    NEED_XVFB=1
    echo "No DISPLAY set, will use Xvfb"
fi

# Start Xvfb if needed
XVFB_PID=""
if [[ "$NEED_XVFB" -eq 1 ]]; then
    if ! command -v Xvfb &> /dev/null; then
        echo -e "${YELLOW}Xvfb not found - emulator tests will be skipped${NC}"
        SKIP_EMULATOR=1
    else
        echo "Starting Xvfb..."
        Xvfb :99 -screen 0 640x480x24 &
        XVFB_PID=$!
        export DISPLAY=:99
        sleep 1

        if ! kill -0 $XVFB_PID 2>/dev/null; then
            echo -e "${YELLOW}Xvfb failed to start${NC}"
            SKIP_EMULATOR=1
            XVFB_PID=""
        else
            echo "Xvfb started on :99"
        fi
    fi
fi

# Cleanup function
cleanup() {
    echo
    echo "Cleaning up..."

    if [[ -n "$XVFB_PID" ]]; then
        kill $XVFB_PID 2>/dev/null || true
        echo "Stopped Xvfb"
    fi
}
trap cleanup EXIT

# Create temp directory for test data
TEMP_DIR=$(mktemp -d -t e2e_test_XXXXXX)
echo "Test data directory: $TEMP_DIR"

# Run the tests
echo
echo -e "${GREEN}Running E2E tests...${NC}"
echo

cd companion-server

if [[ -f ".venv/bin/activate" ]]; then
    source .venv/bin/activate
fi

# Build pytest args
PYTEST_ARGS="tests/integration/ -v --tb=short"

if [[ -n "$SKIP_EMULATOR" ]]; then
    # Only run tests that don't require emulator
    PYTEST_ARGS="$PYTEST_ARGS -k 'not Emulator and not Trust and not NTP and not Search'"
fi

# Add timeout if pytest-timeout is available
if python -c "import pytest_timeout" 2>/dev/null; then
    PYTEST_ARGS="$PYTEST_ARGS --timeout=60"
fi

# Run tests
pytest $PYTEST_ARGS

EXIT_CODE=$?

echo
if [[ $EXIT_CODE -eq 0 ]]; then
    echo -e "${GREEN}E2E tests passed!${NC}"
else
    echo -e "${RED}E2E tests failed!${NC}"
fi

exit $EXIT_CODE
