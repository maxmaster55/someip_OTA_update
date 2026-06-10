#!/bin/bash

UPDATE_FILE="${1:-./build/file_ota_update_2.6.wic.bz2}"

VSOMEIP_CONFIGURATION=./service.json \
    VSOMEIP_APPLICATION_NAME=ota_service \
    ./build/ota_service "$UPDATE_FILE"
