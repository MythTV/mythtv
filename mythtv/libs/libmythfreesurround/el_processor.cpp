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
#include <cstdlib>
#include <cstring>
#include <complex>
#include <cmath>
#include <vector>
#ifdef USE_FFTW3
#include "fftw3.h"
#else
extern "C" {
#include "dsputil.h"
};
typedef FFTSample FFTComplexArray[2];
#endif


#ifdef USE_FFTW3
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
    decoder_impl(unsigned blocksize=8192): N(blocksize), halfN(blocksize/2) {
#ifdef USE_FFTW3
        // create FFTW buffers
        lt = (float*)fftwf_malloc(sizeof(float)*N);
        rt = (float*)fftwf_malloc(sizeof(float)*N);
        dst = (float*)fftwf_malloc(sizeof(float)*N);
        dftL = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*N);
        dftR = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*N);
        src = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*N);
        loadL = fftwf_plan_dft_r2c_1d(N, lt, dftL,FFTW_MEASURE);
        loadR = fftwf_plan_dft_r2c_1d(N, rt, dftR,FFTW_MEASURE);
        store = fftwf_plan_dft_c2r_1d(N, src, dst,FFTW_MEASURE);    
#else
        // create lavc fft buffers
        lt = (float*)av_malloc(sizeof(FFTSample)*N);
        rt = (float*)av_malloc(sizeof(FFTSample)*N);
        dftL = (FFTComplexArray*)av_malloc(sizeof(FFTComplex)*N*2);
        dftR = (FFTComplexArray*)av_malloc(sizeof(FFTComplex)*N*2);
        src = (FFTComplexArray*)av_malloc(sizeof(FFTComplex)*N*2);
        fftContextForward = (FFTContext*)av_malloc(sizeof(FFTContext));
        memset(fftContextForward, 0, sizeof(FFTContext));
        fftContextReverse = (FFTContext*)av_malloc(sizeof(FFTContext));
        memset(fftContextReverse, 0, sizeof(FFTContext));
        ff_fft_init(fftContextForward, 13, 0);
        ff_fft_init(fftContextReverse, 13, 1);
#endif
        // resize our own buffers
        frontR.resize(N);
        frontL.resize(N);
        avg.resize(N);
        surR.resize(N);
        surL.resize(N);
        trueavg.resize(N);
        xfs.resize(N);
        yfs.resize(N);
        inbuf[0].resize(N);
        inbuf[1].resize(N);
        for (unsigned c=0;c<6;c++) {
            outbuf[c].resize(N);
            filter[c].resize(N);
        }
        sample_rate(48000);
        // generate the window function (square root of hann, b/c it is applied before and after the transform)
        wnd.resize(N);
        for (unsigned k=0;k<N;k++)
            wnd[k] = sqrt(0.5*(1-cos(2*PI*k/N))/N);
        current_buf = 0;
        memset(inbufs, 0, sizeof(inbufs));
        memset(outbufs, 0, sizeof(outbufs));
        // set the default coefficients
        surround_coefficients(0.8165,0.5774);
        phase_mode(0);
        separation(1,1);
        steering_mode(1);
    }

    // destructor
    ~decoder_impl() {
#ifdef USE_FFTW3
        // clean up the FFTW stuff
        fftwf_destroy_plan(store);
        fftwf_destroy_plan(loadR);
        fftwf_destroy_plan(loadL);
        fftwf_free(src); 
        fftwf_free(dftR);
        fftwf_free(dftL);
        fftwf_free(dst);
        fftwf_free(rt);
        fftwf_free(lt);
#else
        ff_fft_end(fftContextForward);
        ff_fft_end(fftContextReverse);
        av_free(src); 
        av_free(dftR);
        av_free(dftL);
        av_free(rt);
        av_free(lt);
        av_free(fftContextForward); 
        av_free(fftContextReverse); 
#endif
    }

    float ** getInputBuffers()
    {
        inbufs[0] = &inbuf[0][current_buf*halfN];
        inbufs[1] = &inbuf[1][current_buf*halfN];
        return inbufs;
    }

    float ** getOutputBuffers()
    {
        outbufs[0] = &outbuf[0][current_buf*halfN];
        outbufs[1] = &outbuf[1][current_buf*halfN];
        outbufs[2] = &outbuf[2][current_buf*halfN];
        outbufs[3] = &outbuf[3][current_buf*halfN];
        outbufs[4] = &outbuf[4][current_buf*halfN];
        outbufs[5] = &outbuf[5][current_buf*halfN];
        return outbufs;
    }

    // decode a chunk of stereo sound, has to contain exactly blocksize samples
    //  center_width [0..1] distributes the center information towards the front left/right channels, 1=full distribution, 0=no distribution
    //  dimension [0..1] moves the soundfield backwards, 0=front, 1=side
    //  adaption_rate [0..1] determines how fast the steering gets adapted, 1=instantaneous, 0.1 = very slow adaption
    void decode(float center_width, float dimension, float adaption_rate) {
        // process first part
        int index;
        index = current_buf*halfN;
        float *in_second[2] = {&inbuf[0][index],&inbuf[1][index]};
        current_buf ^= 1;
        index = current_buf*halfN;
        float *in_first[2] = {&inbuf[0][index],&inbuf[1][index]};
        add_output(in_first,in_second,center_width,dimension,adaption_rate,true);
        // shift last half of input buffer to the beginning
    }
    
    // flush the internal buffers
    void flush() {
        for (unsigned k=0;k<N;k++) {
            for (unsigned c=0;c<6;c++)
                outbuf[c][k] = 0;
            inbuf[0][k] = 0;
            inbuf[1][k] = 0;
        }
    }

    // set lfe filter params
    void sample_rate(unsigned int srate) {
        // lfe filter is just straight through band limited
        unsigned int cutoff = (30*N)/srate;
        for (unsigned f=0;f<=halfN;f++) {           
            if (f<cutoff)
                filter[5][f] = 0.5*sqrt(0.5);
            else
                filter[5][f] = 0.0;
        }
    }

    // set the assumed surround mixing coefficients
    void surround_coefficients(float a, float b) {
        // calc the simple coefficients
        surround_high = a;
        surround_low = b;
        surround_balance = (a-b)/(a+b);
        surround_level = 1/(a+b);
        // calc the linear coefficients
        cfloat i(0,1), u((a+b)*i), v((b-a)*i), n(0.25,0),o(1,0);
        A = (v-o)*n; B = (o-u)*n; C = (-o-v)*n; D = (o+u)*n;
        E = (o+v)*n; F = (o+u)*n; G = (o-v)*n;  H = (o-u)*n;
    }

    // set the phase shifting mode
    void phase_mode(unsigned mode) {
        const float modes[4][2] = {{0,0},{0,PI},{PI,0},{-PI/2,PI/2}};
        phase_offsetL = modes[mode][0];
        phase_offsetR = modes[mode][1];
    }

    // what steering mode should be chosen
    void steering_mode(bool mode) { linear_steering = mode; }

    // set front & rear separation controls
    void separation(float front, float rear) {
        front_separation = front;
        rear_separation = rear;
    }

private:
    // polar <-> cartesian coodinates conversion
    static inline float amplitude(const float cf[2]) { return sqrt(cf[0]*cf[0] + cf[1]*cf[1]); }
    static inline float phase(const float cf[2]) { return atan2(cf[1],cf[0]); }
    static inline cfloat polar(float a, float p) { return cfloat(a*cos(p),a*sin(p)); }
    static inline float sqr(float x) { return x*x; }
    // the dreaded min/max
    static inline float min(float a, float b) { return a<b?a:b; }
    static inline float max(float a, float b) { return a>b?a:b; }
    static inline float clamp(float x) { return max(-1,min(1,x)); }

    // handle the output buffering for overlapped calls of block_decode
    void add_output(float *input1[2], float *input2[2], float center_width, float dimension, float adaption_rate, bool result=false) {
        // add the windowed data to the last 1/2 of the output buffer
        float *out[6] = {&outbuf[0][0],&outbuf[1][0],&outbuf[2][0],&outbuf[3][0],&outbuf[4][0],&outbuf[5][0]};
        block_decode(input1,input2,out,center_width,dimension,adaption_rate);
    }

    // CORE FUNCTION: decode a block of data
    void block_decode(float *input1[2], float *input2[2], float *output[6], float center_width, float dimension, float adaption_rate) {
        // 1. scale the input by the window function; this serves a dual purpose:
        // - first it improves the FFT resolution b/c boundary discontinuities (and their frequencies) get removed
        // - second it allows for smooth blending of varying filters between the blocks
        {
            float* pWnd = &wnd[0];
            float* pLt = &lt[0];
            float* pRt = &rt[0];
            float* pIn0 = input1[0];
            float* pIn1 = input1[1];
            for (unsigned k=0;k<halfN;k++) {
                *pLt++ = *pIn0++ * *pWnd;
                *pRt++ = *pIn1++ * *pWnd++;
            }
            pIn0 = input2[0];
            pIn1 = input2[1];
            for (unsigned k=0;k<halfN;k++) {
                *pLt++ = *pIn0++ * *pWnd;
                *pRt++ = *pIn1++ * *pWnd++;
            }
        }

#ifdef USE_FFTW3
        // ... and tranform it into the frequency domain
        fftwf_execute(loadL);
        fftwf_execute(loadR);
#else
        ff_fft_permuteRC(fftContextForward, lt, (FFTComplex*)&dftL[0]);
        ff_fft_permuteRC(fftContextForward, rt, (FFTComplex*)&dftR[0]);
        ff_fft_calc(fftContextForward, (FFTComplex*)&dftL[0]);
        ff_fft_calc(fftContextForward, (FFTComplex*)&dftR[0]);
#endif

        // 2. compare amplitude and phase of each DFT bin and produce the X/Y coordinates in the sound field
        //    but dont do DC or N/2 component
        for (unsigned f=0;f<halfN;f++) {           
            // get left/right amplitudes/phases
            float ampL = amplitude(dftL[f]), ampR = amplitude(dftR[f]);
            float phaseL = phase(dftL[f]), phaseR = phase(dftR[f]);
//          if (ampL+ampR < epsilon)
//              continue;       

            // calculate the amplitude/phase difference
            float ampDiff = clamp((ampL+ampR < epsilon) ? 0 : (ampR-ampL) / (ampR+ampL));
            float phaseDiff = phaseL - phaseR;
            if (phaseDiff < -PI) phaseDiff += 2*PI;
            if (phaseDiff > PI) phaseDiff -= 2*PI;
            phaseDiff = abs(phaseDiff);

            if (linear_steering) {
                // --- this is the fancy new linear mode ---

                // get sound field x/y position
                yfs[f] = get_yfs(ampDiff,phaseDiff);
                xfs[f] = get_xfs(ampDiff,yfs[f]);

                // add dimension control
                yfs[f] = clamp(yfs[f] - dimension);

                // add crossfeed control
                xfs[f] = clamp(xfs[f] * (front_separation*(1+yfs[f])/2 + rear_separation*(1-yfs[f])/2));

                // 3. generate frequency filters for each output channel
                float left = (1-xfs[f])/2, right = (1+xfs[f])/2;
                float front = (1+yfs[f])/2, back = (1-yfs[f])/2;
                float volume[5] = {
                    front * (left * center_width + max(0,-xfs[f]) * (1-center_width)),  // left
                    front * center_level*((1-abs(xfs[f])) * (1-center_width)),          // center
                    front * (right * center_width + max(0, xfs[f]) * (1-center_width)), // right
                    back * surround_level * left,                                       // left surround
                    back * surround_level * right                                       // right surround
                };

                // adapt the prior filter
                for (unsigned c=0;c<5;c++)
                    filter[c][f] = (1-adaption_rate)*filter[c][f] + adaption_rate*volume[c];

            } else {
                // --- this is the old & simple steering mode ---

                // calculate the amplitude/phase difference
                float ampDiff = clamp((ampL+ampR < epsilon) ? 0 : (ampR-ampL) / (ampR+ampL));
                float phaseDiff = phaseL - phaseR;
                if (phaseDiff < -PI) phaseDiff += 2*PI;
                if (phaseDiff > PI) phaseDiff -= 2*PI;
                phaseDiff = abs(phaseDiff);

                // determine sound field x-position
                xfs[f] = ampDiff;

                // determine preliminary sound field y-position from phase difference
                yfs[f] = 1 - (phaseDiff/PI)*2;

                if (abs(xfs[f]) > surround_balance) {
                    // blend linearly between the surrounds and the fronts if the balance exceeds the surround encoding balance
                    // this is necessary because the sound field is trapezoidal and will be stretched behind the listener
                    float frontness = (abs(xfs[f]) - surround_balance)/(1-surround_balance);
                    yfs[f]  = (1-frontness) * yfs[f] + frontness * 1; 
                }

                // add dimension control
                yfs[f] = clamp(yfs[f] - dimension);

                // add crossfeed control
                xfs[f] = clamp(xfs[f] * (front_separation*(1+yfs[f])/2 + rear_separation*(1-yfs[f])/2));

                // 3. generate frequency filters for each output channel, according to the signal position
                // the sum of all channel volumes must be 1.0
                float left = (1-xfs[f])/2, right = (1+xfs[f])/2;
                float front = (1+yfs[f])/2, back = (1-yfs[f])/2;
                float volume[5] = {
                    front * (left * center_width + max(0,-xfs[f]) * (1-center_width)),      // left
                    front * center_level*((1-abs(xfs[f])) * (1-center_width)),              // center
                    front * (right * center_width + max(0, xfs[f]) * (1-center_width)),     // right
                    back * surround_level*max(0,min(1,((1-(xfs[f]/surround_balance))/2))),  // left surround
                    back * surround_level*max(0,min(1,((1+(xfs[f]/surround_balance))/2)))   // right surround
                };

                // adapt the prior filter
                for (unsigned c=0;c<5;c++)
                    filter[c][f] = (1-adaption_rate)*filter[c][f] + adaption_rate*volume[c];
            }

            // ... and build the signal which we want to position
            frontL[f] = polar(ampL+ampR,phaseL);
            frontR[f] = polar(ampL+ampR,phaseR);
            avg[f] = frontL[f] + frontR[f];
            surL[f] = polar(ampL+ampR,phaseL+phase_offsetL);
            surR[f] = polar(ampL+ampR,phaseR+phase_offsetR);
            trueavg[f] = cfloat(dftL[f][0] + dftR[f][0], dftL[f][1] + dftR[f][1]);
        }

        // 4. distribute the unfiltered reference signals over the channels
        apply_filter(&frontL[0],&filter[0][0],&output[0][0]);   // front left
        apply_filter(&avg[0], &filter[1][0],&output[1][0]);     // front center
        apply_filter(&frontR[0],&filter[2][0],&output[2][0]);   // front right
        apply_filter(&surL[0],&filter[3][0],&output[3][0]);     // surround left
        apply_filter(&surR[0],&filter[4][0],&output[4][0]);     // surround right
        apply_filter(&trueavg[0],&filter[5][0],&output[5][0]);  // lfe
    }

#define FASTER_CALC
    // map from amplitude difference and phase difference to yfs
    inline double get_yfs(double ampDiff, double phaseDiff) {
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
    inline double get_xfs(double ampDiff, double yfs) {
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
    void apply_filter(cfloat *signal, float *flt, float *target) {
        // filter the signal
        unsigned f;
        for (f=0;f<=halfN;f++) {
            src[f][0] = signal[f].real() * flt[f];
            src[f][1] = signal[f].imag() * flt[f];
        }
#ifdef USE_FFTW3
        // transform into time domain
        fftwf_execute(store);

        float* pT1   = &target[current_buf*halfN];
        float* pWnd1 = &wnd[0];
        float* pDst1 = &dst[0];
        float* pT2   = &target[(current_buf^1)*halfN];
        float* pWnd2 = &wnd[halfN];
        float* pDst2 = &dst[halfN];
        // add the result to target, windowed
        for (unsigned int k=0;k<halfN;k++)
        {
            // 1st part is overlap add
            *pT1++ += *pWnd1++ * *pDst1++;
            // 2nd part is set as has no history
            *pT2++  = *pWnd2++ * *pDst2++;
        }
#else
        // enforce odd symmetry
        for (f=1;f<halfN;f++) {
            src[N-f][0] = src[f][0];
            src[N-f][1] = -src[f][1];   // complex conjugate
        }
        ff_fft_permute(fftContextReverse, (FFTComplex*)&src[0]);
        ff_fft_calc(fftContextReverse, (FFTComplex*)&src[0]);

        float* pT1   = &target[current_buf*halfN];
        float* pWnd1 = &wnd[0];
        float* pDst1 = &src[0][0];
        float* pT2   = &target[(current_buf^1)*halfN];
        float* pWnd2 = &wnd[halfN];
        float* pDst2 = &src[halfN][0];
        // add the result to target, windowed
        for (unsigned int k=0;k<halfN;k++)
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
        FFTComplex tmp;
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

    unsigned int N;                    // the block size
    unsigned int halfN;                // half block size precalculated
#ifdef USE_FFTW3
    // FFTW data structures
    float *lt,*rt,*dst;                // left total, right total (source arrays), destination array
    fftwf_complex *dftL,*dftR,*src;    // intermediate arrays (FFTs of lt & rt, processing source)
    fftwf_plan loadL,loadR,store;      // plans for loading the data into the intermediate format and back
#else
    FFTContext *fftContextForward, *fftContextReverse; 
    FFTSample *lt,*rt;                 // left total, right total (source arrays), destination array
    FFTComplexArray *dftL,*dftR,*src;  // intermediate arrays (FFTs of lt & rt, processing source)
#endif
    // buffers
    std::vector<cfloat> frontL,frontR,avg,surL,surR; // the signal (phase-corrected) in the frequency domain
    std::vector<cfloat> trueavg;       // for lfe generation
    std::vector<float> xfs,yfs;        // the feature space positions for each frequency bin
    std::vector<float> wnd;            // the window function, precalculated
    std::vector<float> filter[6];      // a frequency filter for each output channel
    std::vector<float> inbuf[2];       // the sliding input buffers
    std::vector<float> outbuf[6];      // the sliding output buffers
    // coefficients
    float surround_high,surround_low;  // high and low surround mixing coefficient (e.g. 0.8165/0.5774)
    float surround_balance;            // the xfs balance that follows from the coeffs
    float surround_level;              // gain for the surround channels (follows from the coeffs
    float phase_offsetL, phase_offsetR;// phase shifts to be applied to the rear channels
    float front_separation;            // front stereo separation
    float rear_separation;             // rear stereo separation
    bool linear_steering;              // whether the steering should be linear or not
    cfloat A,B,C,D,E,F,G,H;            // coefficients for the linear steering
    int current_buf;                   // specifies which buffer is 2nd half of input sliding buffer
    float * inbufs[2];                 // for passing back to driver
    float * outbufs[6];                // for passing back to driver

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
