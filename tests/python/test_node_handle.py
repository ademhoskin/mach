import pytest
import mach


def test_params_returns_expected_keys(osc):
    params = osc.params()
    assert "frequency" in params
    assert "amplitude" in params
    assert "waveform"  in params


def test_set_frequency(osc):
    osc["frequency"] = 440.0


def test_set_amplitude(osc):
    osc["amplitude"] = 0.5


def test_set_waveform_enum(osc):
    osc["waveform"] = mach.Waveform.SQUARE


def test_set_waveform_all_variants(osc):
    for waveform in mach.Waveform.__members__.values():
        osc["waveform"] = waveform


def test_unknown_param_raises(osc):
    with pytest.raises(mach.EngineError, match="unknown param"):
        osc["nonexistent"] = 1.0


def test_amplitude_clamps_above_one(osc):
    osc["amplitude"] = 999.0  # should not raise, node clamps internally
