#include "daisy_seed.h"
#include "delay.h"
#include "mesa_power.h"

#ifndef MESA_POWER_ENABLE
#define MESA_POWER_ENABLE 0
#endif

#ifndef MESA_POWER_POST_IR_PRESENCE
#define MESA_POWER_POST_IR_PRESENCE 0
#endif

#ifndef MESA_POWER_PICK_ATTACK_PIN
#define MESA_POWER_PICK_ATTACK_PIN -1
#endif

#ifndef ENABLE_OUTPUT_STAGE_PIN
#define ENABLE_OUTPUT_STAGE_PIN -1
#endif

#ifndef DELAY_ENABLE_PIN
#define DELAY_ENABLE_PIN -1
#endif

#ifndef DELAY_LEVEL_PIN
#define DELAY_LEVEL_PIN -1
#endif

#ifndef DELAY_TIME_PIN
#define DELAY_TIME_PIN -1
#endif

#ifndef DELAY_FEEDBACK_PIN
#define DELAY_FEEDBACK_PIN -1
#endif

#define MESA_POWER_ACTIVE (MESA_POWER_ENABLE || MESA_POWER_POST_IR_PRESENCE || MESA_POWER_PICK_ATTACK_PIN >= 0)
#define GUITAR_DELAY_ACTIVE \
    (DELAY_ENABLE_PIN >= 0 && DELAY_LEVEL_PIN >= 0 && DELAY_TIME_PIN >= 0 && DELAY_FEEDBACK_PIN >= 0)
#define MESA_POWER_ADC_CHANNEL_COUNT ((MESA_POWER_PRESENCE_PIN >= 0 ? 1 : 0) + (MESA_POWER_PICK_ATTACK_PIN >= 0 ? 1 : 0))
#define GUITAR_DELAY_ADC_CHANNEL_COUNT (GUITAR_DELAY_ACTIVE ? 3 : 0)
#define CONTROL_ADC_CHANNEL_COUNT (MESA_POWER_ADC_CHANNEL_COUNT + GUITAR_DELAY_ADC_CHANNEL_COUNT)

#if defined(IR_CONV_USE_DIRECT)
#include "ir_conv.h"
using IrConvolver = DirectConvolver;
#elif defined(IR_CONV_USE_FFT)
#include "ir_conv_fft.h"
using IrConvolver = FftConvolver;
#elif defined(IR_CONV_USE_FFT_PARTITIONED)
#include "ir_conv_fft_partitioned.h"
using IrConvolver = DirectHeadPartitionedFftConvolver;
#else
#include "ir_conv_fft_partitioned.h"
using IrConvolver = DirectHeadPartitionedFftConvolver;
#endif

using namespace daisy;

static DaisySeed hw;
static IrConvolver convolver;

#if MESA_POWER_ACTIVE
static MesaPowerAmp power_amp;
#endif

#if GUITAR_DELAY_ACTIVE
static GuitarDelay delay;
static dsy_gpio delay_enable;
#endif

#if CONTROL_ADC_CHANNEL_COUNT > 0
static AdcChannelConfig control_adc_config[CONTROL_ADC_CHANNEL_COUNT];
#endif

#if ENABLE_OUTPUT_STAGE_PIN >= 0
static dsy_gpio output_stage_enable;

static void EnableOutputStage()
{
    output_stage_enable.pin = hw.GetPin(ENABLE_OUTPUT_STAGE_PIN);
    output_stage_enable.mode = DSY_GPIO_MODE_OUTPUT_PP;
    output_stage_enable.pull = DSY_GPIO_NOPULL;
    dsy_gpio_init(&output_stage_enable);
    dsy_gpio_write(&output_stage_enable, 1);
}
#endif

static void InitControlAdc()
{
#if CONTROL_ADC_CHANNEL_COUNT > 0
    int adc_count = 0;
#if MESA_POWER_ACTIVE
#if MESA_POWER_PRESENCE_PIN >= 0
    control_adc_config[adc_count].InitSingle(hw.GetPin(MESA_POWER_PRESENCE_PIN));
    power_amp.SetPresenceAdcChannel(adc_count++);
#endif
#if MESA_POWER_PICK_ATTACK_PIN >= 0
    control_adc_config[adc_count].InitSingle(hw.GetPin(MESA_POWER_PICK_ATTACK_PIN));
    power_amp.SetPickAttackAdcChannel(adc_count++);
#endif
#endif
#if GUITAR_DELAY_ACTIVE
    control_adc_config[adc_count].InitSingle(hw.GetPin(DELAY_LEVEL_PIN));
    delay.SetLevelAdcChannel(adc_count++);
    control_adc_config[adc_count].InitSingle(hw.GetPin(DELAY_TIME_PIN));
    delay.SetTimeAdcChannel(adc_count++);
    control_adc_config[adc_count].InitSingle(hw.GetPin(DELAY_FEEDBACK_PIN));
    delay.SetFeedbackAdcChannel(adc_count++);
#endif
    hw.adc.Init(control_adc_config, adc_count);
    hw.adc.Start();
#endif
}

#if GUITAR_DELAY_ACTIVE
static void InitDelayControls()
{
    delay_enable.pin = hw.GetPin(DELAY_ENABLE_PIN);
    delay_enable.mode = DSY_GPIO_MODE_INPUT;
    delay_enable.pull = DSY_GPIO_NOPULL;
    dsy_gpio_init(&delay_enable);
}
#endif

static void UpdateMesaPowerControls()
{
#if (MESA_POWER_ENABLE || MESA_POWER_POST_IR_PRESENCE) && MESA_POWER_PRESENCE_PIN >= 0
    power_amp.UpdatePresenceFromAdc(hw.adc.GetFloat(power_amp.PresenceAdcChannel()));
#endif
#if MESA_POWER_PICK_ATTACK_PIN >= 0
    power_amp.UpdatePickAttackFromAdc(hw.adc.GetFloat(power_amp.PickAttackAdcChannel()));
#endif
}

static void UpdateDelayControls()
{
#if GUITAR_DELAY_ACTIVE
    delay.SetEnabled(dsy_gpio_read(&delay_enable) != 0);
    delay.UpdateLevelFromAdc(hw.adc.GetFloat(delay.LevelAdcChannel()));
    delay.UpdateTimeFromAdc(hw.adc.GetFloat(delay.TimeAdcChannel()));
    delay.UpdateFeedbackFromAdc(hw.adc.GetFloat(delay.FeedbackAdcChannel()));
#endif
}

static void AudioCallback(AudioHandle::InterleavingInputBuffer in, AudioHandle::InterleavingOutputBuffer out, size_t size)
{
    UpdateMesaPowerControls();
    UpdateDelayControls();

    for (size_t i = 0; i < size; i += 2)
    {
        const float mono = 0.5f * (in[i] + in[i + 1]);
#if MESA_POWER_ENABLE
        const float amp = power_amp.Process(mono);
#else
        const float amp = mono;
#endif
#if MESA_POWER_PICK_ATTACK_PIN >= 0
        const float picked = power_amp.ProcessPickAttack(amp);
#else
        const float picked = amp;
#endif
        const float wet = convolver.Process(picked);
#if GUITAR_DELAY_ACTIVE
        const float delayed = delay.Process(wet);
#else
        const float delayed = wet;
#endif
#if MESA_POWER_POST_IR_PRESENCE
        const float tone = power_amp.ProcessPresence(delayed);
        out[i] = tone;
        out[i + 1] = tone;
#else
        out[i] = delayed;
        out[i + 1] = delayed;
#endif
    }
}

int main(void)
{
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(IrConvolver::kAudioBlockSize);

#if MESA_POWER_ACTIVE
    power_amp.Init(hw.AudioSampleRate());
#endif
#if GUITAR_DELAY_ACTIVE
    delay.Init(hw.AudioSampleRate());
    InitDelayControls();
#endif
    InitControlAdc();
    convolver.Init();
#if ENABLE_OUTPUT_STAGE_PIN >= 0
    EnableOutputStage();
#endif
    hw.StartAudio(AudioCallback);

    while (1)
    {
    }
}
