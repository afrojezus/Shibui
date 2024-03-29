#pragma once
#include "PolyBLEPOscillator.h"
#include "EnvelopeGenerator.h"
#include "Filter.h"

class Voice
{
public:
	friend class VoiceManager;
	void reset();
	Voice()
		: mNoteNumber(-1),
		mVelocity(0),
		mFilterEnvelopeAmount(0.0),
		mFilterLFOAmount(0.0),
		mOscillatorOnePitchAmount(0.0),
		mOscillatorTwoPitchAmount(0.0),
		mOscillatorMix(0.5),
		mLFOValue(0.0),
		isActive(false) {
		// Set myself free everytime my volume envelope has fully faded out of RELEASE stage:
		mVolumeEnvelope.finishedEnvelopeCycle.Connect(this, &Voice::setFree);
	};

	inline void setFilterEnvelopeAmount(double amount) { mFilterEnvelopeAmount = amount; }
	inline void setFilterLFOAmount(double amount) { mFilterLFOAmount = amount; }
	inline void setOscillatorOnePitchAmount(double amount) { mOscillatorOnePitchAmount = amount; }
	inline void setOscillatorTwoPitchAmount(double amount) { mOscillatorTwoPitchAmount = amount; }
	inline void setOscillatorMix(double mix) { mOscillatorMix = mix; }
	inline void setLFOValue(double value) { mLFOValue = value; }

	inline void setNoteNumber(int noteNumber) {
		mNoteNumber = noteNumber;
		double frequency = 440.0 * pow(2.0, (mNoteNumber - 69.0) / 12.0);
		mOscillatorOne.setFrequency(frequency);
		mOscillatorTwo.setFrequency(frequency);
	}
	double nextSample();
	void setFree();

private:
	PolyBLEPOscillator mOscillatorOne;
	PolyBLEPOscillator mOscillatorTwo;
	EnvelopeGenerator mVolumeEnvelope;
	EnvelopeGenerator mFilterEnvelope;
	Filter mFilter;
	int mNoteNumber;
	int mVelocity;
	double mFilterEnvelopeAmount;
	double mOscillatorMix;
	double mFilterLFOAmount;
	double mOscillatorOnePitchAmount;
	double mOscillatorTwoPitchAmount;
	double mLFOValue;
	bool isActive;
};

