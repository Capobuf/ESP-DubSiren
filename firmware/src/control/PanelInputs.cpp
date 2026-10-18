#include "PanelInputs.h"

#if defined(DUBSIREN_PHYSICAL_PANEL) && DUBSIREN_PHYSICAL_PANEL
#include "driver/pcnt.h"

namespace {
constexpr int kAdc = 4;
constexpr int kSelect[] = {5, 6, 10};
constexpr int kButtons[] = {38, 39, 40, 41, 42};
constexpr int kEncoderA = 1, kEncoderB = 2;
constexpr uint32_t kMuxSettleUs = 6000;
constexpr uint32_t kDebounceUs = 2000;
}

void PanelInputs::selectChannel(unsigned channel) {
    channel_ = channel;
    for (unsigned bit = 0; bit < 3; ++bit) digitalWrite(kSelect[bit], (channel >> bit) & 1);
    selectedAt_ = micros();
}

bool PanelInputs::begin() {
    for (int pin : kSelect) pinMode(pin, OUTPUT);
    for (unsigned i = 0; i < 5; ++i) {
        pinMode(kButtons[i], INPUT_PULLUP);
        buttons_[i].raw = buttons_[i].stable = digitalRead(kButtons[i]) == LOW;
        buttons_[i].armed = !buttons_[i].raw;
    }
    pinMode(kEncoderA, INPUT_PULLUP); pinMode(kEncoderB, INPUT_PULLUP);
    analogReadResolution(12);
    analogSetPinAttenuation(kAdc, ADC_11db);
    if (!adcAttachPin(kAdc)) return false;
    for (unsigned channel = 0; channel < 2; ++channel) {
        pcnt_config_t config = {};
        config.pulse_gpio_num = channel == 0 ? kEncoderA : kEncoderB;
        config.ctrl_gpio_num = channel == 0 ? kEncoderB : kEncoderA;
        config.channel = channel == 0 ? PCNT_CHANNEL_0 : PCNT_CHANNEL_1;
        config.unit = PCNT_UNIT_0;
        config.pos_mode = channel == 0 ? PCNT_COUNT_INC : PCNT_COUNT_DEC;
        config.neg_mode = channel == 0 ? PCNT_COUNT_DEC : PCNT_COUNT_INC;
        config.lctrl_mode = PCNT_MODE_REVERSE;
        config.hctrl_mode = PCNT_MODE_KEEP;
        config.counter_h_lim = 32767; config.counter_l_lim = -32768;
        if (pcnt_unit_config(&config) != ESP_OK) return false;
    }
    if (pcnt_set_filter_value(PCNT_UNIT_0, 1000) != ESP_OK ||
        pcnt_filter_enable(PCNT_UNIT_0) != ESP_OK ||
        pcnt_counter_clear(PCNT_UNIT_0) != ESP_OK ||
        pcnt_counter_resume(PCNT_UNIT_0) != ESP_OK) return false;
    selectChannel(6);
    // The physical Master takes ownership at its first ADC conversion.
    ControlState state = controls_.snapshot(); state.masterVolume = 0; controls_.set(state);
    ready_ = true;
    return true;
}

void PanelInputs::poll() {
    if (!ready_) return;
    const uint32_t now = micros();
    if (lastPoll_ && now - lastPoll_ > maximumPollGapUs_) maximumPollGapUs_ = now - lastPoll_;
    lastPoll_ = now;
    for (unsigned i = 0; i < 5; ++i) {
        Button &button = buttons_[i];
        const bool pressed = digitalRead(kButtons[i]) == LOW;
        if (pressed != button.raw) { button.raw = pressed; button.changed = now; }
        if (button.stable == button.raw || now - button.changed < kDebounceUs) continue;
        button.stable = button.raw;
        if (!button.stable) button.armed = true;
        if (!button.armed) continue;
        if (i < 4) parser_.setPhysicalPad(i + 1, button.stable);
        else parser_.setPhysicalEncoderPressed(button.stable);
    }
    int16_t count = 0;
    if (pcnt_get_counter_value(PCNT_UNIT_0, &count) == ESP_OK) {
        int delta = count - previousEncoderCount_;
        // PCNT clears at either limit; account for that boundary.
        if (delta > 16384) delta -= 32768;
        else if (delta < -16384) delta += 32767;
        previousEncoderCount_ = count;
        encoderRemainder_ += delta;
        while (encoderRemainder_ >= 4) { parser_.rotatePhysicalEncoder(1); encoderRemainder_ -= 4; }
        while (encoderRemainder_ <= -4) { parser_.rotatePhysicalEncoder(-1); encoderRemainder_ += 4; }
    }
    if (now - selectedAt_ < kMuxSettleUs) return;
    const int millivolts = analogReadMilliVolts(kAdc);
    if (lastMillivolts_[channel_] < 0 || abs(millivolts - lastMillivolts_[channel_]) >= 5) {
        lastMillivolts_[channel_] = millivolts;
        // Nominal divider maxima: 3.3 V * Rpot / (Rpot + 4.7k).
        const float maximum = channel_ == 6 ? 2244.9f : 2672.1f;
        const float position = fminf(1.0f, fmaxf(0.0f, millivolts / maximum));
        if (channel_ == 6) {
            ControlState state = controls_.snapshot(); state.masterVolume = position; controls_.set(state);
            parser_.notifyPhysicalMaster();
        } else parser_.setPhysicalPot(channel_ + 1, position);
    }
    // Master every other slot: no pickup; pot mappings use shared pickup.
    if (channel_ == 6) { selectChannel(nextPot_); nextPot_ = (nextPot_ + 1) % 6; }
    else selectChannel(6);
}
#else
bool PanelInputs::begin() { return false; }
void PanelInputs::poll() {}
void PanelInputs::selectChannel(unsigned) {}
#endif
