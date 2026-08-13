#include "mesa_power.h"

#include <math.h>

namespace
{
constexpr float kPi       = 3.14159265358979323846f;
constexpr float kMinFreq  = 1.0f;
constexpr float kMaxOutPk = 100.0f;
constexpr float kFixedDrive = 0.005f;
constexpr float kFixedOutputLevel = 64.0f;
} // namespace

void MesaPowerAmp::OnePole::Init(float sample_rate, float frequency_hz)
{
    Reset();
    SetFrequency(sample_rate, frequency_hz);
}

void MesaPowerAmp::OnePole::Reset(float value)
{
    z_ = value;
}

void MesaPowerAmp::OnePole::SetFrequency(float sample_rate, float frequency_hz)
{
    const float f = MesaPowerAmp::Clamp(frequency_hz, kMinFreq, 0.45f * sample_rate);
    a_            = 1.0f - expf(-2.0f * kPi * f / sample_rate);
}

float MesaPowerAmp::OnePole::Process(float input)
{
    z_ += a_ * (input - z_);
    return z_;
}

float MesaPowerAmp::OnePole::State() const
{
    return z_;
}

void MesaPowerAmp::DcBlocker::Reset()
{
    x1_ = 0.0f;
    y1_ = 0.0f;
}

float MesaPowerAmp::DcBlocker::Process(float input)
{
    const float y = input - x1_ + 0.9995f * y1_;
    x1_           = input;
    y1_           = y;
    return y;
}

void MesaPowerAmp::SpeakerResonance::Init(float sample_rate, float frequency_hz, float q)
{
    const float f = MesaPowerAmp::Clamp(frequency_hz, 20.0f, 0.20f * sample_rate);
    f_            = 2.0f * sinf(kPi * f / sample_rate);
    damp_         = 1.0f / MesaPowerAmp::Clamp(q, 0.3f, 10.0f);
    Reset();
}

void MesaPowerAmp::SpeakerResonance::Reset()
{
    low_  = 0.0f;
    band_ = 0.0f;
}

float MesaPowerAmp::SpeakerResonance::Process(float input)
{
    low_ += f_ * band_;
    const float high = input - low_ - damp_ * band_;
    band_ += f_ * high;
    return band_;
}

void MesaPowerAmp::BiquadLowpass::Init(float sample_rate, float frequency_hz, float q)
{
    const float f      = MesaPowerAmp::Clamp(frequency_hz, 20.0f, 0.45f * sample_rate);
    const float omega  = 2.0f * kPi * f / sample_rate;
    const float sinw   = sinf(omega);
    const float cosw   = cosf(omega);
    const float alpha  = sinw / (2.0f * MesaPowerAmp::Clamp(q, 0.1f, 10.0f));
    const float a0     = 1.0f + alpha;
    const float b0_raw = (1.0f - cosw) * 0.5f;
    const float b1_raw = 1.0f - cosw;
    const float b2_raw = (1.0f - cosw) * 0.5f;
    const float a1_raw = -2.0f * cosw;
    const float a2_raw = 1.0f - alpha;

    b0_ = b0_raw / a0;
    b1_ = b1_raw / a0;
    b2_ = b2_raw / a0;
    a1_ = a1_raw / a0;
    a2_ = a2_raw / a0;
    Reset();
}

void MesaPowerAmp::BiquadLowpass::Reset()
{
    z1_ = 0.0f;
    z2_ = 0.0f;
}

float MesaPowerAmp::BiquadLowpass::Process(float input)
{
    const float output = b0_ * input + z1_;
    z1_                = b1_ * input - a1_ * output + z2_;
    z2_                = b2_ * input - a2_ * output;
    return output;
}

void MesaPowerAmp::Init(float sample_rate)
{
    sample_rate_ = sample_rate > 1000.0f ? sample_rate : 48000.0f;

    input_coupling_lp_.Init(sample_rate_, 7.0f);
    presence_lp_.Init(sample_rate_, 2800.0f);
    presence_upper_lp_.Init(sample_rate_, 7600.0f);
    speaker_inductance_lp_.Init(sample_rate_, 2800.0f);
    ot_lowpass_.Init(sample_rate_, 12000.0f);
    export_presence_low_lp_.Init(sample_rate_, 3000.0f);
    export_presence_upper_lp_.Init(sample_rate_, 8200.0f);
    export_presence_floor_lp_.Init(sample_rate_, 360.0f);
    export_low_lift_lp_.Init(sample_rate_, 160.0f);
    export_brilliance_low_lp_.Init(sample_rate_, 2400.0f);
    export_brilliance_high_lp_.Init(sample_rate_, 9500.0f);
    export_air_low_lp_.Init(sample_rate_, 9000.0f);
    export_air_high_lp_.Init(sample_rate_, 15000.0f);
    post_presence_low_lp_.Init(sample_rate_, 3100.0f);
    post_presence_high_lp_.Init(sample_rate_, 9300.0f);
    post_presence_air_lp_.Init(sample_rate_, 15500.0f);
    pick_low_lp_.Init(sample_rate_, 2300.0f);
    pick_high_lp_.Init(sample_rate_, 8000.0f);
    pick_env_fast_lp_.Init(sample_rate_, 130.0f);
    pick_env_slow_lp_.Init(sample_rate_, 12.0f);
    export_lowpass_.Init(sample_rate_, 17500.0f, 0.45f);
    speaker_resonance_.Init(sample_rate_, 105.0f, 1.25f);

    sag_attack_coeff_   = TimeCoeff(sample_rate_, 0.012f);
    sag_release_coeff_  = TimeCoeff(sample_rate_, 0.280f);
    bias_attack_coeff_  = TimeCoeff(sample_rate_, 0.004f);
    bias_release_coeff_ = TimeCoeff(sample_rate_, 0.180f);

    Reset();
}

void MesaPowerAmp::Reset()
{
    input_coupling_lp_.Reset();
    presence_lp_.Reset();
    presence_upper_lp_.Reset();
    speaker_inductance_lp_.Reset();
    ot_lowpass_.Reset();
    export_presence_low_lp_.Reset();
    export_presence_upper_lp_.Reset();
    export_presence_floor_lp_.Reset();
    export_low_lift_lp_.Reset();
    export_brilliance_low_lp_.Reset();
    export_brilliance_high_lp_.Reset();
    export_air_low_lp_.Reset();
    export_air_high_lp_.Reset();
    post_presence_low_lp_.Reset();
    post_presence_high_lp_.Reset();
    post_presence_air_lp_.Reset();
    pick_low_lp_.Reset();
    pick_high_lp_.Reset();
    pick_env_fast_lp_.Reset();
    pick_env_slow_lp_.Reset();
    export_lowpass_.Reset();
    speaker_resonance_.Reset();
    output_dc_.Reset();

    previous_speaker_ = 0.0f;
    sag_state_       = 0.0f;
    bias_shift_      = 0.0f;
}

void MesaPowerAmp::SetPresence(float value)
{
    presence_ = Clamp(value, 0.0f, 1.0f);
}

void MesaPowerAmp::SetDamping(float value)
{
    damping_ = Clamp(value, 0.0f, 1.0f);
}

void MesaPowerAmp::SetSag(float value)
{
    sag_amount_ = Clamp(value, 0.0f, 1.0f);
}

void MesaPowerAmp::SetPickAttack(float value)
{
    pick_attack_ = Clamp(value, 0.0f, 1.0f);
}

void MesaPowerAmp::SetPresenceAdcChannel(int channel)
{
    presence_adc_channel_ = channel;
}

int MesaPowerAmp::PresenceAdcChannel() const
{
    return presence_adc_channel_;
}

void MesaPowerAmp::UpdatePresenceFromAdc(float adc_value)
{
    SetPresence(adc_value);
}

void MesaPowerAmp::SetPickAttackAdcChannel(int channel)
{
    pick_attack_adc_channel_ = channel;
}

int MesaPowerAmp::PickAttackAdcChannel() const
{
    return pick_attack_adc_channel_;
}

void MesaPowerAmp::UpdatePickAttackFromAdc(float adc_value)
{
    SetPickAttack(adc_value);
}

float MesaPowerAmp::Process(float input)
{
    // AC coupling dell'ingresso del finale: nel Mesa reale il PI non vede DC dal
    // preamp, ma solo il segnale audio passato dai condensatori di accoppiamento.
    // Qui il passa-alto e' ottenuto sottraendo un low-pass molto lento.
    const float coupled = input - input_coupling_lp_.Process(input);

    // Carico speaker/OT visto dal loop di feedback. previous_speaker_ e' il
    // campione di uscita del trasformatore al giro precedente: lo usiamo come
    // misura locale della tensione sul secondario. speaker_res aggiunge la
    // risonanza bassa del sistema trasformatore+speaker, speaker_hi aggiunge la
    // componente alta legata all'induttanza della bobina. Il controllo damping_
    // cambia quanto il feedback "frena" queste due componenti, simulando la
    // variazione di damping percepito di un finale contro un carico reattivo.
    const float speaker_res = speaker_resonance_.Process(previous_speaker_);
    const float speaker_hi  = previous_speaker_ - speaker_inductance_lp_.Process(previous_speaker_);
    const float speaker     = previous_speaker_
                          + (0.08f + 0.12f * (1.0f - damping_)) * speaker_res
                          + (0.08f + 0.20f * damping_) * speaker_hi;

    // Presence nel loop di negative feedback. Nel circuito valvolare la Presence
    // non e' un semplice EQ in uscita: modifica lo spettro del segnale riportato
    // dal secondario al phase inverter. Riducendo il feedback su una banda alta,
    // quella banda viene amplificata di piu' dal loop chiuso. presence_band e'
    // quindi la parte alta del segnale speaker che viene sottratta dal feedback.
    const float presence_low  = presence_lp_.Process(speaker);
    const float presence_high = presence_upper_lp_.Process(speaker);
    const float presence_band = presence_high - presence_low;
    const float presence_knee  = presence_ * (0.35f + 0.65f * presence_);
    const float presence_loop  = presence_knee;
    const float nfb_gain      = 0.09f + 0.18f * damping_;
    const float feedback      = nfb_gain * (speaker - 0.50f * presence_loop * presence_band);

    // Phase inverter long-tail semplificato. Il segnale in ingresso viene
    // sottratto al feedback, poi generato in due semionde opposte ma non
    // perfettamente simmetriche. I guadagni diversi imitano sbilanciamento PI,
    // tolleranze e diverso swing utile sui due lati. SoftClip qui rappresenta il
    // limite progressivo di swing del PI, non un pedale overdrive.
    const float pi_drive = kFixedDrive * coupled - feedback - 0.18f * bias_shift_;
    const float pi_a     = SoftClip(7.2f * pi_drive, 2.8f);
    const float pi_b     = SoftClip(-7.8f * pi_drive, 2.8f);

    // Grid current e bias shift. Quando il PI spinge troppo le griglie delle 6L6,
    // la conduzione di griglia carica il circuito precedente e sposta
    // temporaneamente il punto di bias. In DSP usiamo l'energia oltre una soglia
    // come proxy della corrente di griglia; attacco rapido e rilascio lento danno
    // compressione dinamica dopo transienti forti, invece di una saturazione
    // statica identica a ogni campione.
    const float grid_current = Clamp(fabsf(pi_a) + fabsf(pi_b) - 2.6f, 0.0f, 8.0f);
    const float bias_target  = 0.11f * grid_current;
    const float bias_coeff   = bias_target > bias_shift_ ? bias_attack_coeff_ : bias_release_coeff_;
    bias_shift_ = bias_coeff * bias_shift_ + (1.0f - bias_coeff) * bias_target;

    // Sag dell'alimentazione. La disponibilita' di tensione delle 6L6 cala quando
    // la corrente media aumenta e recupera piu' lentamente. supply scala quindi
    // sia lo swing utile sia il punto in cui il modello di valvola comprime.
    const float supply = Clamp(1.0f - sag_amount_ * sag_state_, 0.58f, 1.0f);

    // Coppia push-pull di 6L6. TubePairLaw e' una legge morbida con lieve
    // asimmetria: non tenta di essere una SPICE table, ma conserva i fenomeni
    // importanti per l'audio in tempo reale: compressione progressiva, differenza
    // fra i due rami e cancellazione parziale delle armoniche pari nel push-pull.
    const float tube_a = TubePairLaw((pi_a - bias_shift_) * supply, 0.045f);
    const float tube_b = TubePairLaw((pi_b + 0.85f * bias_shift_) * supply, -0.035f);
    const float pp     = 0.5f * (tube_a - tube_b);

    // Misura della richiesta di corrente per aggiornare il sag. Usiamo la somma
    // dei valori assoluti dei due rami perche' la supply vede corrente assorbita
    // anche quando il segnale sul primario e' differenziale.
    const float current_target = Clamp(0.22f * (fabsf(tube_a) + fabsf(tube_b)), 0.0f, 1.0f);
    const float sag_coeff     = current_target > sag_state_ ? sag_attack_coeff_ : sag_release_coeff_;
    sag_state_ = sag_coeff * sag_state_ + (1.0f - sag_coeff) * current_target;

    // Trasformatore di uscita. La saturazione morbida rappresenta il limite del
    // core e dipende dalla supply disponibile; il low-pass successivo limita la
    // banda alta come leakage inductance, capacita' parassite e carico reale.
    const float ot_core = SoftClip(pp, 1.05f * supply);
    const float ot_band = ot_lowpass_.Process(ot_core);
    previous_speaker_   = ot_band;

    // Sagoma di uscita esportata al DSP. Questo e' ancora parte del modello del
    // finale, non della IR: aggiunge il contributo tonale residuo di Presence,
    // risposta bassa e roll-off alto del finale prima del cabinet/microfono.
    // Le bande sono ricavate con differenze fra low-pass, cosi' restano filtri
    // morbidi e stabili adatti alla Daisy.
    const float export_presence_low  = export_presence_low_lp_.Process(ot_band);
    const float export_presence_high = export_presence_upper_lp_.Process(ot_band);
    const float export_presence_band = export_presence_high - export_presence_low;
    const float export_presence_floor = export_presence_floor_lp_.Process(ot_band);
    const float export_presence_knee  = presence_knee;
    const float export_presence_shape = 0.46f * export_presence_knee;
    const float export_presence_floor_gain = 0.0f;
    const float export_presence_flat_gain = 0.0f;
    const float export_low_lift       = export_low_lift_lp_.Process(ot_band);
    const float export_brilliance_low = export_brilliance_low_lp_.Process(ot_band);
    const float export_brilliance_high = export_brilliance_high_lp_.Process(ot_band);
    const float export_brilliance_band = export_brilliance_high - export_brilliance_low;
    const float export_air_low        = export_air_low_lp_.Process(ot_band);
    const float export_air_high       = export_air_high_lp_.Process(ot_band);
    const float export_air_band       = export_air_high - export_air_low;
    const float export_presence_air_relief = export_air_low - export_presence_high;
    const float export_tone           = ot_band + export_presence_flat_gain * ot_band
                              + 0.075f * export_low_lift
                              + 0.055f * export_brilliance_band
                              + export_presence_floor_gain * export_presence_floor
                              + export_presence_shape * export_presence_band
                              - 0.006f * presence_ * export_presence_air_relief
                              - 0.65f * export_air_band;
    const float exported             = export_lowpass_.Process(export_tone);
    float       output   = output_dc_.Process(kFixedOutputLevel * exported);

    // Limite di sicurezza numerico, non clipping progettuale. kMaxOutPk e'
    // volutamente alto: l'headroom reale del sistema viene deciso da input,
    // convolver e DAC, non da un limiter interno al modello Mesa.
    output = Clamp(output, -kMaxOutPk, kMaxOutPk);
    return output;
}

float MesaPowerAmp::ProcessPickAttack(float input)
{
    // Enhancer transiente pre-IR. Simula l'aumento momentaneo di energia nel
    // range "pick/click" prodotto da PI+finale+trasformatore quando il fronte
    // iniziale della pennata eccita il loop di feedback e il carico reattivo.
    // Non e' una Presence: non resta attivo sul sustain, perche' il gain dipende
    // dalla differenza fra inviluppo veloce e inviluppo lento della banda.

    // Banda di interesse della pennata. high-low costruisce un passa-banda largo
    // intorno a medio-alte/alte: abbastanza basso da prendere lo schiocco sulle
    // corde acute, abbastanza alto da non trasformare tutto il corpo della nota
    // in un boost fisso.
    const float low  = pick_low_lp_.Process(input);
    const float high = pick_high_lp_.Process(input);
    const float band = high - low;

    // Rilevatore di transiente. fast segue l'energia quasi subito, slow segue il
    // livello medio della stessa banda. Quando fast supera slow c'e' un attacco;
    // quando la nota diventa stabile, i due inviluppi si riallineano e il boost
    // sparisce. La soglia evita che rumore e fruscio aprano l'enhancer.
    const float energy = fabsf(band);
    const float fast   = pick_env_fast_lp_.Process(energy);
    const float slow   = pick_env_slow_lp_.Process(energy);

    // Quantita' di boost. Il fattore 1.12 richiede che il fronte sia davvero piu'
    // rapido del livello medio, 28.0 rende la zona utile piu' sensibile e Clamp
    // impedisce runaway. gain moltiplica solo la banda transiente, quindi il
    // livello fondamentale e il sustain rimangono quasi invariati.
    const float attack = Clamp((fast - 1.12f * slow - 0.00045f) * 28.0f, 0.0f, 1.0f);
    const float gain   = 0.95f * pick_attack_ * attack;

    // Somma parallela: segnale dry invariato piu' banda pick controllata
    // dinamicamente. In pratica e' analogo a una piccola enfasi dipendente dal
    // transiente, prima della IR, cosi' il cabinet virtuale puo' ancora filtrarla
    // come farebbe uno speaker reale.
    return input + gain * band;
}

float MesaPowerAmp::ProcessPresence(float input)
{
    const float p = Clamp(presence_, 0.0f, 1.0f);

    const float low  = post_presence_low_lp_.Process(input);
    const float high = post_presence_high_lp_.Process(input);
    const float air  = post_presence_air_lp_.Process(input);

    const float presence_band = high - low;
    const float air_band      = air - high;
    const float p2            = p * p;

    return input + (1.25f * p + 1.15f * p2) * presence_band + 0.22f * p2 * air_band;
}

float MesaPowerAmp::Clamp(float value, float lo, float hi)
{
    return value < lo ? lo : (value > hi ? hi : value);
}

float MesaPowerAmp::SoftClip(float x, float limit)
{
    const float safe_limit = limit > 0.001f ? limit : 0.001f;
    return safe_limit * tanhf(x / safe_limit);
}

float MesaPowerAmp::TubePairLaw(float x, float asymmetry)
{
    const float shifted = x + asymmetry * x * x;
    return SoftClip(shifted, 1.0f);
}

float MesaPowerAmp::TimeCoeff(float sample_rate, float seconds)
{
    return expf(-1.0f / (sample_rate * seconds));
}
