import mach


def test_waveform_members_exist():
    assert hasattr(mach.Waveform, "SINE")
    assert hasattr(mach.Waveform, "SAWTOOTH")
    assert hasattr(mach.Waveform, "TRIANGLE")
    assert hasattr(mach.Waveform, "SQUARE")


def test_waveform_coerces_to_int(osc):
    # enums are integer-backed and dispatch to the int overload of __setitem__
    osc["waveform"] = mach.Waveform.TRIANGLE
