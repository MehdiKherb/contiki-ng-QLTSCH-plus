#!/bin/bash

if [ -z "$1" ]; then
  echo "Usage: $0 <experiment_number>"
  exit 1
fi

EXPERIMENT_NUMBER=$1
mkdir /home/mehdi/contiki-ng/examples/Orchestra/logs/exp$EXPERIMENT_NUMBER

COOJA_DIR="/home/mehdi/contiki-ng/tools/cooja"
COOJA_CONFIG_FILE="/home/mehdi/contiki-ng/examples/Orchestra/orchestra_sim.csc"
COOJA_LOG_DIR="/home/mehdi/contiki-ng/examples/Orchestra/logs/exp$EXPERIMENT_NUMBER"

mkdir -p "$COOJA_LOG_DIR"

cd "$COOJA_DIR"

for i in {1..10}; do
  
  ./gradlew run --args="-nogui=$COOJA_CONFIG_FILE"
  cp COOJA.testlog "$COOJA_LOG_DIR/Orchestra-$i.txt"
done

exit

