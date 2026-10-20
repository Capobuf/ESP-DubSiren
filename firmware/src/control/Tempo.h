#pragma once

#include <math.h>
#include <stdint.h>

enum class TempoDivision : uint8_t { Quarter, Eighth, DottedEighth, TripletEighth, Count };

inline const char *tempoDivisionName(TempoDivision division) {
    static const char *names[] = {"1/4", "1/8", "1/8D", "1/8T"};
    const unsigned index = static_cast<unsigned>(division);
    return index < 4 ? names[index] : "N/A";
}

struct TempoState {
    float bpm = 120.0f;
    bool known = false;
    bool echoSync = false;
    bool lfoSync = false;
    TempoDivision echoDivision = TempoDivision::Quarter;
    TempoDivision lfoDivision = TempoDivision::Quarter;
    // Keep the last usable timing when a requested division is unavailable.
    float echoMs = 360.0f;
    float lfoHz = 0.70f;
};

inline float tempoPeriodMs(const TempoState &tempo, TempoDivision division) {
    static constexpr float beats[] = {1.0f, 0.5f, 0.75f, 1.0f / 3.0f};
    const unsigned index = static_cast<unsigned>(division);
    return tempo.known && index < 4 ? 60000.0f / tempo.bpm * beats[index] : 0.0f;
}

inline bool tempoEchoAvailable(const TempoState &tempo) {
    const float ms = tempoPeriodMs(tempo, tempo.echoDivision);
    return ms >= 50.0f && ms <= 1000.0f;
}

inline bool tempoLfoAvailable(const TempoState &tempo) {
    const float ms = tempoPeriodMs(tempo, tempo.lfoDivision);
    return ms >= 50.0f && ms <= 20000.0f;
}

inline void updateTempoTiming(TempoState &tempo) {
    if (tempoEchoAvailable(tempo)) tempo.echoMs = tempoPeriodMs(tempo, tempo.echoDivision);
    if (tempoLfoAvailable(tempo)) tempo.lfoHz = 1000.0f / tempoPeriodMs(tempo, tempo.lfoDivision);
}
