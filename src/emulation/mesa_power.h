#pragma once

#include "daisy_seed.h"

#include <stdint.h>

#ifndef MESA_POWER_PRESENCE_PIN
#define MESA_POWER_PRESENCE_PIN -1
#endif

class MesaPowerAmp
{
  public:
    void Init(float sample_rate);
    void Reset();

    void SetPresence(float value);
    void SetDamping(float value);
    void SetSag(float value);
    void SetPickAttack(float value);
    int ConfigureControls(daisy::AdcChannelConfig *config, int channel, daisy::DaisySeed &hw);
    void UpdateControls(const daisy::AdcHandle &adc);

    float Process(float input);
    float ProcessPickAttack(float input);
    float ProcessPresence(float input);

  private:
    class OnePole
    {
      public:
        void Init(float sample_rate, float frequency_hz);
        void Reset(float value = 0.0f);
        void SetFrequency(float sample_rate, float frequency_hz);
        float Process(float input);
        float State() const;

      private:
        float a_ = 0.0f;
        float z_ = 0.0f;
    };

    class DcBlocker
    {
      public:
        void Reset();
        float Process(float input);

      private:
        float x1_ = 0.0f;
        float y1_ = 0.0f;
    };

    class SpeakerResonance
    {
      public:
        void Init(float sample_rate, float frequency_hz, float q);
        void Reset();
        float Process(float input);

      private:
        float f_    = 0.0f;
        float damp_ = 0.0f;
        float low_  = 0.0f;
        float band_ = 0.0f;
    };

    class BiquadLowpass
    {
      public:
        void Init(float sample_rate, float frequency_hz, float q);
        void Reset();
        float Process(float input);

      private:
        float b0_ = 1.0f;
        float b1_ = 0.0f;
        float b2_ = 0.0f;
        float a1_ = 0.0f;
        float a2_ = 0.0f;
        float z1_ = 0.0f;
        float z2_ = 0.0f;
    };

    static float Clamp(float value, float lo, float hi);
    static float SoftClip(float x, float limit);
    static float TubePairLaw(float x, float asymmetry);
    static float TimeCoeff(float sample_rate, float seconds);

    float sample_rate_  = 48000.0f;
    float presence_     = 0.5f;
    float damping_      = 0.55f;
    float sag_amount_   = 0.45f;
    float pick_attack_  = 1.0f;

    int presence_adc_channel_ = -1;
    int pick_attack_adc_channel_ = -1;

    OnePole input_coupling_lp_;
    OnePole presence_lp_;
    OnePole presence_upper_lp_;
    OnePole speaker_inductance_lp_;
    OnePole ot_lowpass_;
    OnePole export_presence_low_lp_;
    OnePole export_presence_upper_lp_;
    OnePole export_presence_floor_lp_;
    OnePole export_low_lift_lp_;
    OnePole export_brilliance_low_lp_;
    OnePole export_brilliance_high_lp_;
    OnePole export_air_low_lp_;
    OnePole export_air_high_lp_;
    OnePole post_presence_low_lp_;
    OnePole post_presence_high_lp_;
    OnePole post_presence_air_lp_;
    OnePole pick_low_lp_;
    OnePole pick_high_lp_;
    OnePole pick_env_fast_lp_;
    OnePole pick_env_slow_lp_;
    BiquadLowpass export_lowpass_;
    SpeakerResonance speaker_resonance_;
    DcBlocker output_dc_;

    float previous_speaker_ = 0.0f;
    float sag_state_       = 0.0f;
    float bias_shift_      = 0.0f;
    float sag_attack_coeff_ = 0.0f;
    float sag_release_coeff_ = 0.0f;
    float bias_attack_coeff_ = 0.0f;
    float bias_release_coeff_ = 0.0f;
};
