////////////////////////////////////////////////////////////////////////////////
///
/// Sampled sound tempo changer/time stretch algorithm. Changes the sound tempo 
/// while maintaining the original pitch by using a time domain WSOLA-like method 
/// with several performance-increasing tweaks.
///
/// Anti-alias filter is used to prevent folding of high frequencies when 
/// transposing the sample rate with interpolation.
///
/// Author        : Copyright (c) Olli Parviainen
/// Author e-mail : oparviai 'at' iki.fi
/// SoundTouch WWW: http://www.surina.net/soundtouch
///
////////////////////////////////////////////////////////////////////////////////
//
// Last changed  : $Date: 2008-02-10 18:07:12 +0200 (su, 10 helmi 2008) $
// File revision : $Revision: 4 $
//
// $Id: AAFilter.h 4 2008-02-10 16:07:12Z oparviai $
//
////////////////////////////////////////////////////////////////////////////////
//
// License :
//
//  SoundTouch audio processing library
//  Copyright (c) Olli Parviainen
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#ifndef AAFilter_H
#define AAFilter_H

#include "synthwii/soundtouch/include/STTypes.h"
#include "utl/MemMgr.h"

namespace soundtouch
{

class AAFilter
{
protected:
    class FIRFilter *pFIR;

    /// Low-pass filter cut-off frequency, negative = invalid
    double cutoffFreq;

    /// num of filter taps
    uint length;

    /// Calculate the FIR coefficients realizing the given cutoff-frequency
    void calculateCoeffs();
public:
    void* operator new(size_t s) { return _MemAlloc(s, 0x20); }
    AAFilter(uint length);

    ~AAFilter();

    /// Sets new anti-alias filter cut-off edge frequency, scaled to sampling 
    /// frequency (nyquist frequency = 0.5). The filter will cut off the 
    /// frequencies than that.
    void setCutoffFreq(double newCutoffFreq);

    /// Sets number of FIR filter taps, i.e. ~filter complexity
    void setLength(uint newLength);

    uint getLength() const;

    /// Applies the filter to the given sequence of samples. 
    /// Note : The amount of outputted samples is by value of 'filter length' 
    /// smaller than the amount of input samples.
    uint evaluate(SAMPLETYPE *dest, 
                  const SAMPLETYPE *src, 
                  uint numSamples, 
                  uint numChannels) const;
};

}

#endif
