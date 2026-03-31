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


def test_connect(engine):
    osc = engine.add_node("wavetable_oscillator")
    out = engine.get_master_output()
    engine.connect(osc, out)  # should not raise


def test_disconnect(engine):
    osc = engine.add_node("wavetable_oscillator")
    out = engine.get_master_output()
    engine.connect(osc, out)
    engine.disconnect(osc, out)  # should not raise


def test_pool_exhaustion_raises(engine):
    # NOTE: master output takes one slot at construction, so 15 fills the pool
    [engine.add_node("wavetable_oscillator") for _ in range(15)]
    with pytest.raises(mach.EngineError, match="pool capacity exceeded"):
        engine.add_node("wavetable_oscillator")
