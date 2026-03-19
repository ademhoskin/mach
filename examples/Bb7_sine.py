"""Play Bb7 chord in 2nd octave with 4 sine oscillators."""

import time

from mach import _mach_ext as mach

params = mach.EngineInitParams()
params.sample_rate = 48000
params.block_size = 128
params.max_node_pool_size = 16

engine = mach.AudioEngine(params)

# Bb7 chord in 2nd octave: Bb2, D3, F3, Ab3
chord_frequencies = [116.54, 146.83, 174.61, 207.65]

for freq in chord_frequencies:
    osc = engine.add_node("wavetable_oscillator")
    engine.set_node_parameter(osc, 0, freq)

engine.play()

time.sleep(3)

engine.stop()
