#!/usr/bin/env python3

import argparse
import math
import signal
import subprocess
import sys
import time


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Publish sinusoidal MQTT samples for FlexiHMI line chart demo"
    )
    parser.add_argument("--broker", default="127.0.0.1", help="MQTT broker host/IP")
    parser.add_argument("--topic", default="demo/line_chart", help="MQTT topic")
    parser.add_argument("--samples-per-sec", type=float, default=2.0, help="Sampling rate (Hz)")
    parser.add_argument("--samples-period", type=int, default=18, help="Samples per sinusoidal period")
    parser.add_argument("--center", type=float, default=50.0, help="Sine center value")
    parser.add_argument("--amplitude", type=float, default=45.0, help="Sine amplitude")
    parser.add_argument("--qos", type=int, choices=[0, 1, 2], default=0, help="MQTT QoS")
    parser.add_argument("--retain", action="store_true", help="Publish retained messages")
    return parser


def publish_sample(broker: str, topic: str, payload: str, qos: int, retain: bool) -> None:
    cmd = ["mosquitto_pub", "-h", broker, "-t", topic, "-q", str(qos), "-m", payload]
    if retain:
        cmd.append("-r")
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main() -> int:
    args = build_parser().parse_args()

    if args.samples_per_sec <= 0:
        print("samples-per-sec must be > 0", file=sys.stderr)
        return 2
    if args.samples_period <= 0:
        print("samples-period must be > 0", file=sys.stderr)
        return 2

    stop = False

    def handle_stop(signum, frame):
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, handle_stop)
    signal.signal(signal.SIGTERM, handle_stop)

    interval = 1.0 / args.samples_per_sec
    omega = (2.0 * math.pi) / float(args.samples_period)

    print(
        f"Publishing sine data to {args.topic} on {args.broker} "
        f"at {args.samples_per_sec} samples/sec, {args.samples_period} samples/period"
    )

    sample_index = 0
    next_tick = time.monotonic()

    while not stop:
        value = args.center + args.amplitude * math.sin(omega * sample_index)
        payload = str(int(round(value)))

        try:
            publish_sample(args.broker, args.topic, payload, args.qos, args.retain)
            print(f"n={sample_index:6d} -> {payload}")
        except subprocess.CalledProcessError:
            print("Failed to publish. Is mosquitto_pub installed and broker reachable?", file=sys.stderr)
            return 1

        sample_index += 1
        next_tick += interval
        sleep_for = next_tick - time.monotonic()
        if sleep_for > 0:
            time.sleep(sleep_for)
        else:
            next_tick = time.monotonic()

    print("Stopped.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
