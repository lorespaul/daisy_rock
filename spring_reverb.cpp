#include "spring_reverb.h"

#include <math.h>

void SpringReverb::Init(float sample_rate)
{
    (void)sample_rate;
    Clear(diffuser1_, kDiffuser1Size);
    Clear(diffuser2_, kDiffuser2Size);
    Clear(diffuser3_, kDiffuser3Size);
    Clear(comb1_, kComb1Size);
    Clear(comb2_, kComb2Size);
}

void SpringReverb::InitControls(daisy::DaisySeed &hw)
{
    enable_pin_.pin = hw.GetPin(REVERB_ENABLE_PIN);
    enable_pin_.mode = DSY_GPIO_MODE_INPUT;
    enable_pin_.pull = DSY_GPIO_PULLUP;
    dsy_gpio_init(&enable_pin_);
}

int SpringReverb::ConfigureControls(daisy::AdcChannelConfig *config, int channel, daisy::DaisySeed &hw)
{
    config[channel].InitSingle(hw.GetPin(REVERB_LEVEL_PIN));
    level_adc_channel_ = channel;
    return channel + 1;
}

void SpringReverb::UpdateControls(const daisy::AdcHandle &adc)
{
    const float x = Clamp(adc.GetFloat(level_adc_channel_), 0.0f, 1.0f);
    level_        = 0.02f + 0.28f * x * x;
    enabled_ += 0.002f * ((dsy_gpio_read(&enable_pin_) != 0 ? 1.0f : 0.0f) - enabled_);
}

float SpringReverb::Process(float input)
{
    // Spring tanks reject much of the low end and build a dense, metallic tail.
    input_low_ += 0.018f * (input - input_low_);
    float tank = input - input_low_;
    tank       = ProcessAllpass(tank, diffuser1_, kDiffuser1Size, diffuser1_index_);
    tank       = ProcessAllpass(tank, diffuser2_, kDiffuser2Size, diffuser2_index_);
    tank       = ProcessAllpass(tank, diffuser3_, kDiffuser3Size, diffuser3_index_);

    const float comb1 = ProcessComb(tank, comb1_, kComb1Size, comb1_index_, comb1_filter_);
    const float comb2 = ProcessComb(tank, comb2_, kComb2Size, comb2_index_, comb2_filter_);
    const float wet   = 0.5f * (comb1 + comb2);
    return input + wet * level_ * enabled_;
}

float SpringReverb::Clamp(float value, float lo, float hi)
{
    return value < lo ? lo : (value > hi ? hi : value);
}

float SpringReverb::ProcessAllpass(float input, float *buffer, size_t size, size_t &index)
{
    const float delayed = buffer[index];
    buffer[index]       = input + 0.67f * delayed;
    index               = index + 1 == size ? 0 : index + 1;
    return delayed - 0.67f * input;
}

float SpringReverb::ProcessComb(float input, float *buffer, size_t size, size_t &index, float &filter)
{
    const float delayed = buffer[index];
    filter              += 0.22f * (delayed - filter);
    buffer[index]       = input + 0.78f * filter;
    index               = index + 1 == size ? 0 : index + 1;
    return delayed;
}

void SpringReverb::Clear(float *buffer, size_t size)
{
    for(size_t i = 0; i < size; ++i)
        buffer[i] = 0.0f;
}
