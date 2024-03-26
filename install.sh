#!/bin/bash

sudo apt install jq

readonly CONFIG_FILE="config/config.json"
readonly JSON=`cat ${CONFIG_FILE}`

# テンプレのconfig.hを作成
readonly OUT_CONFIG="priority_server/src/conf/config.h"
echo "//common setting" > $OUT_CONFIG
echo "#define PORT $(jq -r '.CONFIG.port' <<< "${JSON}")" >> $OUT_CONFIG
echo "#define SERVER_IP \"$(jq -r '.CONFIG.ip' <<< "${JSON}")\"" >> $OUT_CONFIG
echo "" >> $OUT_CONFIG
echo "//for server" >> $OUT_CONFIG
echo "#define MEASURE_THROUGHPUT $(jq -r '.CONFIG.measure_throughput' <<< "${JSON}")" >> $OUT_CONFIG
echo "#define UPLOAD_DB $(jq -r '.CONFIG.upload_db' <<< "${JSON}")" >> $OUT_CONFIG
echo "#define RECVBUFSIZE $(jq -r '.CONFIG.receive_buffer_size' <<< "${JSON}")" >> $OUT_CONFIG
echo "" >> $OUT_CONFIG
echo "//for client" >> $OUT_CONFIG
echo "#define DEVICE_ID $(jq -r '.CONFIG.device_id' <<< "${JSON}")" >> $OUT_CONFIG
echo "#define SENDBUFSIZE $(jq -r '.CONFIG.send_buffer_size' <<< "${JSON}")" >> $OUT_CONFIG

# テンプレのdbconfig.hを作成
readonly OUT_DBCONFIG="priority_server/src/conf/dbconfig.h"
echo "//db configuration" > $OUT_DBCONFIG
echo "#define ORGANIZATION \"$(jq -r '.DBCONFIG.organization' <<< "${JSON}")\"" >> $OUT_DBCONFIG
echo "#define TOKEN \"$(jq -r '.DBCONFIG.token' <<< "${JSON}")\"" >> $OUT_DBCONFIG
echo "#define BUCKET \"$(jq -r '.DBCONFIG.bucket' <<< "${JSON}")\"" >> $OUT_DBCONFIG


# テンプレのserver_prioritydata.csvの作成
readonly OUT_SERVER_CSV="priority_server/src/conf/server_prioritydata.csv"
if [ -f "$OUT_SERVER_CSV" ]; then
    rm "$OUT_SERVER_CSV"
fi
readonly SAVE_PATH=$(jq -r '.SERVER_PRIORITY.save_path' <<< "${JSON}")
device_num=$(jq -r '.SERVER_PRIORITY.device_num' <<< "${JSON}")
image_num=$(jq -r '.SERVER_PRIORITY.image_num' <<< "${JSON}")

for ((i=1; i<=$device_num; i++))
do
    id_padding="0$i"
    id_name=${id_padding: -2}
    for ((j=1; j<=$image_num; j++))
    do
        echo "$i,$j,$SAVE_PATH/$id_name/image$j/,2" >> $OUT_SERVER_CSV
        mkdir -p "$SAVE_PATH/$id_name/image$j/"
    done
    echo "$i,$j,$SAVE_PATH/$id_name/sensor/,1" >> $OUT_SERVER_CSV
    mkdir -p "$SAVE_PATH/$id_name/sensor/"
done

# make server
cd priority_server/src
make server

# 定期実行設定
readonly SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
readonly SERVICE_FILE_PATH="/etc/systemd/system/priority_server.service"

readonly SERVICE_CONTENT="[Unit]
Description=Priority Sever Service

[Service]
ExecStart=/bin/bash $SCRIPT_DIR/start_priority_server.sh
WorkingDirectory=$SCRIPT_DIR
Restart=always
RestartSec=21600

[Install]
WantedBy=multi-user.target
"
echo "$SERVICE_CONTENT" | sudo tee "$SERVICE_FILE_PATH" > /dev/null
sudo systemctl daemon-reload
sudo systemctl restart priority_server.service

# loglotate設定
readonly LOG_FILE="priority_server/src/log/server.log"

readonly USER_NAME=$(whoami)
readonly GROUP_NAME=$(id -gn)

readonly LOG_ROTATE_CONFIG="/etc/logrotate.d/priority_server.conf"

readonly LOG_ROTATE_CONTENT="
$LOG_FILE {
    missingok
    ifempty
    rotate 4
    weekly
    dateext
    dateformat _%Y-%m-%d
    create 664 $USER_NAME $GROUP_NAME
    su $USER_NAME $GROUP_NAME
}
"
echo "$LOG_ROTATE_CONTENT" | sudo tee "$LOG_ROTATE_CONFIG" > /dev/null