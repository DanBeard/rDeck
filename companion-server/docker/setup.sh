#!/bin/bash
#
# setup.sh - Interactive setup for rDeck maps infrastructure
#
# Downloads a region PBF from Geofabrik, generates vector MBTiles with
# Planetiler, and configures tileserver-gl for on-the-fly raster rendering.
#
# Usage: ./setup.sh
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DATA_DIR="$SCRIPT_DIR/data"
PBF_DIR="$DATA_DIR/pbf"
MBTILES_DIR="$DATA_DIR/mbtiles"
TILESERVER_DIR="$DATA_DIR/tileserver"

# --- Region definitions ---
# Format: "label|geofabrik_path|approx_pbf_size|approx_mbtiles_size"
REGIONS=(
    # North America - US States
    "Colorado|north-america/us/colorado|~230MB|~60MB"
    "California|north-america/us/california|~1.0GB|~250MB"
    "Texas|north-america/us/texas|~700MB|~180MB"
    "New York|north-america/us/new-york|~400MB|~110MB"
    "Florida|north-america/us/florida|~350MB|~90MB"
    "Washington|north-america/us/washington|~300MB|~80MB"
    "Oregon|north-america/us/oregon|~200MB|~55MB"
    "Pennsylvania|north-america/us/pennsylvania|~350MB|~90MB"
    "Illinois|north-america/us/illinois|~350MB|~90MB"
    "Georgia (US)|north-america/us/georgia|~300MB|~80MB"
    # North America - Regions
    "US West|north-america/us-west|~1.5GB|~400MB"
    "US South|north-america/us-south|~2.0GB|~500MB"
    "US Midwest|north-america/us-midwest|~2.0GB|~500MB"
    "US Northeast|north-america/us-northeast|~1.5GB|~400MB"
    "Full US|north-america/us|~9.0GB|~2.0GB"
    # Europe
    "Germany|europe/germany|~4.0GB|~1.0GB"
    "France|europe/france|~4.5GB|~1.1GB"
    "Great Britain|europe/great-britain|~1.5GB|~400MB"
    "Italy|europe/italy|~2.0GB|~500MB"
    "Spain|europe/spain|~1.5GB|~400MB"
    "Switzerland|europe/switzerland|~400MB|~100MB"
    "Netherlands|europe/netherlands|~1.2GB|~300MB"
    "Full Europe|europe|~28GB|~7.0GB"
    # Asia
    "Japan|asia/japan|~2.0GB|~500MB"
    "South Korea|asia/south-korea|~300MB|~80MB"
    "Taiwan|asia/taiwan|~150MB|~40MB"
    # Oceania
    "Australia|australia-oceania/australia|~1.0GB|~250MB"
    "New Zealand|australia-oceania/new-zealand|~200MB|~55MB"
)

# --- Helper functions ---

print_header() {
    echo ""
    echo "============================================"
    echo "  rDeck Maps Setup"
    echo "============================================"
    echo ""
}

print_regions() {
    echo "Available regions:"
    echo ""
    local i=1
    local current_section=""

    for entry in "${REGIONS[@]}"; do
        IFS='|' read -r label path pbf_size mbtiles_size <<< "$entry"

        # Detect section from path
        local section=""
        case "$path" in
            north-america/us/*)   section="US States" ;;
            north-america/us*)    section="US Regions" ;;
            europe/*)             section="Europe" ;;
            asia/*)               section="Asia" ;;
            australia-oceania/*)  section="Oceania" ;;
        esac

        if [ "$section" != "$current_section" ]; then
            echo ""
            echo "  --- $section ---"
            current_section="$section"
        fi

        printf "  %2d) %-20s  PBF: %-8s  Tiles: %s\n" "$i" "$label" "$pbf_size" "$mbtiles_size"
        i=$((i + 1))
    done

    echo ""
    printf "  %2d) Custom Geofabrik URL path\n" "$i"
    echo ""
}

select_region() {
    local num_regions=${#REGIONS[@]}
    local custom_option=$((num_regions + 1))

    while true; do
        read -rp "Select region [1-$custom_option]: " choice

        if [ "$choice" -ge 1 ] 2>/dev/null && [ "$choice" -le "$num_regions" ] 2>/dev/null; then
            local idx=$((choice - 1))
            IFS='|' read -r REGION_LABEL GEOFABRIK_PATH PBF_SIZE MBTILES_SIZE <<< "${REGIONS[$idx]}"
            break
        elif [ "$choice" = "$custom_option" ]; then
            echo ""
            echo "Enter a Geofabrik path (e.g., 'north-america/us/colorado' or 'europe/germany')."
            echo "Browse: https://download.geofabrik.de/"
            read -rp "Geofabrik path: " GEOFABRIK_PATH
            if [ -z "$GEOFABRIK_PATH" ]; then
                echo "Error: Path cannot be empty."
                continue
            fi
            REGION_LABEL="Custom ($GEOFABRIK_PATH)"
            PBF_SIZE="unknown"
            MBTILES_SIZE="unknown"
            break
        else
            echo "Invalid selection."
        fi
    done

    # Derive download URL and filenames
    GEOFABRIK_URL="https://download.geofabrik.de/${GEOFABRIK_PATH}-latest.osm.pbf"
    PBF_FILENAME="$(echo "$GEOFABRIK_PATH" | tr '/' '-')-latest.osm.pbf"
}

confirm_download() {
    echo ""
    echo "Region:          $REGION_LABEL"
    echo "PBF download:    $GEOFABRIK_URL"
    echo "PBF size:        $PBF_SIZE"
    echo "MBTiles output:  $MBTILES_SIZE (approx)"
    echo ""
    echo "Data will be stored in: $DATA_DIR/"
    echo ""
    read -rp "Continue? [Y/n] " confirm
    case "$confirm" in
        [nN]*)
            echo "Aborted."
            exit 0
            ;;
    esac
}

download_pbf() {
    mkdir -p "$PBF_DIR"

    local target="$PBF_DIR/$PBF_FILENAME"

    if [ -f "$target" ]; then
        echo ""
        echo "PBF already exists: $target"
        read -rp "Re-download? [y/N] " redownload
        case "$redownload" in
            [yY]*) ;;
            *)
                echo "Using existing PBF."
                return 0
                ;;
        esac
    fi

    echo ""
    echo "Downloading PBF from Geofabrik..."
    echo "URL: $GEOFABRIK_URL"
    echo ""

    local partial="$target.partial"

    if curl -C - -L -o "$partial" "$GEOFABRIK_URL"; then
        mv "$partial" "$target"
        echo ""
        echo "Download complete: $target"
    else
        echo ""
        echo "ERROR: Download failed."
        echo "Check the URL and try again. Partial download saved for resume."
        exit 1
    fi
}

generate_mbtiles() {
    mkdir -p "$MBTILES_DIR"

    local input="$PBF_DIR/$PBF_FILENAME"
    local output="$MBTILES_DIR/tiles.mbtiles"

    if [ -f "$output" ]; then
        echo ""
        echo "MBTiles already exists: $output"
        read -rp "Regenerate? [y/N] " regen
        case "$regen" in
            [yY]*) rm -f "$output" ;;
            *)
                echo "Using existing MBTiles."
                return 0
                ;;
        esac
    fi

    echo ""
    echo "Generating vector MBTiles with Planetiler..."
    echo "Input:  $input"
    echo "Output: $output"
    echo ""
    echo "This may take a few minutes for small regions, longer for large ones."
    echo ""

    docker run --rm \
        -v "$PBF_DIR:/pbf:ro" \
        -v "$MBTILES_DIR:/output" \
        ghcr.io/onthegomap/planetiler:latest \
        --osm-path="/pbf/$PBF_FILENAME" \
        --output="/output/tiles.mbtiles" \
        --download \
        --force

    if [ -f "$output" ]; then
        local size
        size=$(du -h "$output" | cut -f1)
        echo ""
        echo "MBTiles generated: $output ($size)"
    else
        echo ""
        echo "ERROR: MBTiles generation failed."
        exit 1
    fi
}

copy_tileserver_config() {
    mkdir -p "$TILESERVER_DIR"

    echo ""
    echo "Copying tileserver configuration..."

    cp "$SCRIPT_DIR/tileserver-config.json" "$TILESERVER_DIR/tileserver-config.json"
    cp "$SCRIPT_DIR/tileserver-style.json" "$TILESERVER_DIR/grayscale-style.json"

    echo "Tileserver config written to $TILESERVER_DIR/"
}

clean_stale_container_data() {
    # Check if region changed from a previous setup.
    # Nominatim and Valhalla persist imported data — if the PBF changed,
    # their data must be wiped so they re-import from the new PBF.
    local env_file="$SCRIPT_DIR/.env"
    local old_pbf=""

    if [ -f "$env_file" ]; then
        old_pbf=$(grep '^REGION_PBF_FILE=' "$env_file" 2>/dev/null | cut -d= -f2)
    fi

    if [ -n "$old_pbf" ] && [ "$old_pbf" != "$PBF_FILENAME" ]; then
        echo ""
        echo "Region changed: $old_pbf → $PBF_FILENAME"
        echo "Clearing stale container data so services re-import from new PBF..."

        # Stop containers if running (they hold locks on the data dirs)
        if docker compose -f "$SCRIPT_DIR/docker-compose.yml" ps -q 2>/dev/null | grep -q .; then
            echo "Stopping running containers..."
            docker compose -f "$SCRIPT_DIR/docker-compose.yml" down 2>/dev/null || true
        fi

        if [ -d "$DATA_DIR/nominatim" ]; then
            echo "  Clearing Nominatim data (will re-import on next start)..."
            rm -rf "$DATA_DIR/nominatim"
            mkdir -p "$DATA_DIR/nominatim"
        fi

        if [ -d "$DATA_DIR/valhalla" ]; then
            echo "  Clearing Valhalla data (will rebuild routing graph on next start)..."
            rm -rf "$DATA_DIR/valhalla"
            mkdir -p "$DATA_DIR/valhalla"
        fi

        echo "  Done. All three services will use $PBF_FILENAME on next start."
    fi
}

write_env() {
    local env_file="$SCRIPT_DIR/.env"

    echo ""
    echo "Writing .env..."

    cat > "$env_file" << EOF
# Generated by setup.sh on $(date -Iseconds)
# Region: $REGION_LABEL
# Source: $GEOFABRIK_URL

REGION_PBF_FILE=$PBF_FILENAME
TILESERVER_PORT=8081

# Override these to use remote PBF URLs instead of local files:
# VALHALLA_PBF_URL=https://download.geofabrik.de/${GEOFABRIK_PATH}-latest.osm.pbf
# NOMINATIM_PBF_PATH is set to use the local PBF by default
EOF

    echo "Wrote $env_file"
}

print_summary() {
    echo ""
    echo "============================================"
    echo "  Setup Complete!"
    echo "============================================"
    echo ""
    echo "Data directory:  $DATA_DIR/"
    echo "PBF file:        $PBF_DIR/$PBF_FILENAME"
    echo "MBTiles file:    $MBTILES_DIR/tiles.mbtiles"
    echo "Tileserver conf: $TILESERVER_DIR/"
    echo ""
    echo "Next steps:"
    echo "  1. Start services:  ./run.sh"
    echo "  2. Services will be available at:"
    echo "     - Tileserver: http://localhost:8081"
    echo "     - Valhalla:   http://localhost:8002"
    echo "     - Nominatim:  http://localhost:8080"
    echo ""
}

# --- Main ---

print_header

# Check dependencies
if ! command -v docker &>/dev/null; then
    echo "ERROR: docker is required but not found."
    exit 1
fi

if ! command -v curl &>/dev/null; then
    echo "ERROR: curl is required but not found."
    exit 1
fi

print_regions
select_region
confirm_download
download_pbf
generate_mbtiles
copy_tileserver_config
clean_stale_container_data
write_env
print_summary
