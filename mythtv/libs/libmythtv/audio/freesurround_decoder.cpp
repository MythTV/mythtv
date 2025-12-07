/*
Copyright (C) 2007 Christian Kothe

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include "freesurround_decoder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" {
#include "libavutil/mem.h"
#include "libavutil/tx.h"
}

using cfloat = std::complex<float>;
using InputBufs  = std::array<float*,2>;
using OutputBufs = std::array<float*,6>;

static const float PI = 3.141592654;
static const float epsilon = 0.000001;
static const float center_level = 0.5F*sqrtf(0.5F);

template <class T>
T sqr(T x) { return x*x; }

// private implementation of the surround decoder
class fsurround_decoder::Impl {
public:
    // create an instance of the decoder
    //  blocksize is fixed over the lifetime of this object for performance reasons
    explicit Impl(unsigned blocksize=8192)
      : m_n(blocksize),
        m_halfN(blocksize/2),
        // create lavc fft buffers
        m_dftL((AVComplexFloat*)av_malloc(sizeof(AVComplexFloat) * m_n * 2)),
        m_dftR((AVComplexFloat*)av_malloc(sizeof(AVComplexFloat) * m_n * 2)),
        m_src ((AVComplexFloat*)av_malloc(sizeof(AVComplexFloat) * m_n * 2))
    {
        // If av_tx_init() fails (returns < 0), the contexts will be nullptr and will crash later,
        // but av_malloc() is not checked to succeed either.
        av_tx_init(&m_fftContext , &m_fft , AV_TX_FLOAT_FFT, 0, m_n, &kScale, AV_TX_INPLACE);
        av_tx_init(&m_ifftContext, &m_ifft, AV_TX_FLOAT_FFT, 1, m_n, &kScale, AV_TX_INPLACE);
        // resize our own buffers
        m_frontR.resize(m_n);
        m_frontL.resize(m_n);
        m_avg.resize(m_n);
        m_surR.resize(m_n);
        m_surL.resize(m_n);
        m_trueavg.resize(m_n);
        m_xFs.resize(m_n);
        m_yFs.resize(m_n);
        m_inbuf[0].resize(m_n);
        m_inbuf[1].resize(m_n);
        for (unsigned c=0;c<6;c++) {
            m_outbuf[c].resize(m_n);
            m_filter[c].resize(m_n);
        }
        sample_rate(48000);
        // generate the window function (square root of hann, b/c it is applied before and after the transform)
        m_wnd.resize(m_n);
        for (unsigned k=0;k<m_n;k++)
            m_wnd[k] = std::sqrt(0.5F*(1-std::cos(2*PI*k/m_n))/m_n);
        m_currentBuf = 0;
        m_inbufs.fill(nullptr);
        m_outbufs.fill(nullptr);
        // set the default coefficients
        surround_coefficients(0.8165,0.5774);
        phase_mode(0);
        separation(1,1);
        steering_mode(true);
    }

    // destructor
    ~Impl() {
        av_tx_uninit(&m_fftContext);
        av_tx_uninit(&m_ifftContext);
        av_free(m_src);
        av_free(m_dftR);
        av_free(m_dftL);
    }

    float ** getInputBuffers()
    {
        m_inbufs[0] = &m_inbuf[0][m_currentBuf*m_halfN];
        m_inbufs[1] = &m_inbuf[1][m_currentBuf*m_halfN];
        return m_inbufs.data();
    }

    float ** getOutputBuffers()
    {
        m_outbufs[0] = &m_outbuf[0][m_currentBuf*m_halfN];
        m_outbufs[1] = &m_outbuf[1][m_currentBuf*m_halfN];
        m_outbufs[2] = &m_outbuf[2][m_currentBuf*m_halfN];
        m_outbufs[3] = &m_outbuf[3][m_currentBuf*m_halfN];
        m_outbufs[4] = &m_outbuf[4][m_currentBuf*m_halfN];
        m_outbufs[5] = &m_outbuf[5][m_currentBuf*m_halfN];
        return m_outbufs.data();
    }

    // decode a chunk of stereo sound, has to contain exactly blocksize samples
    //  center_width [0..1] distributes the center information towards the front left/right channels, 1=full distribution, 0=no distribution
    //  dimension [0..1] moves the soundfield backwards, 0=front, 1=side
    //  adaption_rate [0..1] determines how fast the steering gets adapted, 1=instantaneous, 0.1 = very slow adaption
    void decode(float center_width, float dimension, float adaption_rate) {
        // process first part
        int index = m_currentBuf*m_halfN;
        InputBufs in_second {&m_inbuf[0][index],&m_inbuf[1][index]};
        m_currentBuf ^= 1;
        index = m_currentBuf*m_halfN;
        InputBufs in_first {&m_inbuf[0][index],&m_inbuf[1][index]};
        add_output(in_first,in_second,center_width,dimension,adaption_rate,true);
        // shift last half of input buffer to the beginning
    }
    
    // flush the internal buffers
    void flush() {
        for (unsigned k=0;k<m_n;k++) {
            for (auto & c : m_outbuf)
                c[k] = 0;
            m_inbuf[0][k] = 0;
            m_inbuf[1][k] = 0;
        }
    }

    // set lfe filter params
    void sample_rate(unsigned int srate) {
        // lfe filter is just straight through band limited
        unsigned int cutoff = (30*m_n)/srate;
        for (unsigned f=0;f<=m_halfN;f++) {
            if (f<cutoff)
                m_filter[5][f] = 0.5*sqrt(0.5);
            else
                m_filter[5][f] = 0.0;
        }
    }

    // set the assumed surround mixing coefficients
    void surround_coefficients(float a, float b) {
        // calc the simple coefficients
        m_surroundHigh = a;
        m_surroundLow = b;
        m_surroundBalance = (a-b)/(a+b);
        m_surroundLevel = 1/(a+b);
        // calc the linear coefficients
        cfloat i(0,1);
        cfloat u((a+b)*i);
        cfloat v((b-a)*i);
        cfloat n(0.25,0);
        cfloat o(1,0);
        m_a = (v-o)*n; m_b = (o-u)*n; m_c = (-o-v)*n; m_d = (o+u)*n;
        m_e = (o+v)*n; m_f = (o+u)*n; m_g = (o-v)*n;  m_h = (o-u)*n;
    }

    // set the phase shifting mode
    void phase_mode(unsigned mode) {
        const std::array<std::array<float,2>,4> modes {{ {0,0}, {0,PI}, {PI,0}, {-PI/2,PI/2} }};
        m_phaseOffsetL = modes[mode][0];
        m_phaseOffsetR = modes[mode][1];
    }

    // what steering mode should be chosen
    void steering_mode(bool mode) { m_linearSteering = mode; }

    // set front & rear separation controls
    void separation(float front, float rear) {
        m_frontSeparation = front;
        m_rearSeparation = rear;
    }

private:
    // polar <-> cartesian coodinates conversion
    static cfloat polar(float a, float p) { return {a*std::cos(p), a*std::sin(p)}; }
    static float amplitude(AVComplexFloat z) { return std::hypot(z.re, z.im); }
    static float phase(AVComplexFloat z) { return std::atan2(z.im, z.re); }

    /// Clamp the input to the interval [-1, 1], i.e. clamp the magnitude to the unit interval [0, 1]
    static float clamp_unit_mag(float x) { return std::clamp(x, -1.0F, 1.0F); }

    // handle the output buffering for overlapped calls of block_decode
    void add_output(InputBufs input1, InputBufs input2, float center_width, float dimension, float adaption_rate, bool /*result*/=false) {
        // add the windowed data to the last 1/2 of the output buffer
        OutputBufs out {m_outbuf[0].data(),m_outbuf[1].data(),m_outbuf[2].data(),m_outbuf[3].data(),m_outbuf[4].data(),m_outbuf[5].data()};
        block_decode(input1,input2,out,center_width,dimension,adaption_rate);
    }

    // CORE FUNCTION: decode a block of data
    void block_decode(InputBufs input1, InputBufs input2, OutputBufs output, float center_width, float dimension, float adaption_rate) {
        // 1. scale the input by the window function; this serves a dual purpose:
        // - first it improves the FFT resolution b/c boundary discontinuities (and their frequencies) get removed
        // - second it allows for smooth blending of varying filters between the blocks

        // the window also includes 1.0/sqrt(n) normalization
        // concatenate copies of input1 and input2 for some undetermined reason
        // input1 is in the rising half of the window
        // input2 is in the falling half of the window
        for (unsigned k = 0; k < m_halfN; k++)
        {
            m_dftL[k]           = (AVComplexFloat){ .re = input1[0][k] * m_wnd[k], .im = 0 };
            m_dftR[k]           = (AVComplexFloat){ .re = input1[1][k] * m_wnd[k], .im = 0 };

            m_dftL[k + m_halfN] = (AVComplexFloat){ .re = input2[0][k] * m_wnd[k + m_halfN], .im = 0 };
            m_dftR[k + m_halfN] = (AVComplexFloat){ .re = input2[1][k] * m_wnd[k + m_halfN], .im = 0 };
        }

        // ... and tranform it into the frequency domain
        m_fft(m_fftContext, m_dftL, m_dftL, sizeof(AVComplexFloat));
        m_fft(m_fftContext, m_dftR, m_dftR, sizeof(AVComplexFloat));

        // 2. compare amplitude and phase of each DFT bin and produce the X/Y coordinates in the sound field
        //    but dont do DC or N/2 component
        for (unsigned f=0;f<m_halfN;f++) {
            // get left/right amplitudes/phases
            float ampL = amplitude(m_dftL[f]);
            float ampR = amplitude(m_dftR[f]);
            float phaseL = phase(m_dftL[f]);
            float phaseR = phase(m_dftR[f]);
//          if (ampL+ampR < epsilon)
//              continue;       

            // calculate the amplitude/phase difference
            float ampDiff = clamp_unit_mag((ampL+ampR < epsilon) ? 0 : (ampR-ampL) / (ampR+ampL));
            float phaseDiff = phaseL - phaseR;
            if (phaseDiff < -PI) phaseDiff += 2*PI;
            if (phaseDiff > PI) phaseDiff -= 2*PI;
            phaseDiff = std::abs(phaseDiff);

            if (m_linearSteering) {
                // --- this is the fancy new linear mode ---

                // get sound field x/y position
                m_yFs[f] = get_yfs(ampDiff,phaseDiff);
                m_xFs[f] = get_xfs(ampDiff,m_yFs[f]);

                // add dimension control
                m_yFs[f] = clamp_unit_mag(m_yFs[f] - dimension);

                // add crossfeed control
                m_xFs[f] = clamp_unit_mag(m_xFs[f] * (m_frontSeparation*(1+m_yFs[f])/2 + m_rearSeparation*(1-m_yFs[f])/2));

                // 3. generate frequency filters for each output channel
                float left = (1-m_xFs[f])/2;
                float right = (1+m_xFs[f])/2;
                float front = (1+m_yFs[f])/2;
                float back = (1-m_yFs[f])/2;
                std::array<float, 5> volume
                {
                    front * (left  * center_width + std::max(0.0F, -m_xFs[f]) * (1.0F - center_width) ), // left
                    front * center_level * ( (1.0F - std::abs(m_xFs[f])) * (1.0F - center_width) ),      // center
                    front * (right * center_width + std::max(0.0F,  m_xFs[f]) * (1.0F - center_width) ), // right
                    back * m_surroundLevel * left,                                        // left surround
                    back * m_surroundLevel * right                                        // right surround
                };

                // adapt the prior filter
                for (unsigned c=0;c<5;c++)
                    m_filter[c][f] = ((1-adaption_rate)*m_filter[c][f]) + (adaption_rate*volume[c]);

            } else {
                // --- this is the old & simple steering mode ---

                // determine sound field x-position
                m_xFs[f] = ampDiff;

                // determine preliminary sound field y-position from phase difference
                m_yFs[f] = 1 - ((phaseDiff/PI)*2);

                if (std::abs(m_xFs[f]) > m_surroundBalance) {
                    // blend linearly between the surrounds and the fronts if the balance exceeds the surround encoding balance
                    // this is necessary because the sound field is trapezoidal and will be stretched behind the listener
                    float frontness = (std::abs(m_xFs[f]) - m_surroundBalance)/(1-m_surroundBalance);
                    m_yFs[f]  = ((1-frontness) * m_yFs[f]) + (frontness * 1);
                }

                // add dimension control
                m_yFs[f] = clamp_unit_mag(m_yFs[f] - dimension);

                // add crossfeed control
                m_xFs[f] = clamp_unit_mag(m_xFs[f] * (m_frontSeparation*(1+m_yFs[f])/2 + m_rearSeparation*(1-m_yFs[f])/2));

                // 3. generate frequency filters for each output channel, according to the signal position
                // the sum of all channel volumes must be 1.0
                float left = (1-m_xFs[f])/2;
                float right = (1+m_xFs[f])/2;
                float front = (1+m_yFs[f])/2;
                float back = (1-m_yFs[f])/2;
                std::array<float, 5> volume
                {
                    front * (left  * center_width + std::max(0.0F, -m_xFs[f]) * (1.0F-center_width)), // left
                    front * center_level * ( (1.0F - std::abs(m_xFs[f])) * (1.0F - center_width) ),   // center
                    front * (right * center_width + std::max(0.0F,  m_xFs[f]) * (1.0F-center_width)), // right
                    back * m_surroundLevel * std::clamp( (1.0F - (m_xFs[f] / m_surroundBalance) ) / 2.0F, 0.0F, 1.0F),// left surround
                    back * m_surroundLevel * std::clamp( (1.0F + (m_xFs[f] / m_surroundBalance) ) / 2.0F, 0.0F, 1.0F) // right surround
                };

                // adapt the prior filter
                for (unsigned c=0;c<5;c++)
                    m_filter[c][f] = ((1-adaption_rate)*m_filter[c][f]) + (adaption_rate*volume[c]);
            }

            // ... and build the signal which we want to position
            m_frontL[f] = polar(ampL+ampR,phaseL);
            m_frontR[f] = polar(ampL+ampR,phaseR);
            m_avg[f] = m_frontL[f] + m_frontR[f];
            m_surL[f] = polar(ampL+ampR,phaseL+m_phaseOffsetL);
            m_surR[f] = polar(ampL+ampR,phaseR+m_phaseOffsetR);
            m_trueavg[f] = cfloat(m_dftL[f].re + m_dftR[f].re, m_dftL[f].im + m_dftR[f].im);
        }

        // 4. distribute the unfiltered reference signals over the channels
        apply_filter((m_frontL).data(), m_filter[0].data(),&output[0][0]);  // front left
        apply_filter((m_avg).data(),    m_filter[1].data(),&output[1][0]);  // front center
        apply_filter((m_frontR).data(), m_filter[2].data(),&output[2][0]);  // front right
        apply_filter((m_surL).data(),   m_filter[3].data(),&output[3][0]);  // surround left
        apply_filter((m_surR).data(),   m_filter[4].data(),&output[4][0]);  // surround right
        apply_filter((m_trueavg).data(),m_filter[5].data(),&output[5][0]);  // lfe
    }

#define FASTER_CALC
    // map from amplitude difference and phase difference to yfs
    static double get_yfs(double ampDiff, double phaseDiff) {
        double x = 1-(((1-sqr(ampDiff))*phaseDiff)/M_PI*2);
#ifdef FASTER_CALC
        double tanX = tan(x);
        return 0.16468622925824683 + (0.5009268347818189*x) - (0.06462757726992101*x*x)
            + (0.09170680403453149*x*x*x) + (0.2617754892323973*tanX) - (0.04180413533856156*sqr(tanX));
#else
        return 0.16468622925824683 + (0.5009268347818189*x) - (0.06462757726992101*x*x)
            + (0.09170680403453149*x*x*x) + (0.2617754892323973*tan(x)) - (0.04180413533856156*sqr(tan(x)));
#endif
    }

    // map from amplitude difference and yfs to xfs
    static double get_xfs(double ampDiff, double yfs) {
        double x=ampDiff;
        double y=yfs;
#ifdef FASTER_CALC
        double tanX = tan(x);
        double tanY = tan(y);
        double asinX = asin(x);
        double sinX = sin(x);
        double sinY = sin(y);
        double x3 = x*x*x;
        double y2 = y*y;
        double y3 = y*y2;
        return (2.464833559224702*x) - (423.52131153259404*x*y) +
            (67.8557858606918*x3*y) + (788.2429425544392*x*y2) -
            (79.97650354902909*x3*y2) - (513.8966153850349*x*y3) +
            (35.68117670186306*x3*y3) + (13867.406173420834*y*asinX) -
            (2075.8237075786396*y2*asinX) - (908.2722068360281*y3*asinX) -
            (12934.654772878019*asinX*sinY) - (13216.736529661162*y*tanX) +
            (1288.6463247741938*y2*tanX) + (1384.372969378453*y3*tanX) +
            (12699.231471126128*sinY*tanX) + (95.37131275594336*sinX*tanY) -
            (91.21223198407546*tanX*tanY);
#else
        return (2.464833559224702*x) - (423.52131153259404*x*y) +
            (67.8557858606918*x*x*x*y) + (788.2429425544392*x*y*y) -
            (79.97650354902909*x*x*x*y*y) - (513.8966153850349*x*y*y*y) +
            (35.68117670186306*x*x*x*y*y*y) + (13867.406173420834*y*asin(x)) -
            (2075.8237075786396*y*y*asin(x)) - (908.2722068360281*y*y*y*asin(x)) -
            (12934.654772878019*asin(x)*sin(y)) - (13216.736529661162*y*tan(x)) +
            (1288.6463247741938*y*y*tan(x)) + (1384.372969378453*y*y*y*tan(x)) +
            (12699.231471126128*sin(y)*tan(x)) + (95.37131275594336*sin(x)*tan(y)) -
            (91.21223198407546*tan(x)*tan(y));
#endif
    }

    /**
    @brief Filter the complex source signal in the frequency domain and add it
           to the time domain target signal.

    @param[in]  signal  The signal, in the frequency domain, to be filtered.
    @param[in]  flt     The filter, in the frequency domain, to be applied.
    @param[out] target  The signal, in the time domain, to which the filtered signal
                        is added
    */
    void apply_filter(cfloat *signal, const float *flt, float *target) {
        // filter the signal
        for (unsigned f = 0; f <= m_halfN; f++)
        {
            m_src[f].re = signal[f].real() * flt[f];
            m_src[f].im = signal[f].imag() * flt[f];
        }
        // enforce odd symmetry
        for (unsigned f = 1; f < m_halfN; f++)
        {
            m_src[m_n - f].re =  m_src[f].re;
            m_src[m_n - f].im = -m_src[f].im;   // complex conjugate
        }
        m_ifft(m_ifftContext, m_src, m_src, sizeof(AVComplexFloat));

        // add the result to target, windowed
        for (unsigned int k = 0; k < m_halfN; k++)
        {
            // 1st part is overlap add
            target[(m_currentBuf * m_halfN) + k]      += m_src[k].re * m_wnd[k];
            // 2nd part is set as has no history
            target[((m_currentBuf ^ 1) * m_halfN) + k] = m_src[m_halfN + k].re * m_wnd[m_halfN + k];
        }
    }

    size_t m_n;                          // the block size
    size_t m_halfN;                      // half block size precalculated
    static constexpr float kScale {1.0F};
    AVTXContext *m_fftContext  {nullptr};
    av_tx_fn     m_fft         {nullptr};
    AVTXContext *m_ifftContext {nullptr};
    av_tx_fn     m_ifft        {nullptr};
    // FFTs are computed in-place in these buffers on copies of the input
    AVComplexFloat *m_dftL {nullptr};
    AVComplexFloat *m_dftR {nullptr};
    AVComplexFloat *m_src  {nullptr}; ///< Used only in apply_filter
    // buffers
    std::vector<cfloat> m_frontL,m_frontR,m_avg,m_surL,m_surR; // the signal (phase-corrected) in the frequency domain
    std::vector<cfloat> m_trueavg;       // for lfe generation
    std::vector<float> m_xFs,m_yFs;      // the feature space positions for each frequency bin
    std::vector<float> m_wnd;            // the window function, precalculated
    std::array<std::vector<float>,6> m_filter;      // a frequency filter for each output channel
    std::array<std::vector<float>,2> m_inbuf;       // the sliding input buffers
    std::array<std::vector<float>,6> m_outbuf;      // the sliding output buffers
    // coefficients
    float m_surroundHigh    {0.0F};      // high surround mixing coefficient (e.g. 0.8165/0.5774)
    float m_surroundLow     {0.0F};      // low surround mixing coefficient (e.g. 0.8165/0.5774)
    float m_surroundBalance {0.0F};      // the xfs balance that follows from the coeffs
    float m_surroundLevel   {0.0F};      // gain for the surround channels (follows from the coeffs
    float m_phaseOffsetL    {0.0F};      // phase shifts to be applied to the rear channels
    float m_phaseOffsetR    {0.0F};      // phase shifts to be applied to the rear channels
    float m_frontSeparation {0.0F};      // front stereo separation
    float m_rearSeparation  {0.0F};      // rear stereo separation
    bool  m_linearSteering  {false};     // whether the steering should be linear or not
    cfloat m_a,m_b,m_c,m_d,m_e,m_f,m_g,m_h; // coefficients for the linear steering
    int m_currentBuf;                    // specifies which buffer is 2nd half of input sliding buffer
    InputBufs  m_inbufs     {};          // for passing back to driver
    OutputBufs m_outbufs    {};          // for passing back to driver
};


// implementation of the shell class

fsurround_decoder::fsurround_decoder(unsigned blocksize): m_impl(new Impl(blocksize)) { }

fsurround_decoder::~fsurround_decoder() { delete m_impl; }

void fsurround_decoder::decode(float center_width, float dimension, float adaption_rate) {
    m_impl->decode(center_width,dimension,adaption_rate);
}

void fsurround_decoder::flush() { m_impl->flush(); }

void fsurround_decoder::surround_coefficients(float a, float b) { m_impl->surround_coefficients(a,b); }

void fsurround_decoder::phase_mode(unsigned mode) { m_impl->phase_mode(mode); }

void fsurround_decoder::steering_mode(bool mode) { m_impl->steering_mode(mode); }

void fsurround_decoder::separation(float front, float rear) { m_impl->separation(front,rear); }

float ** fsurround_decoder::getInputBuffers()
{
    return m_impl->getInputBuffers();
}

float ** fsurround_decoder::getOutputBuffers()
{
    return m_impl->getOutputBuffers();
}

void fsurround_decoder::sample_rate(unsigned int samplerate)
{
    m_impl->sample_rate(samplerate);
}
