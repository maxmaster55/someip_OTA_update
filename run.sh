#!/bin/bash

set -e

UPDATE_FILE="${1:-./build/file_ota_update_2.6.wic.bz2}"
MODE="${2:-all}"   # all, relay, daemon, service, gui

cleanup() {
    echo ""
    echo "Cleaning up..."
    pkill -9 -x ota_relay 2>/dev/null || true
    pkill -9 -x ota_daemon 2>/dev/null || true
    pkill -9 -x ota_service 2>/dev/null || true
    pkill -9 -x ota_gui 2>/dev/null || true
    sleep 1
    echo "Done."
}

case "$MODE" in
    all)
        trap cleanup EXIT INT TERM

        echo "=== Starting OTA 3-Node Architecture ==="
        echo ""

        # 1. Relay first (routing manager for local dev)
        echo "[1/4] Starting Relay (routing manager)..."
        VSOMEIP_CONFIGURATION=./relay.json \
            ./build/ota_relay ./example_relay_config.json &
        RELAY_PID=$!
        sleep 2

        # 2. Daemon (DaemonControl server - installation agent)
        echo "[2/4] Starting Daemon..."
        VSOMEIP_CONFIGURATION=./daemon.json \
            ./build/ota_daemon ./example_daemon_config.json &
        DAEMON_PID=$!
        sleep 1

        # 3. Service (firmware provider)
        echo "[3/4] Starting Service..."
        VSOMEIP_CONFIGURATION=./service.json \
            ./build/ota_service "$UPDATE_FILE" &
        SERVICE_PID=$!
        sleep 2

        # 4. GUI (optional)
        echo "[4/4] Starting GUI..."
        VSOMEIP_CONFIGURATION=./service.json \
            VSOMEIP_APPLICATION_NAME=ota_gui_proxy \
            ./build/ota_gui &
        GUI_PID=$!

        echo ""
        echo "=== All components running ==="
        echo "  Relay   PID=$RELAY_PID  (routing manager)"
        echo "  Daemon  PID=$DAEMON_PID"
        echo "  Service PID=$SERVICE_PID"
        echo "  GUI     PID=$GUI_PID"
        echo ""
        echo "Press Ctrl+C to stop all"
        echo ""

        wait
        ;;

    relay)
        echo "Starting Relay only..."
        VSOMEIP_CONFIGURATION=./relay.json \
            ./build/ota_relay ./example_relay_config.json
        ;;

    daemon)
        echo "Starting Daemon only...  (relay must be running for routing)"
        VSOMEIP_CONFIGURATION=./daemon.json \
            ./build/ota_daemon ./example_daemon_config.json
        ;;

    service)
        echo "Starting Service only...  (relay must be running for routing)"
        VSOMEIP_CONFIGURATION=./service.json \
            ./build/ota_service "$UPDATE_FILE"
        ;;

    gui)
        echo "Starting GUI only..."
        VSOMEIP_CONFIGURATION=./service.json \
            VSOMEIP_APPLICATION_NAME=ota_gui_proxy \
            ./build/ota_gui
        ;;

    cleanup)
        cleanup
        ;;

    *)
        echo "Usage: $0 [update_file] [mode]"
        echo ""
        echo "Modes:"
        echo "  all     - Start all 4 components (default)"
        echo "  relay   - Start relay only"
        echo "  daemon  - Start daemon only"
        echo "  service - Start service only"
        echo "  gui     - Start GUI only"
        echo "  cleanup - Kill all OTA processes"
        echo ""
        echo "Examples:"
        echo "  $0                                    # Default mode (all)"
        echo "  $0 my_firmware.bin                    # Custom file, all components"
        echo "  $0 my_firmware.bin relay              # Custom file, relay only"
        echo "  $0 '' cleanup                         # Kill all processes"
        ;;
esac
