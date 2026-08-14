#include "delay.h"

#include <math.h>

void GuitarDelay::OnePole::Init(float sample_rate, float frequency_hz)
{
    const float x = -2.0f * 3.14159265358979323846f * frequency_hz / sample_rate;
    a_            = 1.0f - expf(x);
}

void GuitarDelay::OnePole::Reset(float value)
{
    z_ = value;
}

float GuitarDelay::OnePole::Process(float input)
{
    z_ += a_ * (input - z_);
    return z_;
}

void GuitarDelay::Init(float sample_rate)
{
    sample_rate_ = sample_rate;

    level_smoother_.Init(sample_rate_, 8.0f);
    time_smoother_.Init(sample_rate_, 3.0f);
    feedback_smoother_.Init(sample_rate_, 5.0f);
    enable_smoother_.Init(sample_rate_, 40.0f);
    feedback_lowpass_.Init(sample_rate_, 2600.0f);
    feedback_highpass_lp_.Init(sample_rate_, 90.0f);

    Reset();
}

void GuitarDelay::Reset()
{
    for(size_t i = 0; i < kMaxDelaySamples; i++)
        delay_[i] = 0.0f;

    write_index_ = 0;
    enabled_smooth_ = 0.0f;

    level_smoother_.Reset(level_);
    time_smoother_.Reset(time_seconds_);
    feedback_smoother_.Reset(feedback_);
    enable_smoother_.Reset(0.0f);
    feedback_lowpass_.Reset();
    feedback_highpass_lp_.Reset();
}

void GuitarDelay::SetLevelAdcChannel(int channel)
{
    level_adc_channel_ = channel;
}

int GuitarDelay::LevelAdcChannel() const
{
    return level_adc_channel_;
}

void GuitarDelay::SetTimeAdcChannel(int channel)
{
    time_adc_channel_ = channel;
}

int GuitarDelay::TimeAdcChannel() const
{
    return time_adc_channel_;
}

void GuitarDelay::SetFeedbackAdcChannel(int channel)
{
    feedback_adc_channel_ = channel;
}

int GuitarDelay::FeedbackAdcChannel() const
{
    return feedback_adc_channel_;
}

void GuitarDelay::UpdateLevelFromAdc(float adc_value)
{
    const float x = Clamp(adc_value, 0.0f, 1.0f);
    level_        = x * x * 0.75f;
}

void GuitarDelay::UpdateTimeFromAdc(float adc_value)
{
    const float x = Clamp(adc_value, 0.0f, 1.0f);
    time_seconds_ = 0.055f + x * x * 0.895f;
}

void GuitarDelay::UpdateFeedbackFromAdc(float adc_value)
{
    const float x              = Clamp(adc_value, 0.0f, 1.0f);
    const float repeats        = 1.0f + x * 9.0f;
    const float final_echo_gain = 0.08f;
    feedback_                  = powf(final_echo_gain, 1.0f / repeats);
}

void GuitarDelay::SetEnabled(bool enabled)
{
    enabled_smooth_ = enable_smoother_.Process(enabled ? 1.0f : 0.0f);
}

float GuitarDelay::Process(float input)
{
    const float level        = level_smoother_.Process(level_);
    const float time_seconds = time_smoother_.Process(time_seconds_);
    const float feedback    = feedback_smoother_.Process(feedback_);

    const float max_delay_samples = static_cast<float>(kMaxDelaySamples - 2);
    const float delay_samples     = Clamp(time_seconds * sample_rate_, 1.0f, max_delay_samples);
    const float delayed           = ReadDelay(delay_samples);

    const float highpass_lp = feedback_highpass_lp_.Process(delayed);
    const float darker_echo = feedback_lowpass_.Process(delayed - highpass_lp);
    const float feedback_in = input + darker_echo * feedback;

    delay_[write_index_] = feedback_in;
    write_index_++;
    if(write_index_ >= kMaxDelaySamples)
        write_index_ = 0;

    return input + delayed * level * enabled_smooth_;
}

float GuitarDelay::ReadDelay(float delay_samples) const
{
    float read_position = static_cast<float>(write_index_) - delay_samples;
    while(read_position < 0.0f)
        read_position += static_cast<float>(kMaxDelaySamples);

    const size_t index_a = static_cast<size_t>(read_position);
    const size_t index_b = index_a + 1 >= kMaxDelaySamples ? 0 : index_a + 1;
    const float  frac    = read_position - static_cast<float>(index_a);

    return delay_[index_a] + (delay_[index_b] - delay_[index_a]) * frac;
}

float GuitarDelay::Clamp(float value, float lo, float hi)
{
    if(value < lo)
        return lo;
    if(value > hi)
        return hi;
    return value;
}
