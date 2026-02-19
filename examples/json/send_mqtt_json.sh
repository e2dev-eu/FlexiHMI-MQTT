#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "Usage: $0 <broker_ip> <topic> <json_file>" >&2
  exit 1
fi

broker_ip="$1"
topic="$2"
json_file="$3"

if [[ ! -f "$json_file" ]]; then
  echo "Error: JSON file not found: $json_file" >&2
  exit 1
fi

mosquitto_pub -h "$broker_ip" -t "$topic" -m "$(cat "$json_file")" -r
