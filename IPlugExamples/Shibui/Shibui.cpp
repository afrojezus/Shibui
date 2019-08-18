#include "Shibui.h"
#pragma warning( suppress : 4101 4129 )
#include "IPlug_include_in_plug_src.h"
#include "IControl.h"
#include "resource.h"
#include "IKeyboardControl.h"

#include <math.h>
#include <algorithm>

const int kNumPrograms = 5;
const double parameterStep = 0.001;

enum EParams
{
	// Oscillator Section:
	mOsc1Waveform = 0,
	mOsc1PitchMod,
	mOsc2Waveform,
	mOsc2PitchMod,
	mOscMix,
	// Filter Section:
	mFilterMode,
	mFilterCutoff,
	mFilterResonance,
	mFilterLfoAmount,
	mFilterEnvAmount,
	// LFO:
	mLFOWaveform,
	mLFOFrequency,
	// Volume Envelope:
	mVolumeEnvAttack,
	mVolumeEnvDecay,
	mVolumeEnvSustain,
	mVolumeEnvRelease,
	// Filter Envelope:
	mFilterEnvAttack,
	mFilterEnvDecay,
	mFilterEnvSustain,
	mFilterEnvRelease,
	kNumParams
};

// God fucking bless.
const parameterProperties_struct parameterProperties[kNumParams] = {
  {.name = "Osc 1 Waveform",.x = 30,.y = 75},
  {.name = "Osc 1 Pitch Mod",.x = 69,.y = 61,.defaultVal = 0.0,.minVal = 0.0,.maxVal = 1.0},
  {.name = "Osc 2 Waveform",.x = 203,.y = 75},
  {.name = "Osc 2 Pitch Mod",.x = 242,.y = 61,.defaultVal = 0.0,.minVal = 0.0,.maxVal = 1.0},
  {.name = "Osc Mix",.x = 130,.y = 61,.defaultVal = 0.5,.minVal = 0.0,.maxVal = 1.0},
  {.name = "Filter Mode",.x = 30,.y = 188},
  {.name = "Filter Cutoff",.x = 69,.y = 174,.defaultVal = 0.99,.minVal = 0.0,.maxVal = 0.99},
  {.name = "Filter Resonance",.x = 124,.y = 174,.defaultVal = 0.0,.minVal = 0.0,.maxVal = 1.0},
  {.name = "Filter LFO Amount",.x = 179,.y = 174,.defaultVal = 0.0,.minVal = 0.0,.maxVal = 1.0},
  {.name = "Filter Envelope Amount",.x = 234,.y = 174,.defaultVal = 0.0,.minVal = -1.0,.maxVal = 1.0},
  {.name = "LFO Waveform",.x = 30,.y = 298},
  {.name = "LFO Frequency",.x = 69,.y = 284,.defaultVal = 6.0,.minVal = 0.01,.maxVal = 30.0},
  {.name = "Volume Env Attack",.x = 323,.y = 61,.defaultVal = 0.01,.minVal = 0.01,.maxVal = 10.0},
  {.name = "Volume Env Decay",.x = 378,.y = 61,.defaultVal = 0.5,.minVal = 0.01,.maxVal = 15.0},
  {.name = "Volume Env Sustain",.x = 433,.y = 61,.defaultVal = 0.1,.minVal = 0.001,.maxVal = 1.0},
  {.name = "Volume Env Release",.x = 488,.y = 61,.defaultVal = 1.0,.minVal = 0.01,.maxVal = 15.0},
  {.name = "Filter Env Attack",.x = 323,.y = 174,.defaultVal = 0.01,.minVal = 0.01,.maxVal = 10.0},
  {.name = "Filter Env Decay",.x = 378,.y = 174,.defaultVal = 0.5,.minVal = 0.01,.maxVal = 15.0},
  {.name = "Filter Env Sustain",.x = 433,.y = 174,.defaultVal = 0.1,.minVal = 0.001,.maxVal = 1.0},
  {.name = "Filter Env Release",.x = 488,.y = 174,.defaultVal = 1.0,.minVal = 0.01,.maxVal = 15.0}
};

typedef struct {
	const char* name;
	const int x;
	const int y;
	const double defaultVal;
	const double minVal;
	const double maxVal;
} parameterProperties_struct;

enum ELayout
{
	kWidth = GUI_WIDTH,
	kHeight = GUI_HEIGHT,
	kKeybX = 62,
	kKeybY = 425
};

Shibui::Shibui(IPlugInstanceInfo instanceInfo)
  :	IPLUG_CTOR(kNumParams, kNumPrograms, instanceInfo), lastVirtualKeyboardNoteNumber(virtualKeyboardMinimumNoteNumber - 1), filterEnvelopeAmount(0.0), lfoFilterModAmount(0.1)
{
  TRACE;
  CreateParams();
  CreateGraphics();
  CreatePresets();

  mMIDIReceiver.noteOn.Connect(this, &Shibui::onNoteOn);
  mMIDIReceiver.noteOff.Connect(this, &Shibui::onNoteOff);
  mEnvelopeGenerator.beganEnvelopeCycle.Connect(this, &Shibui::onBeganEnvelopeCycle);
  mEnvelopeGenerator.finishedEnvelopeCycle.Connect(this, &Shibui::onFinishedEnvelopeCycle);
}

void Shibui::CreateParams() {
	for (int i = 0; i < kNumParams; i++) {
		IParam* param = GetParam(i);
		const parameterProperties_struct& properties = parameterProperties[i];
		switch (i) {
			// Enum Parameters:
		case mOsc1Waveform:
		case mOsc2Waveform:
			param->InitEnum(properties.name,
				Oscillator::OSCILLATOR_MODE_SAW,
				Oscillator::kNumOscillatorModes);
			// For VST3:
			param->SetDisplayText(0, properties.name);
			break;
		case mLFOWaveform:
			param->InitEnum(properties.name,
				Oscillator::OSCILLATOR_MODE_TRIANGLE,
				Oscillator::kNumOscillatorModes);
			// For VST3:
			param->SetDisplayText(0, properties.name);
			break;
		case mFilterMode:
			param->InitEnum(properties.name,
				Filter::FILTER_MODE_LOWPASS,
				Filter::kNumFilterModes);
			break;
			// Double Parameters:
		default:
			param->InitDouble(properties.name,
				properties.defaultVal,
				properties.minVal,
				properties.maxVal,
				parameterStep);
			break;
		}
	}
	GetParam(mFilterCutoff)->SetShape(2);
	GetParam(mVolumeEnvAttack)->SetShape(3);
	GetParam(mFilterEnvAttack)->SetShape(3);
	GetParam(mVolumeEnvDecay)->SetShape(3);
	GetParam(mFilterEnvDecay)->SetShape(3);
	GetParam(mVolumeEnvSustain)->SetShape(2);
	GetParam(mFilterEnvSustain)->SetShape(2);
	GetParam(mVolumeEnvRelease)->SetShape(3);
	GetParam(mFilterEnvRelease)->SetShape(3);
	for (int i = 0; i < kNumParams; i++) {
		OnParamChange(i);
	}
}

void Shibui::CreateGraphics() {
	IGraphics* pGraphics = MakeGraphics(this, kWidth, kHeight);
	pGraphics->AttachBackground(BG_ID, BG_FN);

	IBitmap whiteKeyImage = pGraphics->LoadIBitmap(WHITE_KEY_ID, WHITE_KEY_FN, 6);
	IBitmap blackKeyImage = pGraphics->LoadIBitmap(BLACK_KEY_ID, BLACK_KEY_FN);
	//                            C#     D#          F#      G#      A#
	int keyCoordinates[12] = { 0, 10, 17, 30, 35, 52, 61, 68, 79, 85, 97, 102 };
	mVirtualKeyboard = new IKeyboardControl(this, kKeybX, kKeybY, virtualKeyboardMinimumNoteNumber, /* octaves: */ 4, &whiteKeyImage, &blackKeyImage, keyCoordinates);
	pGraphics->AttachControl(mVirtualKeyboard);

	IBitmap waveformBitmap = pGraphics->LoadIBitmap(WAVEFORM_ID, WAVEFORM_FN, 4);
	IBitmap filterModeBitmap = pGraphics->LoadIBitmap(FILTERMODE_ID, FILTERMODE_FN, 3);
	IBitmap knobBitmap = pGraphics->LoadIBitmap(KNOB_ID, KNOB_FN, 64);

	for (int i = 0; i < kNumParams; i++) {
		const parameterProperties_struct& properties = parameterProperties[i];
		IControl* control;
		IBitmap* graphic;
		switch (i) {
			// Switches:
		case mOsc1Waveform:
		case mOsc2Waveform:
		case mLFOWaveform:
			graphic = &waveformBitmap;
			control = new ISwitchControl(this, properties.x, properties.y, i, graphic);
			break;
		case mFilterMode:
			graphic = &filterModeBitmap;
			control = new ISwitchControl(this, properties.x, properties.y, i, graphic);
			break;
			// Knobs:
		default:
			graphic = &knobBitmap;
			control = new IKnobMultiControl(this, properties.x, properties.y, i, graphic);
			break;
		}
		pGraphics->AttachControl(control);
	}

	AttachGraphics(pGraphics);
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
		double lfoFilterModulation = mLFO.nextSample() * lfoFilterModAmount;
		mOscillator.setFrequency(mMIDIReceiver.getLastFrequency());
		mFilter.setCutoffMod((mFilterEnvelopeGenerator.nextSample() * filterEnvelopeAmount) + lfoFilterModulation);
		leftOutput[i] = rightOutput[i] = mFilter.process(mOscillator.nextSample() * mEnvelopeGenerator.nextSample() * velocity / 127.0);
	}

	mMIDIReceiver.Flush(nFrames);
}

void Shibui::Reset()
{
  TRACE;
  IMutexLock lock(this);
  mOscillator.setSampleRate(GetSampleRate());
  mEnvelopeGenerator.setSampleRate(GetSampleRate());
  mFilterEnvelopeGenerator.setSampleRate(GetSampleRate());
  mLFO.setSampleRate(GetSampleRate());
}

void Shibui::OnParamChange(int paramIdx)
{
  IMutexLock lock(this);

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
