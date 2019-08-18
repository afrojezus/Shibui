#include "Shibui.h"
#pragma warning( suppress : 4101 4129 )
#include "IPlug_include_in_plug_src.h"
#include "IControl.h"
#include "resource.h"
#include "IKeyboardControl.h"

#include <math.h>
#include <algorithm>

const int kNumPrograms = 5;

enum EParams
{
	mWaveform = 0,
	mAttack,
	mDecay,
	mSustain,
	mRelease,
	kNumParams
};

enum ELayout
{
  kWidth = GUI_WIDTH,
  kHeight = GUI_HEIGHT,
  kKeybX = 1,
  kKeybY = 230
};

Shibui::Shibui(IPlugInstanceInfo instanceInfo)
  :	IPLUG_CTOR(kNumParams, kNumPrograms, instanceInfo), lastVirtualKeyboardNoteNumber(virtualKeyboardMinimumNoteNumber - 1)
{
  TRACE;
  IGraphics* pGraphics = MakeGraphics(this, kWidth, kHeight);
  pGraphics->AttachBackground(BG_ID, BG_FN);

  IBitmap whiteKeyImage = pGraphics->LoadIBitmap(WHITE_KEY_ID, WHITE_KEY_FN, 6);
  IBitmap blackKeyImage = pGraphics->LoadIBitmap(BLACK_KEY_ID, BLACK_KEY_FN);

  //                            C#     D#          F#      G#      A#
  int keyCoordinates[12] = { 0, 7, 12, 20, 24, 36, 43, 48, 56, 60, 69, 72 };
  mVirtualKeyboard = new IKeyboardControl(this, kKeybX, kKeybY, virtualKeyboardMinimumNoteNumber, /* octaves: */ 5, &whiteKeyImage, &blackKeyImage, keyCoordinates);

  pGraphics->AttachControl(mVirtualKeyboard);
  // Waveform switch
  GetParam(mWaveform)->InitEnum("Waveform", OSCILLATOR_MODE_SINE, kNumOscillatorModes);
  GetParam(mWaveform)->SetDisplayText(0, "Sine");
  IBitmap waveformBitmap = pGraphics->LoadIBitmap(WAVEFORM_ID, WAVEFORM_FN, 4);
  pGraphics->AttachControl(new ISwitchControl(this, 24, 53, mWaveform, &waveformBitmap));

  // Knob bitmap for ADSR
  IBitmap knobBitmap = pGraphics->LoadIBitmap(KNOB_ID, KNOB_FN, 64);
  // Attack knob:
  GetParam(mAttack)->InitDouble("Attack", 0.01, 0.01, 10.0, 0.001);
  GetParam(mAttack)->SetShape(3);
  pGraphics->AttachControl(new IKnobMultiControl(this, 95, 34, mAttack, &knobBitmap));
  // Decay knob:
  GetParam(mDecay)->InitDouble("Decay", 0.5, 0.01, 15.0, 0.001);
  GetParam(mDecay)->SetShape(3);
  pGraphics->AttachControl(new IKnobMultiControl(this, 177, 34, mDecay, &knobBitmap));
  // Sustain knob:
  GetParam(mSustain)->InitDouble("Sustain", 0.1, 0.001, 1.0, 0.001);
  GetParam(mSustain)->SetShape(2);
  pGraphics->AttachControl(new IKnobMultiControl(this, 259, 34, mSustain, &knobBitmap));
  // Release knob:
  GetParam(mRelease)->InitDouble("Release", 1.0, 0.001, 15.0, 0.001);
  GetParam(mRelease)->SetShape(3);
  pGraphics->AttachControl(new IKnobMultiControl(this, 341, 34, mRelease, &knobBitmap));
  AttachGraphics(pGraphics);

  CreatePresets();

  mMIDIReceiver.noteOn.Connect(this, &Shibui::onNoteOn);
  mMIDIReceiver.noteOff.Connect(this, &Shibui::onNoteOff);
  mEnvelopeGenerator.beganEnvelopeCycle.Connect(this, &Shibui::onBeganEnvelopeCycle);
  mEnvelopeGenerator.finishedEnvelopeCycle.Connect(this, &Shibui::onFinishedEnvelopeCycle);
}

void Shibui::ProcessMidiMsg(IMidiMsg* pMsg) {
	mMIDIReceiver.onMessageReceived(pMsg);
	mVirtualKeyboard->SetDirty();
}

Shibui::~Shibui() {}

void Shibui::ProcessDoubleReplacing(double** inputs, double** outputs, int nFrames)
{
	// Mutex is already locked for us.

	double* leftOutput = outputs[0];
	double* rightOutput = outputs[1];

	processVirtualKeyboard();

	for (int i = 0; i < nFrames; ++i) {
		mMIDIReceiver.advance();
		int velocity = mMIDIReceiver.getLastVelocity();
		mOscillator.setFrequency(mMIDIReceiver.getLastFrequency());
		leftOutput[i] = rightOutput[i] = mOscillator.nextSample() * mEnvelopeGenerator.nextSample() * velocity / 127.0;
	}

	mMIDIReceiver.Flush(nFrames);
}

void Shibui::Reset()
{
  TRACE;
  IMutexLock lock(this);
  mOscillator.setSampleRate(GetSampleRate());
  mEnvelopeGenerator.setSampleRate(GetSampleRate());
}

void Shibui::OnParamChange(int paramIdx)
{
  IMutexLock lock(this);
  switch (paramIdx) {
  case mWaveform:
	  mOscillator.setMode(static_cast<OscillatorMode>(GetParam(mWaveform)->Int()));
	  break;
  case mAttack:
  case mDecay:
  case mSustain:
  case mRelease:
	  mEnvelopeGenerator.setStageValue(static_cast<EnvelopeGenerator::EnvelopeStage>(paramIdx), GetParam(paramIdx)->Value());
	  break;
  }
}

void Shibui::CreatePresets() {
}

void Shibui::processVirtualKeyboard() {
	IKeyboardControl* virtualKeyboard = (IKeyboardControl*)mVirtualKeyboard;
	int virtualKeyboardNoteNumber = virtualKeyboard->GetKey() + virtualKeyboardMinimumNoteNumber;

	if (lastVirtualKeyboardNoteNumber >= virtualKeyboardMinimumNoteNumber && virtualKeyboardNoteNumber != lastVirtualKeyboardNoteNumber) {
		// The note number has changed from a valid key to something else (valid key or nothing). Release the valid key:
		IMidiMsg midiMessage;
		midiMessage.MakeNoteOffMsg(lastVirtualKeyboardNoteNumber, 0);
		mMIDIReceiver.onMessageReceived(&midiMessage);
	}

	if (virtualKeyboardNoteNumber >= virtualKeyboardMinimumNoteNumber && virtualKeyboardNoteNumber != lastVirtualKeyboardNoteNumber) {
		// A valid key is pressed that wasn't pressed the previous call. Send a "note on" message to the MIDI receiver:
		IMidiMsg midiMessage;
		midiMessage.MakeNoteOnMsg(virtualKeyboardNoteNumber, virtualKeyboard->GetVelocity(), 0);
		mMIDIReceiver.onMessageReceived(&midiMessage);
	}

	lastVirtualKeyboardNoteNumber = virtualKeyboardNoteNumber;
}
