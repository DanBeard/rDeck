#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SERVER_DIR="$(dirname "$SCRIPT_DIR")"
DATA_DIR="$SCRIPT_DIR/data/companion-server"

# Check if setup has been run
if [ ! -f "$SCRIPT_DIR/data/mbtiles/tiles.mbtiles" ]; then
    echo ""
    echo "WARNING: No MBTiles file found. Run ./setup.sh first to download"
    echo "         map data and generate tiles for your region."
    echo ""
    echo "         The companion server will start but map tiles won't be"
    echo "         available until setup is complete."
    echo ""
fi

# Start Docker services (Valhalla, Nominatim, tileserver-gl)
echo "Starting Docker services..."
docker compose -f "$SCRIPT_DIR/docker-compose.yml" up -d

# Ensure data directory exists
mkdir -p "$DATA_DIR"

# Determine tileserver URL
TILESERVER_PORT="${TILESERVER_PORT:-8081}"
TILESERVER_URL="http://localhost:${TILESERVER_PORT}"

# Generate default config if none exists
if [ ! -f "$DATA_DIR/config.json" ]; then
    echo "Generating default companion server config..."
    cat > "$DATA_DIR/config.json" << EOF
{
  "server_name": "Companion Server",
  "enabled_services": ["ntp", "search", "maps"],
  "search_max_results": 5,
  "ai_summary_enabled": false,
  "ai_summary_model_path": null,
  "ai_summary_max_tokens": 256,
  "ai_summary_context_size": 2048,
  "ntp_refresh_interval": 3600,
  "maps_enabled": true,
  "maps_mbtiles_path": null,
  "maps_valhalla_url": "http://localhost:8002",
  "maps_nominatim_url": "http://localhost:8080",
  "maps_tileserver_url": "$TILESERVER_URL"
}
EOF
else
    # Update existing config with tileserver URL if not set
    if grep -q '"maps_tileserver_url": null' "$DATA_DIR/config.json" 2>/dev/null; then
        echo "Updating config with tileserver URL..."
        sed -i "s|\"maps_tileserver_url\": null|\"maps_tileserver_url\": \"$TILESERVER_URL\"|" "$DATA_DIR/config.json"
    elif ! grep -q 'maps_tileserver_url' "$DATA_DIR/config.json" 2>/dev/null; then
        # Field doesn't exist at all - add it before the closing brace
        echo "Adding tileserver URL to config..."
        sed -i "s|}|,\n  \"maps_tileserver_url\": \"$TILESERVER_URL\"\n}|" "$DATA_DIR/config.json"
    fi
fi

echo ""
echo "Tileserver: $TILESERVER_URL"
echo "Valhalla:   http://localhost:8002/status"
echo "Nominatim:  http://localhost:8080/search?q=test&format=json"
echo ""
echo "Starting companion server TUI..."
echo "(Ctrl+C to stop the server. Run 'docker compose -f $SCRIPT_DIR/docker-compose.yml down' to stop Docker services.)"
echo ""

# Run companion server natively with the TUI via uv
cd "$SERVER_DIR"
exec uv run python -m companion_server --data-dir "$DATA_DIR"
