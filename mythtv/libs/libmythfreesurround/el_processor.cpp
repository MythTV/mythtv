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

#include "el_processor.h"
#include <cmath>
#include <complex>
#include <cstdlib>
#include <cstring>
#include <vector>
#ifdef USE_FFTW3
#include "fftw3.h"
#else
extern "C" {
#include "libavcodec/avfft.h"
#include "libavcodec/fft.h"
}
typedef FFTSample FFTComplexArray[2];
#endif


#if defined(_WIN32) && defined(USE_FFTW3)
#pragma comment (lib,"libfftw3f-3.lib")
#endif

typedef std::complex<float> cfloat;

static const float PI = 3.141592654;
static const float epsilon = 0.000001;
static const float center_level = 0.5*sqrt(0.5);

// private implementation of the surround decoder
class decoder_impl {
public:
    // create an instance of the decoder
    //  blocksize is fixed over the lifetime of this object for performance reasons
    explicit decoder_impl(unsigned blocksize=8192): m_n(blocksize), m_halfN(blocksize/2) {
#ifdef USE_FFTW3
        // create FFTW buffers
        m_lt = (float*)fftwf_malloc(sizeof(float)*m_n);
        m_rt = (float*)fftwf_malloc(sizeof(float)*m_n);
        m_dst = (float*)fftwf_malloc(sizeof(float)*m_n);
        m_dftL = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*m_n);
        m_dftR = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*m_n);
        m_src = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*m_n);
        m_loadL = fftwf_plan_dft_r2c_1d(m_n, m_lt, m_dftL,FFTW_MEASURE);
        m_loadR = fftwf_plan_dft_r2c_1d(m_n, m_rt, m_dftR,FFTW_MEASURE);
        m_store = fftwf_plan_dft_c2r_1d(m_n, m_src, m_dst,FFTW_MEASURE);
#else
        // create lavc fft buffers
        m_lt = (float*)av_malloc(sizeof(FFTSample)*m_n);
        m_rt = (float*)av_malloc(sizeof(FFTSample)*m_n);
        m_dftL = (FFTComplexArray*)av_malloc(sizeof(FFTComplex)*m_n*2);
        m_dftR = (FFTComplexArray*)av_malloc(sizeof(FFTComplex)*m_n*2);
        m_src = (FFTComplexArray*)av_malloc(sizeof(FFTComplex)*m_n*2);
        m_fftContextForward = (FFTContext*)av_malloc(sizeof(FFTContext));
        memset(m_fftContextForward, 0, sizeof(FFTContext));
        m_fftContextReverse = (FFTContext*)av_malloc(sizeof(FFTContext));
        memset(m_fftContextReverse, 0, sizeof(FFTContext));
        ff_fft_init(m_fftContextForward, 13, 0);
        ff_fft_init(m_fftContextReverse, 13, 1);
#endif
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
            m_wnd[k] = sqrt(0.5*(1-std::cos(2*PI*k/m_n))/m_n);
        m_currentBuf = 0;
        memset(m_inbufs, 0, sizeof(m_inbufs));
        memset(m_outbufs, 0, sizeof(m_outbufs));
        // set the default coefficients
        surround_coefficients(0.8165,0.5774);
        phase_mode(0);
        separation(1,1);
        steering_mode(true);
    }

    // destructor
    ~decoder_impl() {
#ifdef USE_FFTW3
        // clean up the FFTW stuff
        fftwf_destroy_plan(m_store);
        fftwf_destroy_plan(m_loadR);
        fftwf_destroy_plan(m_loadL);
        fftwf_free(m_src);
        fftwf_free(m_dftR);
        fftwf_free(m_dftL);
        fftwf_free(m_dst);
        fftwf_free(m_rt);
        fftwf_free(m_lt);
#else
        ff_fft_end(m_fftContextForward);
        ff_fft_end(m_fftContextReverse);
        av_free(m_src);
        av_free(m_dftR);
        av_free(m_dftL);
        av_free(m_rt);
        av_free(m_lt);
        av_free(m_fftContextForward);
        av_free(m_fftContextReverse);
#endif
    }

    float ** getInputBuffers()
    {
        m_inbufs[0] = &m_inbuf[0][m_currentBuf*m_halfN];
        m_inbufs[1] = &m_inbuf[1][m_currentBuf*m_halfN];
        return m_inbufs;
    }

    float ** getOutputBuffers()
    {
        m_outbufs[0] = &m_outbuf[0][m_currentBuf*m_halfN];
        m_outbufs[1] = &m_outbuf[1][m_currentBuf*m_halfN];
        m_outbufs[2] = &m_outbuf[2][m_currentBuf*m_halfN];
        m_outbufs[3] = &m_outbuf[3][m_currentBuf*m_halfN];
        m_outbufs[4] = &m_outbuf[4][m_currentBuf*m_halfN];
        m_outbufs[5] = &m_outbuf[5][m_currentBuf*m_halfN];
        return m_outbufs;
    }

    // decode a chunk of stereo sound, has to contain exactly blocksize samples
    //  center_width [0..1] distributes the center information towards the front left/right channels, 1=full distribution, 0=no distribution
    //  dimension [0..1] moves the soundfield backwards, 0=front, 1=side
    //  adaption_rate [0..1] determines how fast the steering gets adapted, 1=instantaneous, 0.1 = very slow adaption
    void decode(float center_width, float dimension, float adaption_rate) {
        // process first part
        int index = m_currentBuf*m_halfN;
        float *in_second[2] = {&m_inbuf[0][index],&m_inbuf[1][index]};
        m_currentBuf ^= 1;
        index = m_currentBuf*m_halfN;
        float *in_first[2] = {&m_inbuf[0][index],&m_inbuf[1][index]};
        add_output(in_first,in_second,center_width,dimension,adaption_rate,true);
        // shift last half of input buffer to the beginning
    }
    
    // flush the internal buffers
    void flush() {
        for (unsigned k=0;k<m_n;k++) {
            for (unsigned c=0;c<6;c++)
                m_outbuf[c][k] = 0;
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
        cfloat i(0,1), u((a+b)*i), v((b-a)*i), n(0.25,0),o(1,0);
        m_a = (v-o)*n; m_b = (o-u)*n; m_c = (-o-v)*n; m_d = (o+u)*n;
        m_e = (o+v)*n; m_f = (o+u)*n; m_g = (o-v)*n;  m_h = (o-u)*n;
    }

    // set the phase shifting mode
    void phase_mode(unsigned mode) {
        const float modes[4][2] = {{0,0},{0,PI},{PI,0},{-PI/2,PI/2}};
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
    static inline float amplitude(const float cf[2]) { return std::sqrt(cf[0]*cf[0] + cf[1]*cf[1]); }
    static inline float phase(const float cf[2]) { return std::atan2(cf[1],cf[0]); }
    static inline cfloat polar(float a, float p) { return {static_cast<float>(a*std::cos(p)),static_cast<float>(a*std::sin(p))}; }
    static inline float sqr(float x) { return x*x; }
    // the dreaded min/max
    static inline float min(float a, float b) { return a<b?a:b; }
    static inline float max(float a, float b) { return a>b?a:b; }
    static inline float clamp(float x) { return max(-1,min(1,x)); }

    // handle the output buffering for overlapped calls of block_decode
    void add_output(float *input1[2], float *input2[2], float center_width, float dimension, float adaption_rate, bool /*result*/=false) {
        // add the windowed data to the last 1/2 of the output buffer
        float *out[6] = {&m_outbuf[0][0],&m_outbuf[1][0],&m_outbuf[2][0],&m_outbuf[3][0],&m_outbuf[4][0],&m_outbuf[5][0]};
        block_decode(input1,input2,out,center_width,dimension,adaption_rate);
    }

    // CORE FUNCTION: decode a block of data
    void block_decode(float *input1[2], float *input2[2], float *output[6], float center_width, float dimension, float adaption_rate) {
        // 1. scale the input by the window function; this serves a dual purpose:
        // - first it improves the FFT resolution b/c boundary discontinuities (and their frequencies) get removed
        // - second it allows for smooth blending of varying filters between the blocks
        {
            float* pWnd = &m_wnd[0];
            float* pLt =  &m_lt[0];
            float* pRt =  &m_rt[0];
            float* pIn0 = input1[0];
            float* pIn1 = input1[1];
            for (unsigned k=0;k<m_halfN;k++) {
                *pLt++ = *pIn0++ * *pWnd;
                *pRt++ = *pIn1++ * *pWnd++;
            }
            pIn0 = input2[0];
            pIn1 = input2[1];
            for (unsigned k=0;k<m_halfN;k++) {
                *pLt++ = *pIn0++ * *pWnd;
                *pRt++ = *pIn1++ * *pWnd++;
            }
        }

#ifdef USE_FFTW3
        // ... and tranform it into the frequency domain
        fftwf_execute(m_loadL);
        fftwf_execute(m_loadR);
#else
        ff_fft_permuteRC(m_fftContextForward, m_lt, (FFTComplex*)&m_dftL[0]);
        av_fft_calc(m_fftContextForward, (FFTComplex*)&m_dftL[0]);

        ff_fft_permuteRC(m_fftContextForward, m_rt, (FFTComplex*)&m_dftR[0]);
        av_fft_calc(m_fftContextForward, (FFTComplex*)&m_dftR[0]);
#endif

        // 2. compare amplitude and phase of each DFT bin and produce the X/Y coordinates in the sound field
        //    but dont do DC or N/2 component
        for (unsigned f=0;f<m_halfN;f++) {
            // get left/right amplitudes/phases
            float ampL = amplitude(m_dftL[f]), ampR = amplitude(m_dftR[f]);
            float phaseL = phase(m_dftL[f]), phaseR = phase(m_dftR[f]);
//          if (ampL+ampR < epsilon)
//              continue;       

            // calculate the amplitude/phase difference
            float ampDiff = clamp((ampL+ampR < epsilon) ? 0 : (ampR-ampL) / (ampR+ampL));
            float phaseDiff = phaseL - phaseR;
            if (phaseDiff < -PI) phaseDiff += 2*PI;
            if (phaseDiff > PI) phaseDiff -= 2*PI;
            phaseDiff = abs(phaseDiff);

            if (m_linearSteering) {
                // --- this is the fancy new linear mode ---

                // get sound field x/y position
                m_yFs[f] = get_yfs(ampDiff,phaseDiff);
                m_xFs[f] = get_xfs(ampDiff,m_yFs[f]);

                // add dimension control
                m_yFs[f] = clamp(m_yFs[f] - dimension);

                // add crossfeed control
                m_xFs[f] = clamp(m_xFs[f] * (m_frontSeparation*(1+m_yFs[f])/2 + m_rearSeparation*(1-m_yFs[f])/2));

                // 3. generate frequency filters for each output channel
                float left = (1-m_xFs[f])/2, right = (1+m_xFs[f])/2;
                float front = (1+m_yFs[f])/2, back = (1-m_yFs[f])/2;
                float volume[5] = {
                    front * (left * center_width + max(0,-m_xFs[f]) * (1-center_width)),  // left
                    front * center_level*((1-abs(m_xFs[f])) * (1-center_width)),          // center
                    front * (right * center_width + max(0, m_xFs[f]) * (1-center_width)), // right
                    back * m_surroundLevel * left,                                        // left surround
                    back * m_surroundLevel * right                                        // right surround
                };

                // adapt the prior filter
                for (unsigned c=0;c<5;c++)
                    m_filter[c][f] = (1-adaption_rate)*m_filter[c][f] + adaption_rate*volume[c];

            } else {
                // --- this is the old & simple steering mode ---

                // calculate the amplitude/phase difference
                float ampDiff = clamp((ampL+ampR < epsilon) ? 0 : (ampR-ampL) / (ampR+ampL));
                float phaseDiff = phaseL - phaseR;
                if (phaseDiff < -PI) phaseDiff += 2*PI;
                if (phaseDiff > PI) phaseDiff -= 2*PI;
                phaseDiff = abs(phaseDiff);

                // determine sound field x-position
                m_xFs[f] = ampDiff;

                // determine preliminary sound field y-position from phase difference
                m_yFs[f] = 1 - (phaseDiff/PI)*2;

                if (abs(m_xFs[f]) > m_surroundBalance) {
                    // blend linearly between the surrounds and the fronts if the balance exceeds the surround encoding balance
                    // this is necessary because the sound field is trapezoidal and will be stretched behind the listener
                    float frontness = (abs(m_xFs[f]) - m_surroundBalance)/(1-m_surroundBalance);
                    m_yFs[f]  = (1-frontness) * m_yFs[f] + frontness * 1;
                }

                // add dimension control
                m_yFs[f] = clamp(m_yFs[f] - dimension);

                // add crossfeed control
                m_xFs[f] = clamp(m_xFs[f] * (m_frontSeparation*(1+m_yFs[f])/2 + m_rearSeparation*(1-m_yFs[f])/2));

                // 3. generate frequency filters for each output channel, according to the signal position
                // the sum of all channel volumes must be 1.0
                float left = (1-m_xFs[f])/2, right = (1+m_xFs[f])/2;
                float front = (1+m_yFs[f])/2, back = (1-m_yFs[f])/2;
                float volume[5] = {
                    front * (left * center_width + max(0,-m_xFs[f]) * (1-center_width)),      // left
                    front * center_level*((1-abs(m_xFs[f])) * (1-center_width)),              // center
                    front * (right * center_width + max(0, m_xFs[f]) * (1-center_width)),     // right
                    back * m_surroundLevel*max(0,min(1,((1-(m_xFs[f]/m_surroundBalance))/2))),// left surround
                    back * m_surroundLevel*max(0,min(1,((1+(m_xFs[f]/m_surroundBalance))/2))) // right surround
                };

                // adapt the prior filter
                for (unsigned c=0;c<5;c++)
                    m_filter[c][f] = (1-adaption_rate)*m_filter[c][f] + adaption_rate*volume[c];
            }

            // ... and build the signal which we want to position
            m_frontL[f] = polar(ampL+ampR,phaseL);
            m_frontR[f] = polar(ampL+ampR,phaseR);
            m_avg[f] = m_frontL[f] + m_frontR[f];
            m_surL[f] = polar(ampL+ampR,phaseL+m_phaseOffsetL);
            m_surR[f] = polar(ampL+ampR,phaseR+m_phaseOffsetR);
            m_trueavg[f] = cfloat(m_dftL[f][0] + m_dftR[f][0], m_dftL[f][1] + m_dftR[f][1]);
        }

        // 4. distribute the unfiltered reference signals over the channels
        apply_filter(&m_frontL[0], &m_filter[0][0],&output[0][0]);  // front left
        apply_filter(&m_avg[0],    &m_filter[1][0],&output[1][0]);  // front center
        apply_filter(&m_frontR[0], &m_filter[2][0],&output[2][0]);  // front right
        apply_filter(&m_surL[0],   &m_filter[3][0],&output[3][0]);  // surround left
        apply_filter(&m_surR[0],   &m_filter[4][0],&output[4][0]);  // surround right
        apply_filter(&m_trueavg[0],&m_filter[5][0],&output[5][0]);  // lfe
    }

#define FASTER_CALC
    // map from amplitude difference and phase difference to yfs
    static inline double get_yfs(double ampDiff, double phaseDiff) {
        double x = 1-(((1-sqr(ampDiff))*phaseDiff)/PI*2);
#ifdef FASTER_CALC
        double tanX = tan(x);
        return 0.16468622925824683 + 0.5009268347818189*x - 0.06462757726992101*x*x
            + 0.09170680403453149*x*x*x + 0.2617754892323973*tanX - 0.04180413533856156*sqr(tanX);
#else
        return 0.16468622925824683 + 0.5009268347818189*x - 0.06462757726992101*x*x
            + 0.09170680403453149*x*x*x + 0.2617754892323973*tan(x) - 0.04180413533856156*sqr(tan(x));
#endif
    }

    // map from amplitude difference and yfs to xfs
    static inline double get_xfs(double ampDiff, double yfs) {
        double x=ampDiff,y=yfs;
#ifdef FASTER_CALC
        double tanX = tan(x);
        double tanY = tan(y);
        double asinX = asin(x);
        double sinX = sin(x);
        double sinY = sin(y);
        double x3 = x*x*x;
        double y2 = y*y;
        double y3 = y*y2;
        return 2.464833559224702*x - 423.52131153259404*x*y + 
            67.8557858606918*x3*y + 788.2429425544392*x*y2 - 
            79.97650354902909*x3*y2 - 513.8966153850349*x*y3 + 
            35.68117670186306*x3*y3 + 13867.406173420834*y*asinX - 
            2075.8237075786396*y2*asinX - 908.2722068360281*y3*asinX - 
            12934.654772878019*asinX*sinY - 13216.736529661162*y*tanX + 
            1288.6463247741938*y2*tanX + 1384.372969378453*y3*tanX + 
            12699.231471126128*sinY*tanX + 95.37131275594336*sinX*tanY - 
            91.21223198407546*tanX*tanY;
#else
        return 2.464833559224702*x - 423.52131153259404*x*y + 
            67.8557858606918*x*x*x*y + 788.2429425544392*x*y*y - 
            79.97650354902909*x*x*x*y*y - 513.8966153850349*x*y*y*y + 
            35.68117670186306*x*x*x*y*y*y + 13867.406173420834*y*asin(x) - 
            2075.8237075786396*y*y*asin(x) - 908.2722068360281*y*y*y*asin(x) - 
            12934.654772878019*asin(x)*sin(y) - 13216.736529661162*y*tan(x) + 
            1288.6463247741938*y*y*tan(x) + 1384.372969378453*y*y*y*tan(x) + 
            12699.231471126128*sin(y)*tan(x) + 95.37131275594336*sin(x)*tan(y) - 
            91.21223198407546*tan(x)*tan(y);
#endif
    }

    // filter the complex source signal and add it to target
    void apply_filter(cfloat *signal, const float *flt, float *target) {
        // filter the signal
        for (unsigned f=0;f<=m_halfN;f++) {
            m_src[f][0] = signal[f].real() * flt[f];
            m_src[f][1] = signal[f].imag() * flt[f];
        }
#ifdef USE_FFTW3
        // transform into time domain
        fftwf_execute(m_store);

        float* pT1   = &target[m_currentBuf*m_halfN];
        float* pWnd1 = &m_wnd[0];
        float* pDst1 = &m_dst[0];
        float* pT2   = &target[(m_currentBuf^1)*m_halfN];
        float* pWnd2 = &m_wnd[m_halfN];
        float* pDst2 = &m_dst[m_halfN];
        // add the result to target, windowed
        for (unsigned int k=0;k<m_halfN;k++)
        {
            // 1st part is overlap add
            *pT1++ += *pWnd1++ * *pDst1++;
            // 2nd part is set as has no history
            *pT2++  = *pWnd2++ * *pDst2++;
        }
#else
        // enforce odd symmetry
        for (unsigned f=1;f<m_halfN;f++) {
            m_src[m_n-f][0] = m_src[f][0];
            m_src[m_n-f][1] = -m_src[f][1];   // complex conjugate
        }
        av_fft_permute(m_fftContextReverse, (FFTComplex*)&m_src[0]);
        av_fft_calc(m_fftContextReverse, (FFTComplex*)&m_src[0]);

        float* pT1   = &target[m_currentBuf*m_halfN];
        float* pWnd1 = &m_wnd[0];
        float* pDst1 = &m_src[0][0];
        float* pT2   = &target[(m_currentBuf^1)*m_halfN];
        float* pWnd2 = &m_wnd[m_halfN];
        float* pDst2 = &m_src[m_halfN][0];
        // add the result to target, windowed
        for (unsigned int k=0;k<m_halfN;k++)
        {
            // 1st part is overlap add
            *pT1++ += *pWnd1++ * *pDst1; pDst1 += 2;
            // 2nd part is set as has no history
            *pT2++  = *pWnd2++ * *pDst2; pDst2 += 2;
        }
#endif
    }

#ifndef USE_FFTW3
    /**
     *  * Do the permutation needed BEFORE calling ff_fft_calc()
     *  special for freesurround that also copies
     *   */
    void ff_fft_permuteRC(FFTContext *s, FFTSample *r, FFTComplex *z)
    {
        int j, k, np;
        const uint16_t *revtab = s->revtab;

        /* reverse */
        np = 1 << s->nbits;
        for(j=0;j<np;j++) {
            k = revtab[j];
            z[k].re = r[j];
            z[k].im = 0.0;
        }
    }

    /**
     *  * Do the permutation needed BEFORE calling ff_fft_calc()
     *  special for freesurround that also copies and 
     *  discards im component as it should be 0
     *   */
    void ff_fft_permuteCR(FFTContext *s, FFTComplex *z, FFTSample *r)
    {
        int j, k, np;
        const uint16_t *revtab = s->revtab;

        /* reverse */
        np = 1 << s->nbits;
        for(j=0;j<np;j++) {
            k = revtab[j];
            if (k < j) {
                r[k] = z[j].re;
                r[j] = z[k].re;
            }
        }
    }
#endif

    unsigned int m_n;                    // the block size
    unsigned int m_halfN;                // half block size precalculated
#ifdef USE_FFTW3
    // FFTW data structures
    float *m_lt,*m_rt,*m_dst;              // left total, right total (source arrays), destination array
    fftwf_complex *m_dftL,*m_dftR,*m_src;  // intermediate arrays (FFTs of lt & rt, processing source)
    fftwf_plan m_loadL,m_loadR,m_store;    // plans for loading the data into the intermediate format and back
#else
    FFTContext *m_fftContextForward, *m_fftContextReverse;
    FFTSample *m_lt,*m_rt;                 // left total, right total (source arrays), destination array
    FFTComplexArray *m_dftL,*m_dftR,*m_src;// intermediate arrays (FFTs of lt & rt, processing source)
#endif
    // buffers
    std::vector<cfloat> m_frontL,m_frontR,m_avg,m_surL,m_surR; // the signal (phase-corrected) in the frequency domain
    std::vector<cfloat> m_trueavg;       // for lfe generation
    std::vector<float> m_xFs,m_yFs;      // the feature space positions for each frequency bin
    std::vector<float> m_wnd;            // the window function, precalculated
    std::vector<float> m_filter[6];      // a frequency filter for each output channel
    std::vector<float> m_inbuf[2];       // the sliding input buffers
    std::vector<float> m_outbuf[6];      // the sliding output buffers
    // coefficients
    float m_surroundHigh,m_surroundLow;  // high and low surround mixing coefficient (e.g. 0.8165/0.5774)
    float m_surroundBalance;             // the xfs balance that follows from the coeffs
    float m_surroundLevel;               // gain for the surround channels (follows from the coeffs
    float m_phaseOffsetL, m_phaseOffsetR;// phase shifts to be applied to the rear channels
    float m_frontSeparation;             // front stereo separation
    float m_rearSeparation;              // rear stereo separation
    bool m_linearSteering;               // whether the steering should be linear or not
    cfloat m_a,m_b,m_c,m_d,m_e,m_f,m_g,m_h; // coefficients for the linear steering
    int m_currentBuf;                    // specifies which buffer is 2nd half of input sliding buffer
    float * m_inbufs[2];                 // for passing back to driver
    float * m_outbufs[6];                // for passing back to driver

    friend class fsurround_decoder;
};


// implementation of the shell class

fsurround_decoder::fsurround_decoder(unsigned blocksize): impl(new decoder_impl(blocksize)) { }

fsurround_decoder::~fsurround_decoder() { delete impl; }

void fsurround_decoder::decode(float center_width, float dimension, float adaption_rate) {
    impl->decode(center_width,dimension,adaption_rate);
}

void fsurround_decoder::flush() { impl->flush(); }

void fsurround_decoder::surround_coefficients(float a, float b) { impl->surround_coefficients(a,b); }

void fsurround_decoder::phase_mode(unsigned mode) { impl->phase_mode(mode); }

void fsurround_decoder::steering_mode(bool mode) { impl->steering_mode(mode); }

void fsurround_decoder::separation(float front, float rear) { impl->separation(front,rear); }

float ** fsurround_decoder::getInputBuffers()
{
    return impl->getInputBuffers();
}

float ** fsurround_decoder::getOutputBuffers()
{
    return impl->getOutputBuffers();
}

void fsurround_decoder::sample_rate(unsigned int samplerate)
{
    impl->sample_rate(samplerate);
}
