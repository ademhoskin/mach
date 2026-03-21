import pytest
import mach


def test_add_node_returns_handle(engine):
    osc = engine.add_node("wavetable_oscillator")
    assert osc is not None


def test_unknown_node_type_raises(engine):
    with pytest.raises(mach.EngineError, match="unknown node type"):
        engine.add_node("does_not_exist")


def test_remove_node(engine):
    osc = engine.add_node("wavetable_oscillator")
    engine.remove_node(osc)  # should not raise


def test_pool_exhaustion_raises(engine):
    nodes = [engine.add_node("wavetable_oscillator") for _ in range(16)]
    with pytest.raises(mach.EngineError, match="pool capacity exceeded"):
        engine.add_node("wavetable_oscillator")
