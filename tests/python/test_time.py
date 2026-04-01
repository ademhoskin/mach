import pytest
import mach


def test_samples_init():
    s = mach.Samples(100)
    assert s.count == 100


def test_seconds_init():
    s = mach.Seconds(1.5)
    assert s.value == pytest.approx(1.5)


def test_beats_init():
    b = mach.Beats(4.0)
    assert b.value == pytest.approx(4.0)


def test_note_constants_exist():
    assert mach.note.WHOLE.value == pytest.approx(4.0)
    assert mach.note.DOTTED_WHOLE.value == pytest.approx(6.0)
    assert mach.note.HALF.value == pytest.approx(2.0)
    assert mach.note.DOTTED_HALF.value == pytest.approx(3.0)
    assert mach.note.QUARTER.value == pytest.approx(1.0)
    assert mach.note.DOTTED_QUARTER.value == pytest.approx(1.5)
    assert mach.note.EIGHTH.value == pytest.approx(0.5)
    assert mach.note.DOTTED_EIGHTH.value == pytest.approx(0.75)
    assert mach.note.SIXTEENTH.value == pytest.approx(0.25)
    assert mach.note.DOTTED_SIXTEENTH.value == pytest.approx(0.375)
    assert mach.note.THIRTY_SECOND.value == pytest.approx(0.125)
    assert mach.note.DOTTED_THIRTY_SECOND.value == pytest.approx(0.1875)
    assert mach.note.SIXTY_FOURTH.value == pytest.approx(0.0625)
    assert mach.note.DOTTED_SIXTY_FOURTH.value == pytest.approx(0.09375)


def test_set_bpm(engine):
    engine.set_bpm(140.0)  # should not raise


def test_set_bpm_invalid_raises(engine):
    with pytest.raises(mach.EngineError, match="invalid parameter"):
        engine.set_bpm(0.0)

    with pytest.raises(mach.EngineError, match="invalid parameter"):
        engine.set_bpm(1001.0)


def test_schedule_set_bpm_with_beats(engine):
    engine.schedule_set_bpm(140.0, mach.Beats(1.0))  # should not raise


def test_schedule_set_bpm_with_seconds(engine):
    engine.schedule_set_bpm(140.0, mach.Seconds(0.5))  # should not raise


def test_schedule_set_bpm_with_samples(engine):
    engine.schedule_set_bpm(140.0, mach.Samples(48000))  # should not raise


def test_schedule_set_bpm_with_note_constant(engine):
    engine.schedule_set_bpm(140.0, mach.note.QUARTER)  # should not raise


def test_schedule_set_param_with_beats(engine):
    osc = engine.add_node("wavetable_oscillator")
    engine.schedule_set_param(osc, "frequency", 880.0, mach.Beats(1.0))


def test_schedule_set_param_with_seconds(engine):
    osc = engine.add_node("wavetable_oscillator")
    engine.schedule_set_param(osc, "frequency", 880.0, mach.Seconds(0.5))


def test_schedule_set_param_with_samples(engine):
    osc = engine.add_node("wavetable_oscillator")
    engine.schedule_set_param(osc, "frequency", 880.0, mach.Samples(48000))


def test_schedule_set_param_with_note_constant(engine):
    osc = engine.add_node("wavetable_oscillator")
    engine.schedule_set_param(osc, "frequency", 880.0, mach.note.QUARTER)


def test_schedule_set_param_with_enum(engine):
    osc = engine.add_node("wavetable_oscillator")
    engine.schedule_set_param(osc, "waveform", mach.Waveform.SAWTOOTH, mach.Beats(2.0))


def test_schedule_set_param_unknown_raises(engine):
    osc = engine.add_node("wavetable_oscillator")
    with pytest.raises(mach.EngineError, match="unknown param"):
        engine.schedule_set_param(osc, "nonexistent", 1.0, mach.Beats(1.0))


def test_sleep_with_seconds(engine):
    import time
    start = time.monotonic()
    engine.sleep(mach.Seconds(0.1))
    elapsed = time.monotonic() - start
    assert elapsed == pytest.approx(0.1, abs=0.05)


def test_sleep_with_samples(engine):
    import time
    engine.play()
    start = time.monotonic()
    engine.sleep(mach.Samples(4800))  # 0.1s at 48kHz
    elapsed = time.monotonic() - start
    assert elapsed == pytest.approx(0.1, abs=0.05)


def test_sleep_with_beats(engine):
    import time
    engine.play()
    start = time.monotonic()
    engine.sleep(mach.Beats(1.0))  # 0.5s at 120 bpm
    elapsed = time.monotonic() - start
    assert elapsed == pytest.approx(0.5, abs=0.05)
