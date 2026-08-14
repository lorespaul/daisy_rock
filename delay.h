#pragma once

#include <cstddef>

class GuitarDelay
{
  public:
    void Init(float sample_rate);
    void Reset();

    void SetLevelAdcChannel(int channel);
    int  LevelAdcChannel() const;
    void SetTimeAdcChannel(int channel);
    int  TimeAdcChannel() const;
    void SetFeedbackAdcChannel(int channel);
    int  FeedbackAdcChannel() const;

    void UpdateLevelFromAdc(float adc_value);
    void UpdateTimeFromAdc(float adc_value);
    void UpdateFeedbackFromAdc(float adc_value);
    void SetEnabled(bool enabled);

    float Process(float input);

  private:
    class OnePole
    {
      public:
        void Init(float sample_rate, float frequency_hz);
        void Reset(float value = 0.0f);
        float Process(float input);

      private:
        float a_ = 0.0f;
        float z_ = 0.0f;
    };

    static constexpr size_t kMaxDelaySamples = 48000;

    static float Clamp(float value, float lo, float hi);

    float ReadDelay(float delay_samples) const;

    float  sample_rate_ = 48000.0f;
    float  delay_[kMaxDelaySamples];
    size_t write_index_ = 0;

    float enabled_smooth_ = 0.0f;
    float level_          = 0.35f;
    float time_seconds_   = 0.32f;
    float feedback_       = 0.35f;

    int level_adc_channel_    = -1;
    int time_adc_channel_     = -1;
    int feedback_adc_channel_ = -1;

    OnePole level_smoother_;
    OnePole time_smoother_;
    OnePole feedback_smoother_;
    OnePole enable_smoother_;
    OnePole feedback_lowpass_;
    OnePole feedback_highpass_lp_;
};
