#include "undertow/sequencer.h"
#include "undertow/params.h"
#include <algorithm>
#include <cmath>

namespace undertow {

// --- defaultPattern ---
Pattern defaultPattern() {
    Pattern p;
    const int semi[] = {0,0,12,0,0,0,12,10,0,0,12,0,3,0,7,12};
    for (int i = 0; i < kSteps; ++i) {
        auto& s = p.steps[static_cast<std::size_t>(i)];
        s.semitones = semi[i];
        s.gate = (i != 5);
        s.accent = (i == 2 || i == 7 || i == 10 || i == 15);
        s.slide = (i == 6 || i == 14);
        s.probability = (i == 11) ? 0.5f : 1.0f;
    }
    return p;
}

// --- inScale ---
bool inScale(Scale scale, int semitones) {
    int d = semitones % 12;
    if (d < 0) d += 12;
    switch (scale) {
        case Scale::Chromatic: return true;
        case Scale::Minor: {
            static const bool deg[] = {1,0,1,1,0,1,0,1,1,0,1,0};
            return deg[d];
        }
        case Scale::Phrygian: {
            static const bool deg[] = {1,1,0,1,0,1,0,1,1,0,1,0};
            return deg[d];
        }
        case Scale::Dorian: {
            static const bool deg[] = {1,0,1,1,0,1,0,1,0,1,1,0};
            return deg[d];
        }
        case Scale::MinorPentatonic: {
            static const bool deg[] = {1,0,0,1,0,1,0,1,0,0,1,0};
            return deg[d];
        }
    }
    return false;
}

// --- Scale degree tables for mutate ---
static const int kChromaticDeg[] = {0,1,2,3,4,5,6,7,8,9,10,11};
static const int kMinorDeg[] = {0,2,3,5,7,8,10};
static const int kPhrygianDeg[] = {0,1,3,5,7,8,10};
static const int kDorianDeg[] = {0,2,3,5,7,9,10};
static const int kMinorPentDeg[] = {0,3,5,7,10};

struct ScaleInfo { const int* degrees; int count; };
static ScaleInfo getScaleInfo(Scale scale) {
    switch (scale) {
        case Scale::Chromatic: return {kChromaticDeg, 12};
        case Scale::Minor: return {kMinorDeg, 7};
        case Scale::Phrygian: return {kPhrygianDeg, 7};
        case Scale::Dorian: return {kDorianDeg, 7};
        case Scale::MinorPentatonic: return {kMinorPentDeg, 5};
    }
    return {kChromaticDeg, 12};
}

// --- Hash functions ---
// Deterministic hash for probability: same (seed, k) always gives the same result.
static float hashRandom(int seed, int k) {
    std::uint32_t h = static_cast<std::uint32_t>(seed);
    std::uint32_t x = static_cast<std::uint32_t>(k);
    h ^= x * 0xcc9e2d51u;
    h *= 0x1b873593u;
    h = (h << 13) | (h >> 19);
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    h *= 0x27d4eb2du;
    h ^= h >> 15;
    return static_cast<float>(h) / 4294967296.0f;
}

// Deterministic hash for mutate: depends on seed, mutation count, and step.
static std::uint32_t mutateHash(int seed, int mc, int step) {
    std::uint32_t h = static_cast<std::uint32_t>(seed) * 2654435761u
                     + static_cast<std::uint32_t>(mc) * 2246822519u
                     + static_cast<std::uint32_t>(step) * 3266489917u;
    h ^= h >> 16; h *= 0x85ebca6bu; h ^= h >> 13; h *= 0xc2b2ae35u; h ^= h >> 16;
    return h;
}

static float mutateRandom(int seed, int mc, int step) {
    return static_cast<float>(mutateHash(seed, mc, step)) / 4294967296.0f;
}

// Emit an event if there is room.
static void emit(SeqEvent* out, int& n, int max, int off,
                 SeqEvent::Type type, int note, bool accent) {
    if (n < max) { out[n] = {off, type, note, accent}; ++n; }
}

// --- Sequencer ---

Sequencer::Sequencer(Sequencer&& o) noexcept { *this = std::move(o); }
Sequencer& Sequencer::operator=(Sequencer&& o) noexcept {
    if (this != &o) {
        pattern_ = o.pattern_;
        sampleRate_ = o.sampleRate_; stepBeats_ = o.stepBeats_;
        seqLength_ = o.seqLength_; seqGate_ = o.seqGate_; seqSwing_ = o.seqSwing_;
        seqRoot_ = o.seqRoot_; seqScale_ = o.seqScale_; seqSeed_ = o.seqSeed_;
        seqEvolve_ = o.seqEvolve_; seqMutate_ = o.seqMutate_; transpose_ = o.transpose_;
        running_ = o.running_; lastPpq_ = o.lastPpq_; playing_ = o.playing_;
        playingNote_ = o.playingNote_; playingAccent_ = o.playingAccent_;
        playingStepK_ = o.playingStepK_; playingSlide_ = o.playingSlide_;
        currentStep_ = o.currentStep_; mutationCount_ = o.mutationCount_;
    }
    return *this;
}

void Sequencer::prepare(double sr) { sampleRate_ = sr; }

void Sequencer::reset() {
    running_ = false; lastPpq_ = 0.0; playing_ = false; playingNote_ = -1;
    playingAccent_ = false; playingStepK_ = -1; playingSlide_ = false;
    currentStep_ = -1; mutationCount_ = 0;
}

void Sequencer::setParams(const float* v) {
    switch (static_cast<int>(v[param::SeqRate])) {
        case 0: stepBeats_ = 0.5; break;
        case 1: stepBeats_ = 0.25; break;
        case 2: stepBeats_ = 0.125; break;
        case 3: stepBeats_ = 1.0 / 6.0; break;
        default: stepBeats_ = 0.25; break;
    }
    seqLength_ = std::clamp(static_cast<int>(v[param::SeqLength]), 1, 16);
    seqGate_ = v[param::SeqGate];
    seqSwing_ = v[param::SeqSwing];
    seqRoot_ = static_cast<int>(v[param::SeqRoot]);
    seqScale_ = static_cast<int>(v[param::SeqScale]);
    seqSeed_ = static_cast<int>(v[param::SeqSeed]);
    switch (static_cast<int>(v[param::SeqEvolve])) {
        case 0: seqEvolve_ = 0; break; case 1: seqEvolve_ = 1; break;
        case 2: seqEvolve_ = 2; break; case 3: seqEvolve_ = 4; break;
        case 4: seqEvolve_ = 8; break; case 5: seqEvolve_ = 16; break;
        default: seqEvolve_ = 0; break;
    }
    seqMutate_ = v[param::SeqMutate];
}

void Sequencer::setPattern(const Pattern& p) { pattern_ = p; }
const Pattern& Sequencer::pattern() const { return pattern_; }
void Sequencer::setTranspose(int s) { transpose_ = s; }

double Sequencer::stepBeat(int k) const {
    double b = static_cast<double>(k) * stepBeats_;
    if ((k & 1) && seqSwing_ > 0.0f)
        b += static_cast<double>(seqSwing_) / 3.0 * stepBeats_;
    return b;
}

int Sequencer::patternStep(int k) const {
    int l = seqLength_;
    int r = k % l;
    return r < 0 ? r + l : r;
}

bool Sequencer::stepPlays(int k) const {
    int ps = patternStep(k);
    const auto& s = pattern_.steps[static_cast<std::size_t>(ps)];
    return s.gate && hashRandom(seqSeed_, k) < s.probability;
}

int Sequencer::noteForStep(int k) const {
    int ps = patternStep(k);
    return std::clamp(seqRoot_ + transpose_ + pattern_.steps[static_cast<std::size_t>(ps)].semitones, 0, 127);
}

int Sequencer::process(double ppq, double bpm, bool running,
                       int numSamples, SeqEvent* out, int maxEvents) {
    int n = 0;

    // --- Not running: end any playing note and stop ---
    if (!running) {
        if (playing_) {
            emit(out, n, maxEvents, 0, SeqEvent::Type::NoteOff, playingNote_, false);
            playing_ = false;
        }
        currentStep_ = -1;
        running_ = false;
        lastPpq_ = ppq;
        return n;
    }

    const double bps = bpm / (60.0 * sampleRate_);  // beats per sample
    const double endPpq = ppq + static_cast<double>(numSamples) * bps;
    const double hsb = 0.5 * bps;  // half a sample in beats

    // --- Detect jump (start, loop, relocation) ---
    bool isJump = !running_ || std::fabs(ppq - lastPpq_) > hsb;
    if (isJump && playing_) {
        emit(out, n, maxEvents, 0, SeqEvent::Type::NoteOff, playingNote_, false);
        playing_ = false;
        playingNote_ = -1;
        playingStepK_ = -1;
        playingSlide_ = false;
    }
    running_ = true;

    // --- Find first step at or after ppq ---
    int k = 0;
    if (ppq > hsb)
        k = std::max(0, static_cast<int>(ppq / stepBeats_) - 2);
    while (stepBeat(k) < ppq - hsb)
        ++k;

    bool firstAfterJump = isJump;

    for (; ; ++k) {
        double sb = stepBeat(k);
        if (sb >= endPpq - hsb) break;

        int so = static_cast<int>(std::round((sb - ppq) / bps));
        if (so < 0) so = 0;
        if (so >= numSamples) break;

        int ps = patternStep(k);
        currentStep_ = ps;

        // --- Evolve at bar boundaries (skip the first step after a jump) ---
        if (!firstAfterJump && seqEvolve_ > 0 && seqMutate_ > 0.0f) {
            double bib = std::fmod(sb, 4.0);
            if (sb > hsb && (bib < hsb || bib > 4.0 - hsb)) {
                int bar = static_cast<int>(std::round(sb / 4.0));
                if (bar > 0 && (bar % seqEvolve_) == 0)
                    mutate(seqMutate_);
            }
        }
        firstAfterJump = false;

        // --- Handle the currently playing note ending at or before this step ---
        if (playing_) {
            if (playingSlide_) {
                // A slide note ends at the start of the next step.
                double slideEnd = stepBeat(playingStepK_ + 1);
                if (slideEnd <= sb + hsb) {
                    // The slide ends at (or before) this step.
                    if (stepPlays(k)) {
                        // Legato: new note-on before the slide's note-off.
                        int note = noteForStep(k);
                        bool acc = pattern_.steps[static_cast<std::size_t>(ps)].accent;
                        emit(out, n, maxEvents, so, SeqEvent::Type::NoteOn, note, acc);
                        emit(out, n, maxEvents, so, SeqEvent::Type::NoteOff, playingNote_, false);
                        playing_ = true;
                        playingNote_ = note;
                        playingAccent_ = acc;
                        playingStepK_ = k;
                        playingSlide_ = pattern_.steps[static_cast<std::size_t>(ps)].slide;
                    } else {
                        // Slide into a rest or a step that doesn't play.
                        emit(out, n, maxEvents, so, SeqEvent::Type::NoteOff, playingNote_, false);
                        playing_ = false;
                        playingNote_ = -1;
                        playingStepK_ = -1;
                        playingSlide_ = false;
                    }
                    continue;  // This step has been handled.
                }
                // The slide hasn't ended yet; the playing note covers this step.
                // Don't start a new note while the slide is still active.
                continue;
            } else {
                // Non-slide: note ends at startBeat + gate * stepBeats.
                double noteEnd = stepBeat(playingStepK_) + static_cast<double>(seqGate_) * stepBeats_;
                if (noteEnd <= sb + hsb) {
                    // The note ended at or before this step.
                    int neSo = static_cast<int>(std::round((noteEnd - ppq) / bps));
                    neSo = std::clamp(neSo, 0, numSamples - 1);
                    emit(out, n, maxEvents, neSo, SeqEvent::Type::NoteOff, playingNote_, false);
                    playing_ = false;
                    playingNote_ = -1;
                    playingStepK_ = -1;
                    playingSlide_ = false;
                }
                // If the note hasn't ended yet, it's still playing; the new step
                // starts on top. This shouldn't normally happen (gate < 1 means
                // the note ends within the step), but if it does, the old note
                // keeps playing and we skip starting a new one.
                if (playing_) continue;
            }
        }

        // --- Play this step ---
        if (stepPlays(k)) {
            int note = noteForStep(k);
            bool acc = pattern_.steps[static_cast<std::size_t>(ps)].accent;
            emit(out, n, maxEvents, so, SeqEvent::Type::NoteOn, note, acc);
            playing_ = true;
            playingNote_ = note;
            playingAccent_ = acc;
            playingStepK_ = k;
            playingSlide_ = pattern_.steps[static_cast<std::size_t>(ps)].slide;
        }
    }

    // --- After all steps, check if the playing note ends within the block ---
    if (playing_ && !playingSlide_) {
        double noteEnd = stepBeat(playingStepK_) + static_cast<double>(seqGate_) * stepBeats_;
        if (noteEnd < endPpq + hsb) {
            int neSo = static_cast<int>(std::round((noteEnd - ppq) / bps));
            neSo = std::clamp(neSo, 0, numSamples - 1);
            emit(out, n, maxEvents, neSo, SeqEvent::Type::NoteOff, playingNote_, false);
            playing_ = false;
            playingNote_ = -1;
            playingStepK_ = -1;
            playingSlide_ = false;
        }
    }

    lastPpq_ = endPpq;
    return n;
}

int Sequencer::currentStep() const { return currentStep_; }

void Sequencer::mutate(float amount) {
    Scale scale = static_cast<Scale>(seqScale_);
    ScaleInfo info = getScaleInfo(scale);

    // Build the list of valid semitones within [-12, 12] that are in the scale.
    int valid[25];
    int numValid = 0;
    for (int oct = -1; oct <= 1; ++oct) {
        for (int d = 0; d < info.count; ++d) {
            int s = oct * 12 + info.degrees[d];
            if (s >= -12 && s <= 12)
                valid[numValid++] = s;
        }
    }

    for (int i = 0; i < kSteps; ++i) {
        float r = mutateRandom(seqSeed_, mutationCount_, i);
        if (r >= amount) continue;

        float t = mutateRandom(seqSeed_, mutationCount_ + 10000, i);
        auto& s = pattern_.steps[static_cast<std::size_t>(i)];
        if (t < 0.5f) {
            // New note: pick a random valid semitones value.
            float nr = mutateRandom(seqSeed_, mutationCount_ + 20000, i);
            int idx = static_cast<int>(nr * static_cast<float>(numValid));
            if (idx >= numValid) idx = numValid - 1;
            s.semitones = valid[idx];
        } else if (t < 0.75f) {
            s.accent = !s.accent;
        } else {
            s.slide = !s.slide;
        }
    }
    ++mutationCount_;
}

}  // namespace undertow
