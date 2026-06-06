#!/usr/bin/env python3
# /// script
# requires-python = ">=3.8"
# dependencies = [
#   "websocket-client",
# ]
# ///

import websocket
import math
import json
import random
import time as time_module

ws = websocket.WebSocket()
ws.connect("ws://localhost:9871")

DT = 0.05               # seconds per tick
SEGMENT_DURATION = 4.0  # seconds before switching to new random frequency
TRANSITION_DURATION = 1.0  # seconds to blend between old and new frequency
FREQ_MIN = 0.5          # Hz
FREQ_MAX = 5.0          # Hz


def random_freq():
    return random.uniform(FREQ_MIN, FREQ_MAX)


def smoothstep(t):
    """Cubic smoothstep: smooth start and end."""
    return t * t * (3.0 - 2.0 * t)


elapsed = 0.0
segment_elapsed = 0.0
old_freq = random_freq()
new_freq = random_freq()
phase = 0.0

while True:
    time_module.sleep(DT)
    elapsed += DT
    segment_elapsed += DT

    if segment_elapsed >= SEGMENT_DURATION:
        segment_elapsed -= SEGMENT_DURATION
        old_freq = new_freq
        new_freq = random_freq()

    # Smoothly blend frequency during the transition window at the start of each segment
    if segment_elapsed < TRANSITION_DURATION:
        t = smoothstep(segment_elapsed / TRANSITION_DURATION)
        freq = old_freq + (new_freq - old_freq) * t
    else:
        freq = new_freq

    # Phase accumulator keeps the signal continuous across frequency changes
    phase += 2.0 * math.pi * freq * DT

    data = {
        "timestamp": elapsed,
        "test_data": {
            "cos": math.cos(phase),
            "sin": math.sin(phase),
            "frequency": freq,
            "random": random.gauss(0, 1)
        }
    }
    ws.send(json.dumps(data))
