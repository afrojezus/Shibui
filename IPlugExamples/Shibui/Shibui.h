#ifndef __SHIBUI__
#define __SHIBUI__

#pragma warning( suppress : 4101 4129 )
#include "IPlug_include_in_plug_hdr.h"

#include "Oscillator.h"
#include "MIDIReceiver.h"
#include "EnvelopeGenerator.h"

class Shibui : public IPlug
{
public:
  Shibui(IPlugInstanceInfo instanceInfo);
  ~Shibui();

  void Reset();
  void OnParamChange(int paramIdx);
  void ProcessDoubleReplacing(double** inputs, double** outputs, int nFrames);
  void ProcessMidiMsg(IMidiMsg* pMsg);

  // Needed for the GUI keyboard:
	// Should return non-zero if one or more keys are playing.
  inline int GetNumKeys() const { return mMIDIReceiver.getNumKeys(); };
  // Should return true if the specified key is playing.
  inline bool GetKeyStatus(int key) const { return mMIDIReceiver.getKeyStatus(key); };
  static const int virtualKeyboardMinimumNoteNumber = 48;
  int lastVirtualKeyboardNoteNumber;

private:
  double mFrequency;
  void CreatePresets();
  Oscillator mOscillator;
  MIDIReceiver mMIDIReceiver;
  IControl* mVirtualKeyboard;
  void processVirtualKeyboard();
  EnvelopeGenerator mEnvelopeGenerator;
  inline void onNoteOn(const int noteNumber, const int velocity) { mEnvelopeGenerator.enterStage(EnvelopeGenerator::ENVELOPE_STAGE_ATTACK); };
  inline void onNoteOff(const int noteNumber, const int velocity) { mEnvelopeGenerator.enterStage(EnvelopeGenerator::ENVELOPE_STAGE_RELEASE); };
  inline void onBeganEnvelopeCycle() { mOscillator.setMuted(false); }
  inline void onFinishedEnvelopeCycle() { mOscillator.setMuted(true); }
};

#endif
