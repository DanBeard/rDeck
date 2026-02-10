#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SERVER_DIR="$(dirname "$SCRIPT_DIR")"
DATA_DIR="${COMPANION_DATA_DIR:-$HOME/.companion-server}"
OLD_DATA_DIR="$SCRIPT_DIR/data/companion-server"

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

# One-time migration: copy identity + trust from old docker data dir
if [ -f "$OLD_DATA_DIR/reticulum/identity" ]; then
    if [ ! -f "$DATA_DIR/reticulum/identity" ]; then
        echo "Migrating identity and trust data from docker/data/ to $DATA_DIR..."
        mkdir -p "$DATA_DIR/reticulum"
        cp "$OLD_DATA_DIR/reticulum/identity" "$DATA_DIR/reticulum/identity"
        [ -f "$OLD_DATA_DIR/trust.json" ] && cp "$OLD_DATA_DIR/trust.json" "$DATA_DIR/trust.json"
        echo "Migration complete. Old data preserved in $OLD_DATA_DIR"
    elif ! cmp -s "$OLD_DATA_DIR/reticulum/identity" "$DATA_DIR/reticulum/identity"; then
        # Identities differ — prefer docker one if it's newer (likely has active trust)
        if [ "$OLD_DATA_DIR/reticulum/identity" -nt "$DATA_DIR/reticulum/identity" ]; then
            echo "Updating identity from docker/data/ (newer, has active trust relationships)..."
            cp "$DATA_DIR/reticulum/identity" "$DATA_DIR/reticulum/identity.bak"
            cp "$OLD_DATA_DIR/reticulum/identity" "$DATA_DIR/reticulum/identity"
            [ -f "$OLD_DATA_DIR/trust.json" ] && cp "$OLD_DATA_DIR/trust.json" "$DATA_DIR/trust.json"
            echo "Identity updated. Old identity backed up to identity.bak"
        fi
    fi
fi

# Warn if Nominatim data looks stale (imported from a different region)
if [ -f "$SCRIPT_DIR/.env" ] && [ -d "$SCRIPT_DIR/data/nominatim" ]; then
    CURRENT_PBF=$(grep '^REGION_PBF_FILE=' "$SCRIPT_DIR/.env" 2>/dev/null | cut -d= -f2)
    # Check if Nominatim has already imported (marker file exists) but data might be stale
    if [ -f "$SCRIPT_DIR/data/nominatim/import-finished" ] && [ -n "$CURRENT_PBF" ]; then
        # If the PBF file is newer than the import marker, data is likely stale
        if [ "$SCRIPT_DIR/data/pbf/$CURRENT_PBF" -nt "$SCRIPT_DIR/data/nominatim/import-finished" ] 2>/dev/null; then
            echo ""
            echo "WARNING: Nominatim data may be stale (PBF is newer than import)."
            echo "         Run ./setup.sh again to re-import, or manually clear:"
            echo "         rm -rf $SCRIPT_DIR/data/nominatim && rm -rf $SCRIPT_DIR/data/valhalla"
            echo ""
        fi
    fi
fi

# Start Docker services (Valhalla, Nominatim, tileserver-gl)
echo "Starting Docker services..."
docker compose -f "$SCRIPT_DIR/docker-compose.yml" up -d

echo ""
echo "Tileserver: http://localhost:${TILESERVER_PORT:-8081}"
echo "Valhalla:   http://localhost:8002/status"
echo "Nominatim:  http://localhost:8080/search?q=test&format=json"
echo ""
echo "Starting companion server..."
echo "(Ctrl+C to stop the server. Run 'docker compose -f $SCRIPT_DIR/docker-compose.yml down' to stop Docker services.)"
echo ""

# Run companion server with maps enabled, passing through any extra args
cd "$SERVER_DIR"
exec uv run python -m companion_server --data-dir "$DATA_DIR" --with-maps "$@"
