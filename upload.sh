#!/bin/bash

port=$1

if [ -z "$port" ]; then
	port=$(arduino-cli board list | awk '/arduino:sam:arduino_due_x/ {print $2}')
fi

arduino-cli upload -b arduino:sam:arduino_due_x -i firmware.ino.bin -p "$port"
