#!/bin/bash

LOGS_DIR="/home/mehdi/contiki-ng/examples/OST/logs/various_exp"

COOJA_DIR="/home/mehdi/contiki-ng/tools/cooja"
COOJA_CONFIG_FILE="/home/mehdi/contiki-ng/examples/OST/ql-tsch-plus-sim.csc"

cd "$COOJA_DIR"

RANDOM_SEED=$((RANDOM % 100000))
# Replace the random seed value in the configuration file
sed -i "s/<randomseed>[0-9]*<\/randomseed>/<randomseed>$RANDOM_SEED<\/randomseed>/g" "$COOJA_CONFIG_FILE"
./gradlew run --args="-nogui=$COOJA_CONFIG_FILE"
cp COOJA.testlog "$LOGS_DIR/OST-$RANDOM_SEED.txt"

exit
