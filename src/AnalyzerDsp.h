#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

// Motor de medicion de INJUSTICE ANALYZER.
//  - LoudnessMeter : LUFS (BS.1770-4) con compuerta + LRA (EBU Tech 3342). Corre en el hilo de audio.
//  - LevelMeter    : pico de muestra y RMS (ventana de 300 ms) por canal. Corre en el hilo de audio.
//  - SampleRing    : buffer circular sin bloqueos para pasar muestras al hilo de interfaz.
//  - SpectrumEngine: FFT real, bandas logaritmicas de 16 Hz a 20 kHz y balistica. Corre en el hilo de interfaz.
namespace an
{
    constexpr float kFloorDb = -200.0f;

    inline float powerToDb (double p) { return p > 1e-20 ? (float) (10.0 * std::log10 (p)) : kFloorDb; }

    inline void atomicMax (std::atomic<float>& a, float v)
    {
        float cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }

    //==========================================================================================
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        inline double process (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void reset() { z1 = z2 = 0; }
    };

    // Filtro K de ITU-R BS.1770-4: etapa 1 (shelf alto) + etapa 2 (pasa-altos RLB) para cualquier fs.
    inline void kWeighting (double fs, Biquad& shelf, Biquad& hp)
    {
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
            shelf.b1 = 2.0 * (K * K - Vh) / a0;
            shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
            shelf.a1 = 2.0 * (K * K - 1.0) / a0;
            shelf.a2 = (1.0 - K / Q + K * K) / a0;
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            hp.b0 = 1.0; hp.b1 = -2.0; hp.b2 = 1.0;
            hp.a1 = 2.0 * (K * K - 1.0) / a0;
            hp.a2 = (1.0 - K / Q + K * K) / a0;
        }
    }

    //==========================================================================================
    class LoudnessMeter
    {
    public:
        static constexpr int kBins = 8000;      // de -70 a +10 LUFS en pasos de 0.01 LU
        static constexpr int kShortBlocks = 30; // 3 s en bloques de 100 ms
        static constexpr int kMomBlocks = 4;    // 400 ms

        void prepare (double sampleRate)
        {
            fs = sampleRate;
            blockLen = std::max (1, (int) std::lround (sampleRate * 0.1));
            kWeighting (fs, shelf[0], hp[0]);
            kWeighting (fs, shelf[1], hp[1]);
            eM.assign (kBins, 0.0); cM.assign (kBins, 0);
            eS.assign (kBins, 0.0); cS.assign (kBins, 0);
            resetState();
        }

        void requestReset() { resetReq.store (true); }

        void process (const float* l, const float* r, int n)
        {
            if (resetReq.exchange (false)) resetState();
            for (int i = 0; i < n; ++i)
            {
                const double yl = hp[0].process (shelf[0].process ((double) l[i]));
                const double yr = hp[1].process (shelf[1].process ((double) r[i]));
                sumL += yl * yl;
                sumR += yr * yr;
                if (++count >= blockLen)
                {
                    endBlock ((sumL + sumR) / (double) blockLen);
                    count = 0; sumL = sumR = 0.0;
                }
            }
        }

        float momentary()  const { return mom.load(); }
        float shortTerm()  const { return st.load(); }
        float integrated() const { return integ.load(); }
        float range()      const { return lraV.load(); }

        static double toLufs (double z) { return z > 1e-12 ? -0.691 + 10.0 * std::log10 (z) : -200.0; }

    private:
        void resetState()
        {
            for (auto& s : shelf) s.reset();
            for (auto& h : hp) h.reset();
            std::fill (eM.begin(), eM.end(), 0.0); std::fill (cM.begin(), cM.end(), 0u);
            std::fill (eS.begin(), eS.end(), 0.0); std::fill (cS.begin(), cS.end(), 0u);
            totEM = totES = 0.0; totCM = totCS = 0;
            for (auto& b : blocks) b = 0.0;
            bi = 0; nBlocks = 0; count = 0; sumL = sumR = 0.0;
            mom.store (-200.f); st.store (-200.f); integ.store (-200.f); lraV.store (0.f);
        }

        double meanLast (int k) const
        {
            double s = 0.0;
            for (int i = 1; i <= k; ++i) s += blocks[(size_t) ((bi - i + 2 * kShortBlocks) % kShortBlocks)];
            return s / (double) k;
        }

        static int binOf (double lufs) { return juce::jlimit (0, kBins - 1, (int) std::floor ((lufs + 70.0) * 100.0)); }
        static int firstBinAtOrAbove (double lufs) { return juce::jlimit (0, kBins, (int) std::ceil ((lufs + 70.0) * 100.0)); }

        void endBlock (double z)
        {
            blocks[(size_t) bi] = z;
            bi = (bi + 1) % kShortBlocks;
            nBlocks = std::min (nBlocks + 1, kShortBlocks);

            mom.store ((float) toLufs (meanLast (std::min (kMomBlocks, nBlocks))));
            st.store ((float) toLufs (meanLast (nBlocks)));

            if (nBlocks >= kMomBlocks)
            {
                const double zm = meanLast (kMomBlocks), lm = toLufs (zm);
                if (lm > -70.0) { const int b = binOf (lm); eM[(size_t) b] += zm; ++cM[(size_t) b]; totEM += zm; ++totCM; }
            }
            if (nBlocks >= kShortBlocks)
            {
                const double zs = meanLast (kShortBlocks), ls = toLufs (zs);
                if (ls > -70.0) { const int b = binOf (ls); eS[(size_t) b] += zs; ++cS[(size_t) b]; totES += zs; ++totCS; }
            }
            computeIntegrated();
            computeRange();
        }

        void computeIntegrated()
        {
            if (totCM == 0) { integ.store (-200.f); return; }
            const double gate = toLufs (totEM / (double) totCM) - 10.0;   // compuerta relativa: -10 LU
            double e = 0.0; uint64_t c = 0;
            for (int i = firstBinAtOrAbove (gate); i < kBins; ++i) { e += eM[(size_t) i]; c += cM[(size_t) i]; }
            integ.store (c > 0 ? (float) toLufs (e / (double) c) : -200.f);
        }

        void computeRange()
        {
            if (totCS < 2) { lraV.store (0.f); return; }
            const double gate = toLufs (totES / (double) totCS) - 20.0;   // compuerta relativa: -20 LU
            const int start = firstBinAtOrAbove (gate);
            uint64_t n = 0;
            for (int i = start; i < kBins; ++i) n += cS[(size_t) i];
            if (n < 2) { lraV.store (0.f); return; }
            const uint64_t r10 = (uint64_t) std::llround (0.10 * (double) (n - 1));
            const uint64_t r95 = (uint64_t) std::llround (0.95 * (double) (n - 1));
            double v10 = 0.0, v95 = 0.0; bool g10 = false, g95 = false;
            uint64_t cum = 0;
            for (int i = start; i < kBins && ! (g10 && g95); ++i)
            {
                cum += cS[(size_t) i];
                const double centre = -70.0 + ((double) i + 0.5) * 0.01;
                if (! g10 && cum > r10) { v10 = centre; g10 = true; }
                if (! g95 && cum > r95) { v95 = centre; g95 = true; }
            }
            lraV.store ((float) std::max (0.0, v95 - v10));
        }

        double fs = 48000.0;
        int blockLen = 4800, count = 0;
        Biquad shelf[2], hp[2];
        double sumL = 0.0, sumR = 0.0;
        double blocks[kShortBlocks] {};
        int bi = 0, nBlocks = 0;
        std::vector<double> eM, eS;
        std::vector<uint32_t> cM, cS;
        double totEM = 0.0, totES = 0.0;
        uint64_t totCM = 0, totCS = 0;
        std::atomic<bool> resetReq { false };
        std::atomic<float> mom { -200.f }, st { -200.f }, integ { -200.f }, lraV { 0.f };
    };

    //==========================================================================================
    // Pico de muestra (maximo desde la ultima lectura) y RMS de 300 ms por canal.
    class LevelMeter
    {
    public:
        void prepare (double sampleRate)
        {
            winLen = std::max (1, (int) std::lround (sampleRate * 0.3));
            for (int c = 0; c < 2; ++c)
            {
                sq[c].assign ((size_t) winLen, 0.f);
                sum[c] = 0.0;
                peak[c].store (0.f);
                ms[c].store (0.f);
            }
            pos = 0; sinceResum = 0;
        }

        void process (const float* const* ch, int n)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float* x = ch[c];
                float pk = 0.f;
                int p = pos;
                double s = sum[c];
                float* q = sq[c].data();
                for (int i = 0; i < n; ++i)
                {
                    const float a = std::abs (x[i]);
                    if (a > pk) pk = a;
                    const float v = x[i] * x[i];
                    s += (double) v - (double) q[p];
                    q[p] = v;
                    if (++p == winLen) p = 0;
                }
                sum[c] = s;
                atomicMax (peak[c], pk);
            }
            pos = (pos + n) % winLen;
            sinceResum += n;
            if (sinceResum > 1 << 18)       // suma exacta cada cierto tiempo: sin deriva numerica
            {
                sinceResum = 0;
                for (int c = 0; c < 2; ++c)
                {
                    double s = 0.0;
                    for (float v : sq[c]) s += (double) v;
                    sum[c] = s;
                }
            }
            for (int c = 0; c < 2; ++c)
                ms[c].store ((float) std::max (0.0, sum[c] / (double) winLen));
        }

        float takePeak (int c) { return peak[c].exchange (0.f); }   // pico lineal desde la ultima lectura
        float meanSquare (int c) const { return ms[c].load(); }

    private:
        int winLen = 14400, pos = 0, sinceResum = 0;
        std::vector<float> sq[2];
        double sum[2] { 0.0, 0.0 };
        std::atomic<float> peak[2], ms[2];
    };

    //==========================================================================================
    struct SampleRing
    {
        static constexpr size_t N = 1u << 17;
        SampleRing() : data (N, 0.f) {}

        void push (const float* x, int n)
        {
            uint64_t w = wr.load (std::memory_order_relaxed);
            for (int i = 0; i < n; ++i) data[(size_t) ((w + (uint64_t) i) & (N - 1))] = x[i];
            wr.store (w + (uint64_t) n, std::memory_order_release);
        }
        uint64_t writeIndex() const { return wr.load (std::memory_order_acquire); }

        // copia las ultimas `count` muestras; false si el escritor las piso mientras se leia
        bool readLatest (float* dst, int count) const
        {
            const uint64_t w = wr.load (std::memory_order_acquire);
            const uint64_t first = w >= (uint64_t) count ? w - (uint64_t) count : 0;
            for (int i = 0; i < count; ++i)
            {
                const uint64_t idx = first + (uint64_t) i;
                dst[i] = (w >= (uint64_t) count || idx < w) ? data[(size_t) (idx & (N - 1))] : 0.f;
            }
            const uint64_t w2 = wr.load (std::memory_order_acquire);
            return (w2 - first) <= (uint64_t) (N - 64);
        }

        std::vector<float> data;
        std::atomic<uint64_t> wr { 0 };
    };

    //==========================================================================================
    enum Detection { Left = 0, Right, LRmax, Mono };
    enum SpecMode { RmsSlow = 0, RmsFast, Peak };

    class SpectrumEngine
    {
    public:
        static constexpr double kFLo = 16.0, kFHi = 20000.0;

        void prepare (double sampleRate, int order, int nBands)
        {
            fs = sampleRate;
            fftOrder = order;
            N = 1 << order;
            fft = std::make_unique<juce::dsp::FFT> (order);
            window.resize ((size_t) N);
            double sw2 = 0.0;
            for (int i = 0; i < N; ++i)                       // Hann periodica
            {
                const double w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / N);
                window[(size_t) i] = (float) w;
                sw2 += w * w;
            }
            normPower = (float) (2.0 / ((double) N * sw2));  // potencia unilateral en media cuadratica
            fftData.assign ((size_t) (2 * N), 0.f);
            bufL.assign ((size_t) N, 0.f);
            bufR.assign ((size_t) N, 0.f);
            binPower.assign ((size_t) (N / 2 + 1), 0.f);
            setBands (nBands);
            lastWr = 0; haveFrame = false;
        }

        void setBands (int n)
        {
            nb = n;
            edges.resize ((size_t) n + 1);
            const double ratio = std::pow (kFHi / kFLo, 1.0 / n);
            for (int i = 0; i <= n; ++i) edges[(size_t) i] = kFLo * std::pow (ratio, i);

            weights.assign ((size_t) n, {});
            firstBin.assign ((size_t) n, 0);
            const double df = fs / N;
            auto C = [] (double t)       // integral acumulada de la funcion triangulo
            {
                if (t <= -1.0) return 0.0;
                if (t <= 0.0) return 0.5 * (t + 1.0) * (t + 1.0);
                if (t < 1.0) return 1.0 - 0.5 * (1.0 - t) * (1.0 - t);
                return 1.0;
            };
            for (int b = 0; b < n; ++b)
            {
                const double lo = edges[(size_t) b], hi = edges[(size_t) b + 1];
                const int k0 = std::max (0, (int) std::floor (lo / df) - 1);
                const int k1 = std::min (N / 2, (int) std::ceil (hi / df) + 1);
                firstBin[(size_t) b] = k0;
                auto& w = weights[(size_t) b];
                w.clear();
                for (int k = k0; k <= k1; ++k)
                {
                    const double fk = k * df;
                    w.push_back ((float) (C ((hi - fk) / df) - C ((lo - fk) / df)));
                }
            }
            avgPower.assign ((size_t) n, 0.f);
            bandPow.assign ((size_t) n, 0.f);
            bandPowR.assign ((size_t) n, 0.f);
            disp.assign ((size_t) n, kFloorDb);
            marker.assign ((size_t) n, kFloorDb);
            holdLeft.assign ((size_t) n, 0.f);
            haveFrame = false;
        }

        int numBands() const { return edges.empty() ? 0 : nb; }   // 0 mientras no se ha preparado
        const std::vector<double>& bandEdges() const { return edges; }
        const std::vector<float>& levels() const { return disp; }
        const std::vector<float>& peaks() const { return marker; }
        int fftSize() const { return N; }

        void resetPeaks()
        {
            for (size_t i = 0; i < disp.size(); ++i) { marker[i] = disp[i]; holdLeft[i] = 0.f; }
        }

        // Una pasada de analisis + balistica. Devuelve true si hay algo que redibujar.
        bool update (const SampleRing& rl, const SampleRing& rr, int detection, int mode, double wallDt,
                     float rateDbPerSec, double holdSec, bool holdLatched)
        {
            const uint64_t wr = rl.writeIndex();
            const uint64_t adv = wr - lastWr;
            const bool fresh = adv >= (uint64_t) std::max (64.0, fs * 0.005) && wr != 0;
            bool gotFrame = false;
            double frameDt = 0.0;

            if (fresh)
            {
                const bool needL = detection != Right, needR = detection != Left;
                bool ok = true;
                if (needL) ok = rl.readLatest (bufL.data(), N) && ok;
                if (needR) ok = rr.readLatest (bufR.data(), N) && ok;
                if (ok)
                {
                    lastWr = wr;
                    frameDt = std::min (0.25, (double) adv / fs);
                    switch (detection)
                    {
                        case Left:  analyse (bufL.data(), bandPow); break;
                        case Right: analyse (bufR.data(), bandPow); break;
                        case Mono:
                            for (int i = 0; i < N; ++i) bufL[(size_t) i] = 0.5f * (bufL[(size_t) i] + bufR[(size_t) i]);
                            analyse (bufL.data(), bandPow);
                            break;
                        default:
                            analyse (bufL.data(), bandPow);
                            analyse (bufR.data(), bandPowR);
                            for (int b = 0; b < nb; ++b) bandPow[(size_t) b] = std::max (bandPow[(size_t) b], bandPowR[(size_t) b]);
                            break;
                    }
                    gotFrame = true;
                }
            }

            const double tau = mode == RmsSlow ? 1.0 : 0.125;      // ponderacion temporal Slow / Fast
            const float fall = (float) (rateDbPerSec * wallDt);
            for (int b = 0; b < nb; ++b)
            {
                const size_t i = (size_t) b;
                float target = kFloorDb;
                if (gotFrame)
                {
                    if (mode == Peak) target = powerToDb (2.0 * bandPow[i]);
                    else
                    {
                        if (! haveFrame) avgPower[i] = bandPow[i];
                        const float a = (float) (1.0 - std::exp (-frameDt / tau));
                        avgPower[i] += a * (bandPow[i] - avgPower[i]);
                        target = powerToDb (2.0 * avgPower[i]);
                    }
                }
                // sin audio nuevo (transporte detenido): las barras solo caen por el Return Rate
                if (! gotFrame) { avgPower[i] = 0.f; target = kFloorDb; }

                if (target >= disp[i]) disp[i] = target;
                else disp[i] = std::max (target, disp[i] - fall);

                // indicador de pico
                if (holdLatched)                                  { if (disp[i] > marker[i]) marker[i] = disp[i]; }
                else if (disp[i] >= marker[i])                    { marker[i] = disp[i]; holdLeft[i] = (float) holdSec; }
                else if (holdLeft[i] > 0.f)                       holdLeft[i] -= (float) wallDt;
                else                                              marker[i] = std::max (disp[i], marker[i] - fall);
            }
            haveFrame = gotFrame;
            return true;
        }

        // Para pruebas: analiza una ventana arbitraria y devuelve la potencia por banda (media cuadratica).
        const std::vector<float>& analyseBlock (const float* x)
        {
            analyse (x, bandPow);
            return bandPow;
        }

    private:
        void analyse (const float* x, std::vector<float>& outBandPow)
        {
            for (int i = 0; i < N; ++i) fftData[(size_t) i] = x[i] * window[(size_t) i];
            std::fill (fftData.begin() + N, fftData.end(), 0.f);
            fft->performRealOnlyForwardTransform (fftData.data(), true);
            for (int k = 0; k <= N / 2; ++k)
            {
                const float re = fftData[(size_t) (2 * k)], im = fftData[(size_t) (2 * k + 1)];
                binPower[(size_t) k] = (re * re + im * im) * normPower;
            }
            for (int b = 0; b < nb; ++b)
            {
                const auto& w = weights[(size_t) b];
                const int k0 = firstBin[(size_t) b];
                double s = 0.0;
                for (size_t j = 0; j < w.size(); ++j) s += (double) w[j] * (double) binPower[(size_t) k0 + j];
                outBandPow[(size_t) b] = (float) s;
            }
        }

        double fs = 48000.0;
        int fftOrder = 14, N = 16384, nb = 63;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> window, fftData, bufL, bufR, binPower;
        float normPower = 1.f;
        std::vector<double> edges;
        std::vector<std::vector<float>> weights;
        std::vector<int> firstBin;
        std::vector<float> avgPower, bandPow, bandPowR, disp, marker, holdLeft;
        uint64_t lastWr = 0;
        bool haveFrame = false;
    };
}
