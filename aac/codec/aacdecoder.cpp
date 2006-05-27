/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: RCSL 1.0/RPSL 1.0 
 *  
 * Portions Copyright (c) 1995-2002 RealNetworks, Inc. All Rights Reserved. 
 *      
 * The contents of this file, and the files included with this file, are 
 * subject to the current version of the RealNetworks Public Source License 
 * Version 1.0 (the "RPSL") available at 
 * http://www.helixcommunity.org/content/rpsl unless you have licensed 
 * the file under the RealNetworks Community Source License Version 1.0 
 * (the "RCSL") available at http://www.helixcommunity.org/content/rcsl, 
 * in which case the RCSL will apply. You may also obtain the license terms 
 * directly from RealNetworks.  You may not use this file except in 
 * compliance with the RPSL or, if you have a valid RCSL with RealNetworks 
 * applicable to this file, the RCSL.  Please see the applicable RPSL or 
 * RCSL for the rights, obligations and limitations governing use of the 
 * contents of the file.  
 *  
 * This file is part of the Helix DNA Technology. RealNetworks is the 
 * developer of the Original Code and owns the copyrights in the portions 
 * it created. 
 *  
 * This file, and the files included with this file, is distributed and made 
 * available on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND REALNETWORKS HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS 
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * 
 * Technology Compatibility Kit Test Suite(s) Location: 
 *    http://www.helixcommunity.org/content/tck 
 * 
 * Contributor(s): 
 *  
 * ***** END LICENSE BLOCK ***** */ 


#include <math.h>

#include "hxtypes.h"

#include "hxresult.h"

#include "faad.h"
#include "aacdecoder.h"
#include "aacconstants.h"

#include "hxheap.h"
#ifdef _DEBUG
#undef HX_THIS_FILE
static char HX_THIS_FILE[] = __FILE__;
#endif

/****************************************************************************
 *  IUnknown methods
 */
/////////////////////////////////////////////////////////////////////////
//  Method:
//	IUnknown::QueryInterface
//
STDMETHODIMP CAACDecoder::QueryInterface(REFIID riid, void** ppvObj)
{
    if (IsEqualIID(riid, IID_IHXAudioDecoder))
    {
        AddRef();
        *ppvObj = (IHXAudioDecoder*) this;
        return HXR_OK;
    }
    else if (IsEqualIID(riid, IID_IUnknown))
    {
        AddRef();
        *ppvObj = this;
        return HXR_OK;
    }
    
    *ppvObj = NULL;
    
    return HXR_NOINTERFACE;
}

/////////////////////////////////////////////////////////////////////////
//  Method:
//	IUnknown::AddRef
//
STDMETHODIMP_(ULONG32) CAACDecoder::AddRef()
{
    return InterlockedIncrement(&m_lRefCount);
}

/////////////////////////////////////////////////////////////////////////
//  Method:
//	IUnknown::Release
//
STDMETHODIMP_(ULONG32) CAACDecoder::Release()
{
    if (InterlockedDecrement(&m_lRefCount) > 0)
    {
        return m_lRefCount;
    }

    delete this;
    return 0;
}

CAACDecoder::CAACDecoder()
: mhDecoder(0)
, mSamplesInFrame(0)
, m_lRefCount(0)
, mpLastframe(0)
, mpDecoderSpecificInfo(0)
{}

CAACDecoder::~CAACDecoder()
{
    if (mhDecoder)
    {
        faacDecClose(mhDecoder) ;
        mhDecoder = 0 ;
    }

    HX_VECTOR_DELETE(mpLastframe) ;
}

STDMETHODIMP CAACDecoder::Reset()
{
    HX_RESULT res = HXR_OK ;

    if (mhDecoder)
    {
        faacDecClose(mhDecoder) ;
        mhDecoder = 0 ;
    }

	mNumChannels = mConfig.GetNChannels() ;
	mSampleRate  = mConfig.GetSampleRate() ;
	mSamplesInFrame = mNumChannels * mConfig.GetFrameLength() ;

	mhDecoder = faacDecOpen() ;
	if (!mhDecoder)
		res = HXR_OUTOFMEMORY ;

	if (SUCCEEDED(res))
	{
        unsigned long sr;
        unsigned char nc;
		faacDecInit2(mhDecoder, mpDecoderSpecificInfo, mDecoderSpecificInfoSize, &sr, &nc) ;

        if (sr != (unsigned)mSampleRate || nc != (unsigned)mNumChannels)
        {
			res = HXR_FAIL ;
        }
    }

	if (SUCCEEDED(res))
	{
		faacDecConfigurationPtr p = faacDecGetCurrentConfiguration(mhDecoder) ;
		if (p)
		{
			p->defSampleRate = mConfig.GetSampleRate() ;
			p->defObjectType = (unsigned char)mConfig.GetObjectType() ;
            p->outputFormat  = FAAD_FMT_16BIT ;
			faacDecSetConfiguration(mhDecoder, p) ;
		}
		else
			res = HXR_FAIL ;
	}

    if (SUCCEEDED(res))
    {
        // AAC frames have a maximum size of 6144 bits/channel

        HX_VECTOR_DELETE(mpLastframe) ;

        mpLastframe = new UCHAR[mNumChannels * eAACMaxFrameBitsPerChannel / 8] ;
        if (!mpLastframe)
            res = HXR_OUTOFMEMORY ;
    }

    mnBytesLastframe = 0 ;
    mConcealSamples = 0 ;
	mDecoderInit = 0 ;

	return res ;
}

STDMETHODIMP CAACDecoder::OpenDecoder(UINT32 configType,
				   const void* pConfig,
				   UINT32 nBytes)
{
	HX_RESULT res = HXR_OK ;
	struct BITSTREAM *pBs = 0 ;

	if (configType != eAACConfigAudioSpecificCfg)
		res = HXR_INVALID_PARAMETER ;

    if (SUCCEEDED(res))
    {
        if (mpDecoderSpecificInfo)
        {
            delete[] mpDecoderSpecificInfo ; mpDecoderSpecificInfo = 0 ;
        }
        mDecoderSpecificInfoSize = nBytes ;
        mpDecoderSpecificInfo = new unsigned char[nBytes] ;
        if (!mpDecoderSpecificInfo)
        {
            res = HXR_OUTOFMEMORY ;
        }
    }

    if (SUCCEEDED(res))
    {
        memcpy(mpDecoderSpecificInfo, pConfig, nBytes) ; /* Flawfinder: ignore */
    }

    if (SUCCEEDED(res))
	{
		if (newBitstream(&pBs, 8*nBytes))
		{
			res = HXR_OUTOFMEMORY ;
		}
	}

	if (SUCCEEDED(res))
	{
		feedBitstream(pBs, (unsigned char*)pConfig, 8*nBytes) ;
		setAtBitstream(pBs,0,1);

		res = mConfig.Read(*pBs) ;
	}

	// we are done with reading the bitstream, delete it
	if (pBs)
	{
		deleteBitstream(pBs) ;
	}

    // Reset will re-initialize the codec with parameters from mConfig and
    // mpDecoderSpecificInfo
    if (SUCCEEDED(res))
        res = Reset() ;

	return res ;
}

STDMETHODIMP CAACDecoder::Conceal(UINT32 nSamples)
{
    // round the number of samples to conceal to the nearest number of integral frames.
    mConcealSamples = mSamplesInFrame * ((nSamples + mSamplesInFrame/2) / mSamplesInFrame) ;
    return HXR_OK ;
}

STDMETHODIMP CAACDecoder::Decode(const UCHAR* data, UINT32 nBytes, UINT32 &nBytesConsumed, INT16 *samplesOut, UINT32& nSamplesOut, HXBOOL eof)
{
    UINT32 maxSamplesOut = nSamplesOut ;

    nSamplesOut = 0 ;
    nBytesConsumed = 0 ;
	
    VerifyInput((UCHAR*)data, nBytes, nBytes);

    /* fake concealment */
    while (mConcealSamples && nSamplesOut + mSamplesInFrame <= maxSamplesOut)
    {
        {
            memset(samplesOut + nSamplesOut, 0, sizeof(*samplesOut)*mSamplesInFrame) ;
        }
        nSamplesOut += mSamplesInFrame ;
        mConcealSamples -= mSamplesInFrame ;
    }
	
    while (nBytesConsumed < nBytes && nSamplesOut + mSamplesInFrame <= maxSamplesOut)
    {
        faacDecFrameInfo fi ;

		const signed short* sampleBuffer = (const signed short*)faacDecDecode(mhDecoder, &fi,(unsigned char*)data + nBytesConsumed, nBytes - nBytesConsumed) ;

        if (fi.error > 0 ||
 //           fi.samples != (unsigned)mSamplesInFrame ||
            fi.bytesconsumed > (unsigned)mNumChannels * eAACMaxFrameBitsPerChannel/8)
        {
            return HXR_FAIL ;
        }

        memcpy(samplesOut + nSamplesOut, sampleBuffer, fi.samples * sizeof(short)); /* Flawfinder: ignore */
        if (fi.bytesconsumed)
        {
            // make sure we have place for this frame
            if (fi.bytesconsumed <= (unsigned)mNumChannels * eAACMaxFrameBitsPerChannel/8)
            {
                memcpy(mpLastframe, data+nBytesConsumed, fi.bytesconsumed) ; /* Flawfinder: ignore */ // save this frame
                mnBytesLastframe = fi.bytesconsumed ;
            }
            // if not, the stream is illegal -- should we return an error?
        }

		nBytesConsumed += fi.bytesconsumed ;
		if (fi.bytesconsumed) nSamplesOut += mSamplesInFrame ;
    }

    if (nBytesConsumed > nBytes)
    {
        FAILED(HXR_FAIL) ;
        return HXR_FAILED ;
    }

	return HXR_OK ;
}

STDMETHODIMP CAACDecoder::GetMaxSamplesOut(UINT32& nSamples) CONSTMETHOD
{
    nSamples = mSamplesInFrame ;
    return HXR_OK;
}

STDMETHODIMP CAACDecoder::GetNChannels (UINT32& nChannels) CONSTMETHOD
{
	nChannels = mNumChannels ;
	return HXR_OK ;
}

STDMETHODIMP CAACDecoder::GetSampleRate (UINT32& sampleRate) CONSTMETHOD
{
	sampleRate = mSampleRate ;
	return HXR_OK ;
}

STDMETHODIMP CAACDecoder::GetDelay (UINT32& nSamples) CONSTMETHOD
{
    nSamples = mSamplesInFrame ;
    return HXR_OK;
}
