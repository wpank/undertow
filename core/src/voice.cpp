#include "undertow/voice.h"

#include <algorithm>
#include <cmath>

namespace undertow {

namespace {

constexpr double kLog2 = 0.6931471805599453;

inline double midiNoteHz(double semitones) {
    return 440.0 * std::exp2((semitones - 69.0) / 12.0);
}

} // namespace

void MonoVoice::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    osc_.prepare(sampleRate);
    sub_.prepare(sampleRate);
    filter_.prepare(sampleRate);
    ampEnv_.prepare(sampleRate);
    filtEnv_.prepare(sampleRate);

    // Smoother: ~5 ms time constant
    const double coeff = 1.0 - std::exp(-1.0 / (0.005 * sampleRate));
    oscLevelSmooth_.coeff = coeff;
    subLevelSmooth_.coeff = coeff;
    oscPwSmooth_.coeff = coeff;
    filterResSmooth_.coeff = coeff;
    filterDriveSmooth_.coeff = coeff;
    accentGainSmooth_.coeff = coeff;
    log2CutoffSmooth_.coeff = coeff;

    prepared_ = true;
    firstSetParams_ = true;
    reset();
}

void MonoVoice::reset() {
    osc_.reset(0.0);
    sub_.reset(0.0);
    filter_.reset();
    ampEnv_.reset();
    filtEnv_.reset();

    numHeld_ = 0;
    currentNote_ = -1;
    isAccent_ = false;
    accentGain_ = 1.0f;
    accentOctaves_ = 0.0;
    pitchBend_ = 0.0f;
    gliding_ = false;
    glidePos_ = 0.0;
    glideFromNote_ = 0.0;
    glideToNote_ = 0.0;
    noteHz_ = 0.0;
    playedHz_ = 0.0;
    params_ = nullptr;
    bendRange_ = 2;
    glideMode_ = 1;
    glideMs_ = 50.0;

    // Reset smoothers to safe defaults
    oscLevelSmooth_.init(0.0);
    subLevelSmooth_.init(0.0);
    oscPwSmooth_.init(0.5);
    filterResSmooth_.init(0.0);
    filterDriveSmooth_.init(0.0);
    accentGainSmooth_.init(1.0);
    log2CutoffSmooth_.init(std::log2(600.0));
}

void MonoVoice::setParams(const float* v) {
    params_ = v;

    // Cache frequently-used params
    bendRange_ = static_cast<int>(v[param::BendRange]);
    glideMode_ = static_cast<int>(v[param::GlideMode]);
    glideMs_ = static_cast<double>(v[param::GlideMs]);

    // Accent gain: 1 + accent if accented, else 1
    const double accent = static_cast<double>(v[param::Accent]);
    accentGain_ = isAccent_ ? static_cast<float>(1.0 + accent) : 1.0f;
    accentOctaves_ = isAccent_ ? 2.0 * accent : 0.0;

    // Oscillator wave
    osc_.setWave(static_cast<Wave>(static_cast<int>(v[param::OscWave])));

    // Smoothers
    if (firstSetParams_) {
        oscLevelSmooth_.init(static_cast<double>(v[param::OscLevel]));
        subLevelSmooth_.init(static_cast<double>(v[param::SubLevel]));
        oscPwSmooth_.init(static_cast<double>(v[param::OscPw]));
        filterResSmooth_.init(static_cast<double>(v[param::FilterRes]));
        filterDriveSmooth_.init(static_cast<double>(v[param::FilterDrive]));
        accentGainSmooth_.init(static_cast<double>(accentGain_));
        log2CutoffSmooth_.init(std::log2(std::max(static_cast<double>(v[param::FilterCutoff]), 20.0)));
        firstSetParams_ = false;
    } else {
        oscLevelSmooth_.setTarget(static_cast<double>(v[param::OscLevel]));
        subLevelSmooth_.setTarget(static_cast<double>(v[param::SubLevel]));
        oscPwSmooth_.setTarget(static_cast<double>(v[param::OscPw]));
        filterResSmooth_.setTarget(static_cast<double>(v[param::FilterRes]));
        filterDriveSmooth_.setTarget(static_cast<double>(v[param::FilterDrive]));
        accentGainSmooth_.setTarget(static_cast<double>(accentGain_));
        log2CutoffSmooth_.setTarget(std::log2(std::max(static_cast<double>(v[param::FilterCutoff]), 20.0)));
    }

    // Envelopes
    ampEnv_.setAttackMs(static_cast<double>(v[param::AmpAttack]));
    ampEnv_.setDecayMs(static_cast<double>(v[param::AmpDecay]));
    ampEnv_.setSustain(static_cast<double>(v[param::AmpSustain]));
    ampEnv_.setReleaseMs(static_cast<double>(v[param::AmpRelease]));

    filtEnv_.setAttackMs(static_cast<double>(v[param::FiltAttack]));
    filtEnv_.setDecayMs(static_cast<double>(v[param::FiltDecay]));
    filtEnv_.setSustain(static_cast<double>(v[param::FiltSustain]));
    filtEnv_.setReleaseMs(static_cast<double>(v[param::FiltRelease]));
}

void MonoVoice::setModulation(const VoiceModulation& modulation) {
    mod_ = modulation;
}

void MonoVoice::noteOn(int note, float velocity) {
    const bool wasActive = isActive();
    const bool isLegato = (numHeld_ > 0);

    // Add to held notes (last-note priority: newest is last)
    if (numHeld_ < 16)
        heldNotes_[numHeld_++] = note;

    // Accent: velocity >= 100/127
    isAccent_ = (velocity >= 100.0f / 127.0f);

    // Update accent gain immediately if params available
    if (params_) {
        const double accent = static_cast<double>(params_[param::Accent]);
        accentGain_ = isAccent_ ? static_cast<float>(1.0 + accent) : 1.0f;
        accentOctaves_ = isAccent_ ? 2.0 * accent : 0.0;
        accentGainSmooth_.setTarget(static_cast<double>(accentGain_));
    }

    // Glide setup
    if (isLegato) {
        // Legato note: glide from current pitch to new note, no envelope restart
        const double prevPitch = gliding_ ? glideFromNote_ + glidePos_ * (glideToNote_ - glideFromNote_)
                                          : static_cast<double>(currentNote_);
        glideFromNote_ = prevPitch;
        glideToNote_ = static_cast<double>(note);

        if (glideMode_ == 0 || glideMs_ <= 0.0) {
            // Off: jump immediately
            gliding_ = false;
        } else {
            // Legato or Always: glide
            gliding_ = (std::fabs(glideToNote_ - glideFromNote_) > 1e-6);
            glidePos_ = 0.0;
        }
    } else {
        // No overlap: retrigger envelopes
        if (!wasActive) {
            // Note from silence: reset oscillators to phase 0
            osc_.reset(0.0);
            sub_.reset(0.0);
            filter_.reset();
        }

        ampEnv_.noteOn();
        filtEnv_.noteOn();

        if (glideMode_ == 2 && wasActive && glideMs_ > 0.0) {
            // Always mode: glide even detached notes while still sounding
            const double prevPitch = gliding_ ? glideFromNote_ + glidePos_ * (glideToNote_ - glideFromNote_)
                                              : static_cast<double>(currentNote_);
            glideFromNote_ = prevPitch;
            glideToNote_ = static_cast<double>(note);
            gliding_ = (std::fabs(glideToNote_ - glideFromNote_) > 1e-6);
            glidePos_ = 0.0;
        } else {
            glideFromNote_ = static_cast<double>(note);
            glideToNote_ = static_cast<double>(note);
            gliding_ = false;
            glidePos_ = 1.0;
        }
    }

    currentNote_ = note;
    noteHz_ = midiNoteHz(static_cast<double>(note));
    recomputePitch();
}

void MonoVoice::noteOff(int note) {
    // Remove from held notes
    for (int i = numHeld_ - 1; i >= 0; --i) {
        if (heldNotes_[i] == note) {
            for (int j = i; j < numHeld_ - 1; ++j)
                heldNotes_[j] = heldNotes_[j + 1];
            --numHeld_;
            break;
        }
    }

    if (numHeld_ > 0) {
        // Return to the newest held note (last in array)
        const double prevPitch = gliding_ ? glideFromNote_ + glidePos_ * (glideToNote_ - glideFromNote_)
                                          : static_cast<double>(currentNote_);
        currentNote_ = heldNotes_[numHeld_ - 1];
        glideFromNote_ = prevPitch;
        glideToNote_ = static_cast<double>(currentNote_);

        // Legato return always glides if glide mode allows
        if (glideMode_ >= 1 && glideMs_ > 0.0) {
            gliding_ = (std::fabs(glideToNote_ - glideFromNote_) > 1e-6);
            glidePos_ = 0.0;
        } else {
            gliding_ = false;
        }

        noteHz_ = midiNoteHz(static_cast<double>(currentNote_));
        recomputePitch();
    } else {
        // Last note released
        ampEnv_.noteOff();
        filtEnv_.noteOff();
        // Keep currentNote_ during release
    }
}

void MonoVoice::allNotesOff() {
    numHeld_ = 0;
    ampEnv_.noteOff();
    filtEnv_.noteOff();
}

void MonoVoice::setPitchBend(float amount) {
    pitchBend_ = std::clamp(amount, -1.0f, 1.0f);
}

float MonoVoice::process() {
    // Advance glide
    if (gliding_) {
        const double glideSamples = glideMs_ * sampleRate_ / 1000.0;
        const double inc = 1.0 / glideSamples;
        glidePos_ += inc;
        if (glidePos_ >= 1.0) {
            glidePos_ = 1.0;
            gliding_ = false;
        }
    }

    // Compute current pitch in semitones (with glide and bend)
    const double glideNote = glideFromNote_ + glidePos_ * (glideToNote_ - glideFromNote_);
    const double bendSemitones = static_cast<double>(pitchBend_) * static_cast<double>(bendRange_);
    playedHz_ = midiNoteHz(glideNote + bendSemitones + mod_.pitchSemitones);
    noteHz_ = midiNoteHz(glideNote); // without bend or modulation, for currentHz()

    // Set oscillator and sub frequencies
    if (params_) {
        const double oscOctave = static_cast<double>(static_cast<int>(params_[param::OscOctave]));
        const double oscFine = static_cast<double>(params_[param::OscFine]) / 1200.0;
        const double oscMult = std::exp2(oscOctave + oscFine);
        osc_.setFrequency(playedHz_ * oscMult);

        const int subOct = static_cast<int>(params_[param::SubOctave]);
        const double subDiv = (subOct == 0) ? 2.0 : 4.0;
        sub_.setFrequency(playedHz_ / subDiv);
    }

    // Tick smoothers
    oscLevelSmooth_.tick();
    subLevelSmooth_.tick();
    oscPwSmooth_.tick();
    filterResSmooth_.tick();
    filterDriveSmooth_.tick();
    accentGainSmooth_.tick();
    log2CutoffSmooth_.tick();

    // Set pulse width from smoother + modulation
    double pw = oscPwSmooth_.value() + mod_.pulseWidth;
    pw = std::clamp(pw, 0.05, 0.95);
    osc_.setPulseWidth(pw);

    // If voice is fully idle, output zero
    if (ampEnv_.stage() == Envelope::Stage::Idle && !gliding_) {
        return 0.0f;
    }

    // Render oscillator and sub
    const float oscSample = osc_.process();
    const float subSample = sub_.process();

    // Apply osc_level before filter
    const double oscLevel = oscLevelSmooth_.value();
    float oscFiltered = filter_.process(static_cast<float>(oscSample * oscLevel));

    // Update filter coefficients from smoothers + modulation
    filter_.setResonance(std::clamp(filterResSmooth_.value() + mod_.resonance, 0.0, 1.0));
    filter_.setDrive(filterDriveSmooth_.value());

    // Update filter cutoff from smoothed log2 cutoff + env + accent + keytrack
    if (params_) {
        const double filtEnvLevel = static_cast<double>(filtEnv_.level());
        const double filterEnv = static_cast<double>(params_[param::FilterEnv]);
        const double filterTrack = static_cast<double>(params_[param::FilterTrack]);

        const double log2Cutoff = log2CutoffSmooth_.value();
        double totalLog2Cutoff = log2Cutoff;
        totalLog2Cutoff += filterEnv * filtEnvLevel;  // filter_env in octaves * level = octaves
        totalLog2Cutoff += accentOctaves_ * filtEnvLevel; // accent adds 2*accent octaves of filter env
        totalLog2Cutoff += filterTrack * (glideNote - 36.0) / 12.0; // key track in octaves
        totalLog2Cutoff += mod_.cutoffOctaves; // LFO/voice modulation

        const double cutoffHz = std::exp2(totalLog2Cutoff);
        filter_.setCutoff(cutoffHz);
    }

    // Mix: filter(osc * osc_level) + sub * sub_level
    const double subLevel = subLevelSmooth_.value();
    const double mix = static_cast<double>(oscFiltered) + static_cast<double>(subSample) * subLevel;

    // Apply amp envelope and accent gain
    const float ampEnvSample = ampEnv_.process();
    const float filtEnvSample = filtEnv_.process();
    const double accentGain = accentGainSmooth_.value();
    const float output = static_cast<float>(mix * static_cast<double>(ampEnvSample) * accentGain * mod_.gain);

    return output;
}

bool MonoVoice::isActive() const {
    return ampEnv_.stage() != Envelope::Stage::Idle;
}

int MonoVoice::currentNote() const {
    if (ampEnv_.stage() == Envelope::Stage::Idle && !gliding_)
        return -1;
    return currentNote_;
}

double MonoVoice::currentHz() const {
    if (ampEnv_.stage() == Envelope::Stage::Idle && !gliding_)
        return 0.0;
    // Include bend per spec: "glide and bend, without osc_octave/osc_fine"
    const double bendSemitones = static_cast<double>(pitchBend_) * static_cast<double>(bendRange_);
    return noteHz_ * std::exp2(bendSemitones / 12.0);
}

void MonoVoice::recomputePitch() {
    const double glideNote = glideFromNote_ + glidePos_ * (glideToNote_ - glideFromNote_);
    const double bendSemitones = static_cast<double>(pitchBend_) * static_cast<double>(bendRange_);
    playedHz_ = midiNoteHz(glideNote + bendSemitones);
    noteHz_ = midiNoteHz(glideNote);

    if (params_) {
        const double oscOctave = static_cast<double>(static_cast<int>(params_[param::OscOctave]));
        const double oscFine = static_cast<double>(params_[param::OscFine]) / 1200.0;
        osc_.setFrequency(playedHz_ * std::exp2(oscOctave + oscFine));

        const int subOct = static_cast<int>(params_[param::SubOctave]);
        const double subDiv = (subOct == 0) ? 2.0 : 4.0;
        sub_.setFrequency(playedHz_ / subDiv);
    }
}

void MonoVoice::updateFilterCutoff(double filtEnvLevel) {
    (void)filtEnvLevel;
}

}  // namespace undertow
