#/bin/bash


VSOMEIP_CONFIGURATION=./service.json \
    VSOMEIP_APPLICATION_NAME=ota_service \
    ./build/ota_service ./build/file_ota_update_2.6.wic.bz2