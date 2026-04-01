import pytest
import mach


@pytest.fixture
def engine():
    params = mach.EngineInitParams()
    params.sample_rate = 48000
    params.block_size = 512
    params.max_node_pool_size = 16
    params.bpm = 120.0
    e = mach.Engine(params)
    yield e
    e.stop()


@pytest.fixture
def osc(engine):
    return engine.add_node("wavetable_oscillator")
