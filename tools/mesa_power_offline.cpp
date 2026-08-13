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

const float kPresenceValues[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
const float kAmpValues[]      = {0.10f, 0.20f, 0.35f, 0.50f, 0.75f, 1.00f, 1.25f, 1.50f};

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

struct Measures
{
    float rms = 0.0f;
    float pk  = 0.0f;
    float nk  = 0.0f;
    float thd = 0.0f;
};

Measures MeasureSine(MesaPowerAmp& amp, float input_pk, float freq_hz, float seconds, float measure_seconds)
{
    amp.Reset();
    const int total_samples   = static_cast<int>(seconds * kSampleRate);
    const int measure_samples = static_cast<int>(measure_seconds * kSampleRate);
    const int measure_start   = total_samples - measure_samples;

    double sum_sq = 0.0;
    float  pk     = -1.0e9f;
    float  nk     = 1.0e9f;

    std::vector<float> measured;
    measured.reserve(measure_samples);

    for(int n = 0; n < total_samples; ++n)
    {
        const float y = amp.Process(Sine(input_pk, freq_hz, n));
        if(n >= measure_start)
        {
            sum_sq += static_cast<double>(y) * y;
            if(y > pk)
                pk = y;
            if(y < nk)
                nk = y;
            measured.push_back(y);
        }
    }

    double fundamental_re = 0.0;
    double fundamental_im = 0.0;
    double harmonic_sum   = 0.0;
    for(int h = 1; h <= 10; ++h)
    {
        double re = 0.0;
        double im = 0.0;
        for(size_t n = 0; n < measured.size(); ++n)
        {
            const double phase = 2.0 * kPi * h * freq_hz * static_cast<double>(n) / kSampleRate;
            re += measured[n] * std::cos(phase);
            im -= measured[n] * std::sin(phase);
        }
        const double mag = 2.0 * std::sqrt(re * re + im * im) / static_cast<double>(measured.size());
        if(h == 1)
        {
            fundamental_re = re;
            fundamental_im = im;
        }
        else
        {
            harmonic_sum += mag * mag;
        }
    }

    const double fundamental_mag = 2.0 * std::sqrt(fundamental_re * fundamental_re + fundamental_im * fundamental_im)
                                  / static_cast<double>(measured.size());

    Measures result;
    result.rms = static_cast<float>(std::sqrt(sum_sq / measure_samples));
    result.pk  = pk;
    result.nk  = nk;
    result.thd = fundamental_mag > 1.0e-9 ? static_cast<float>(100.0 * std::sqrt(harmonic_sum) / fundamental_mag) : 0.0f;
    return result;
}

void GenerateAcPresence(const std::string& out_dir)
{
    const auto freqs = LogFrequencies(10.0f, 50000.0f, 80);

    std::ofstream summary(out_dir + "/dsp_ac_presence_summary.csv");
    summary << "presence,db_100hz,db_1khz,db_5khz,db_10khz\n";

    for(float presence : kPresenceValues)
    {
        MesaPowerAmp amp;
        amp.Init(kSampleRate);
        amp.SetPresence(presence);

        char suffix[16];
        std::snprintf(suffix, sizeof(suffix), "%.2f", presence);
        std::ofstream csv(out_dir + "/dsp_ac_presence_" + suffix + ".csv");
        csv << "freq_hz,mesa_out_db\n";

        float best_100_dist  = 1.0e9f;
        float best_1k_dist   = 1.0e9f;
        float best_5k_dist   = 1.0e9f;
        float best_10k_dist  = 1.0e9f;
        float best_100_db    = 0.0f;
        float best_1k_db     = 0.0f;
        float best_5k_db     = 0.0f;
        float best_10k_db    = 0.0f;

        for(float freq : freqs)
        {
            amp.SetPresence(presence);
            const Measures m      = MeasureSine(amp, 0.01f, freq, 0.22f, 0.06f);
            const float input_rms = 0.01f / std::sqrt(2.0f);
            const float db        = 20.0f * std::log10((m.rms + 1.0e-12f) / input_rms);
            csv << std::setprecision(9) << freq << "," << db << "\n";

            const float d100 = std::fabs(freq - 100.0f);
            const float d1k  = std::fabs(freq - 1000.0f);
            const float d5k  = std::fabs(freq - 5000.0f);
            const float d10k = std::fabs(freq - 10000.0f);
            if(d100 < best_100_dist)
            {
                best_100_dist = d100;
                best_100_db   = db;
            }
            if(d1k < best_1k_dist)
            {
                best_1k_dist = d1k;
                best_1k_db   = db;
            }
            if(d5k < best_5k_dist)
            {
                best_5k_dist = d5k;
                best_5k_db   = db;
            }
            if(d10k < best_10k_dist)
            {
                best_10k_dist = d10k;
                best_10k_db   = db;
            }
        }

        summary << presence << "," << best_100_db << "," << best_1k_db << "," << best_5k_db << "," << best_10k_db << "\n";
    }
}

void GenerateAmplitudeSweep(const std::string& out_dir)
{
    std::ofstream summary(out_dir + "/dsp_amplitude_1khz_summary.csv");
    summary << "vin_pk,out_rms,out_pk,out_nk,thd_percent\n";

    for(float input_pk : kAmpValues)
    {
        MesaPowerAmp amp;
        amp.Init(kSampleRate);
        amp.SetPresence(0.5f);

        const Measures m = MeasureSine(amp, input_pk, 1000.0f, 0.26f, 0.05f);
        summary << input_pk << "," << m.rms << "," << m.pk << "," << m.nk << "," << m.thd << "\n";

        char suffix[16];
        std::snprintf(suffix, sizeof(suffix), "%.2f", input_pk);
        std::ofstream csv(out_dir + "/dsp_tran_1khz_amp_" + suffix + ".csv");
        csv << "time_s,mesa_out_v\n";

        amp.Reset();
        const int total_samples = static_cast<int>(0.26f * kSampleRate);
        for(int n = 0; n < total_samples; ++n)
        {
            const float y = amp.Process(Sine(input_pk, 1000.0f, n));
            if(n >= static_cast<int>(0.18f * kSampleRate))
                csv << std::setprecision(9) << static_cast<float>(n) / kSampleRate << "," << y << "\n";
        }
    }
}

void GenerateBurst(const std::string& out_dir)
{
    MesaPowerAmp amp;
    amp.Init(kSampleRate);
    amp.SetPresence(0.5f);

    std::ofstream csv(out_dir + "/dsp_burst_100hz.csv");
    csv << "time_s,in_v,mesa_out_v\n";
    const int total_samples = static_cast<int>(0.30f * kSampleRate);
    for(int n = 0; n < total_samples; ++n)
    {
        const float t = static_cast<float>(n) / kSampleRate;
        const float x = (t > 0.030f && t < 0.150f) ? Sine(1.0f, 100.0f, n) : 0.0f;
        const float y = amp.Process(x);
        csv << std::setprecision(9) << t << "," << x << "," << y << "\n";
    }
}

} // namespace

int main(int argc, char** argv)
{
    const std::string out_dir = argc > 1 ? argv[1] : "../mesa_spice/results/golden_reference";
    GenerateAcPresence(out_dir);
    GenerateAmplitudeSweep(out_dir);
    GenerateBurst(out_dir);
    return 0;
}
