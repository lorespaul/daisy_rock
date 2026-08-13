#include "../mesa_power.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

namespace
{
constexpr float kPi         = 3.14159265358979323846f;
constexpr float kSampleRate = 48000.0f;
constexpr float kInputPk    = 0.01f;
constexpr float kStartHz    = 80.0f;
constexpr float kStopHz     = 20000.0f;

struct PresenceRun
{
    float       value;
    const char* suffix;
};

const PresenceRun kPresenceRuns[] = {
    {0.0f, "0"},
    {0.33f, "33"},
    {0.66f, "66"},
    {1.0f, "100"},
};

float Sine(float amp, float freq, int sample)
{
    return amp * std::sin(2.0f * kPi * freq * static_cast<float>(sample) / kSampleRate);
}

std::vector<float> LogFrequencies(float start_hz, float stop_hz, int points_per_decade)
{
    std::vector<float> freqs;
    const float decades = std::log10(stop_hz / start_hz);
    const int   points  = static_cast<int>(std::floor(decades * points_per_decade)) + 1;
    freqs.reserve(points + 1);
    for(int i = 0; i < points; ++i)
    {
        freqs.push_back(start_hz * std::pow(10.0f, static_cast<float>(i) / points_per_decade));
    }
    if(freqs.empty() || freqs.back() < stop_hz)
        freqs.push_back(stop_hz);
    return freqs;
}

float MeasureFundamentalGainDb(float presence, float frequency_hz)
{
    MesaPowerAmp amp;
    amp.Init(kSampleRate);
    amp.SetPresence(presence);

    const int measure_samples = static_cast<int>(0.35f * kSampleRate);
    const int settle_samples  = static_cast<int>(std::max(0.25f, 12.0f / frequency_hz) * kSampleRate);
    const int total_samples   = settle_samples + measure_samples;

    double out_re = 0.0;
    double out_im = 0.0;
    double in_re  = 0.0;
    double in_im  = 0.0;
    for(int n = 0; n < total_samples; ++n)
    {
        const float x = Sine(kInputPk, frequency_hz, n);
        const float y = amp.Process(x);
        if(n >= settle_samples)
        {
            const int    k     = n - settle_samples;
            const double phase = 2.0 * kPi * frequency_hz * static_cast<double>(k) / kSampleRate;
            const double win   = 0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(k) / (measure_samples - 1));
            const double c     = std::cos(phase);
            const double s     = std::sin(phase);
            out_re += win * y * c;
            out_im -= win * y * s;
            in_re += win * x * c;
            in_im -= win * x * s;
        }
    }

    const double out_mag = std::sqrt(out_re * out_re + out_im * out_im);
    const double in_mag  = std::sqrt(in_re * in_re + in_im * in_im);
    return 20.0f * std::log10(static_cast<float>((out_mag + 1.0e-18) / (in_mag + 1.0e-18)));
}

} // namespace

int main(int argc, char** argv)
{
    const std::string out_dir
        = argc > 1 ? argv[1] : "seed/DSP/ir_conv/mesa_spice/results/freq_response_mesa_power";
    const auto freqs = LogFrequencies(kStartHz, kStopHz, 80);

    for(const PresenceRun& run : kPresenceRuns)
    {
        const std::string path = out_dir + "/mesa_power_presence_" + run.suffix + ".csv";
        std::ofstream     csv(path);
        if(!csv)
            return 1;

        csv << "X,Y\n";
        for(float freq : freqs)
        {
            const float mesa_db = MeasureFundamentalGainDb(run.value, freq);
            csv << std::setprecision(9) << freq << "," << mesa_db << "\n";
        }
    }

    return 0;
}
