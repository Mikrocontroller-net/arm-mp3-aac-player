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

#ifndef _AACDECODER_H_
#define _AACDECODER_H_

#include "hxresult.h"
#include "hxtypes.h"
#include "hxcom.h"
#include "hxacodec.h"
#include "gaConfig.h"

typedef void *faacDecHandle ;

class CAACDecoder : public IHXAudioDecoder
{
public:
    CAACDecoder() ;

    /*
     *	IUnknown methods
     */
    STDMETHOD(QueryInterface)	(THIS_
				REFIID riid,
				void** ppvObj) ;

    STDMETHOD_(ULONG32,AddRef)	(THIS) ;

    STDMETHOD_(ULONG32,Release)	(THIS) ;

    /*
     *	IHXAudioDecoder methods
     */

    STDMETHOD(OpenDecoder) (THIS_
							UINT32 configType,
							const void* pConfig,
							UINT32 nBytes) ;

    STDMETHOD(Reset) (THIS) ;

    STDMETHOD(Conceal) (THIS_
						UINT32 nSamples) ;
    STDMETHOD(Decode)  (THIS_
						const UCHAR* data, UINT32 nBytes, UINT32 &nBytesConsumed, INT16 *samplesOut, UINT32& nSamplesOut, HXBOOL eof) ;
    STDMETHOD(GetMaxSamplesOut)   (THIS_
								UINT32& nSamples) CONSTMETHOD ;
	STDMETHOD(GetNChannels) (THIS_
								UINT32& nChannels) CONSTMETHOD ;
	STDMETHOD(GetSampleRate) (THIS_
								UINT32& sampleRate) CONSTMETHOD ;
    STDMETHOD(GetDelay) (THIS_
								UINT32& nSamples) CONSTMETHOD ;

protected:
    virtual void VerifyInput(UCHAR* pData, UINT32 nBytes, UINT32 nBufSize) {};

    virtual ~CAACDecoder() ;

private:
	// Declaring these private, and not providing an implementation
	// makes it a compile error to accidentally try to use them
	CAACDecoder( const CAACDecoder& ) ;
	CAACDecoder& operator =( const CAACDecoder& ) ;

	CAudioSpecificConfig mConfig ;

    unsigned char *mpDecoderSpecificInfo ;
    int mDecoderSpecificInfoSize ;

    faacDecHandle mhDecoder ;
    int mSampleRate ;
    int mNumChannels ;
    int mDecoderInit ;
    int mConcealSamples ;

    int mSamplesInFrame ;

    UCHAR* mpLastframe ;
    int mnBytesLastframe ;

	INT32 m_lRefCount ;
} ;

#endif // _AACDECODER_H_
