#!/bin/bash

LOGS_DIR="/home/mehdi/contiki-ng-QLTSCH-plus/examples/salle_blanche_expleo/Orchestra/logs/various_exp"

COOJA_DIR="/home/mehdi/contiki-ng-QLTSCH-plus/tools/cooja"
COOJA_CONFIG_FILE="/home/mehdi/contiki-ng-QLTSCH-plus/examples/salle_blanche_expleo/Orchestra/orchestra_salle_blanche_sim.csc"

cd "$COOJA_DIR"

RANDOM_SEED=$((RANDOM % 100))
echo $RANDOM_SEED
# Replace the random seed value in the configuration file
#sed -i "s/<randomseed>[0-9]*<\/randomseed>/<randomseed>$RANDOM_SEED<\/randomseed>/g" "$COOJA_CONFIG_FILE"
./gradlew run --args="-nogui=$COOJA_CONFIG_FILE"
cp COOJA.testlog "$LOGS_DIR/Orchestra-$RANDOM_SEED.txt"

exit
