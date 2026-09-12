#pragma once

#include "daisy_seed.h"

#include <cstddef>

class SpringReverb
{
  public:
    void Init(float sample_rate);
    void InitControls(daisy::DaisySeed &hw);
    int ConfigureControls(daisy::AdcChannelConfig *config, int channel, daisy::DaisySeed &hw);
    void UpdateControls(const daisy::AdcHandle &adc);
    float Process(float input);

  private:
    static constexpr size_t kDiffuser1Size = 149;
    static constexpr size_t kDiffuser2Size = 211;
    static constexpr size_t kDiffuser3Size = 263;
    static constexpr size_t kComb1Size     = 4093;
    static constexpr size_t kComb2Size     = 2971;

    static float Clamp(float value, float lo, float hi);
    static float ProcessAllpass(float input, float *buffer, size_t size, size_t &index);
    static float ProcessComb(float input, float *buffer, size_t size, size_t &index, float &filter);
    static void Clear(float *buffer, size_t size);

    float level_   = 0.02f;
    float enabled_ = 0.0f;
    float input_low_   = 0.0f;
    float comb1_filter_ = 0.0f;
    float comb2_filter_ = 0.0f;

    int level_adc_channel_ = -1;
    dsy_gpio enable_pin_;

    size_t diffuser1_index_ = 0;
    size_t diffuser2_index_ = 0;
    size_t diffuser3_index_ = 0;
    size_t comb1_index_     = 0;
    size_t comb2_index_     = 0;
    float diffuser1_[kDiffuser1Size];
    float diffuser2_[kDiffuser2Size];
    float diffuser3_[kDiffuser3Size];
    float comb1_[kComb1Size];
    float comb2_[kComb2Size];
};
