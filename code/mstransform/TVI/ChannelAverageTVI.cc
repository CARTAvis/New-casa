//# ChannelAverageTVI.h: This file contains the implementation of the ChannelAverageTVI class.
//#
//#  CASA - Common Astronomy Software Applications (http://casa.nrao.edu/)
//#  Copyright (C) Associated Universities, Inc. Washington DC, USA 2011, All rights reserved.
//#  Copyright (C) European Southern Observatory, 2011, All rights reserved.
//#
//#  This library is free software; you can redistribute it and/or
//#  modify it under the terms of the GNU Lesser General Public
//#  License as published by the Free software Foundation; either
//#  version 2.1 of the License, or (at your option) any later version.
//#
//#  This library is distributed in the hope that it will be useful,
//#  but WITHOUT ANY WARRANTY, without even the implied warranty of
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//#  Lesser General Public License for more details.
//#
//#  You should have received a copy of the GNU Lesser General Public
//#  License along with this library; if not, write to the Free Software
//#  Foundation, Inc., 59 Temple Place, Suite 330, Boston,
//#  MA 02111-1307  USA
//# $Id: $

#include <mstransform/TVI/ChannelAverageTVI.h>

namespace casa { //# NAMESPACE CASA - BEGIN

namespace vi { //# NAMESPACE VI - BEGIN

//////////////////////////////////////////////////////////////////////////
// ChannelAverageTVI class
//////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
ChannelAverageTVI::ChannelAverageTVI(	ViImplementation2 * inputVii,
										const Record &configuration):
										FreqAxisTVI (inputVii,configuration)
{
	// Parse and check configuration parameters
	// Note: if a constructor finishes by throwing an exception, the memory
	// associated with the object itself is cleaned up — there is no memory leak.
	if (not parseConfiguration(configuration))
	{
		throw AipsError("Error parsing ChannelAverageTVI configuration");
	}

	initialize();

	return;
}

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
Bool ChannelAverageTVI::parseConfiguration(const Record &configuration)
{
	int exists = 0;
	Bool ret = True;

	// Parse chanbin parameter (mandatory)
	exists = configuration.fieldNumber ("chanbin");
	if (exists >= 0)
	{
		if ( configuration.type(exists) == casa::TpInt )
		{
			Int freqbin;
			configuration.get (exists, freqbin);
			chanbin_p = Vector<Int>(spwInpChanIdxMap_p.size(),freqbin);
		}
		else if ( configuration.type(exists) == casa::TpArrayInt)
		{
			configuration.get (exists, chanbin_p);
		}
		else
		{
			ret = False;
			logger_p << LogIO::SEVERE << LogOrigin("ChannelAverageTVI", __FUNCTION__)
					<< "Wrong format for chanbin parameter (only Int and arrayInt are supported) "
					<< LogIO::POST;
		}

		logger_p << LogIO::NORMAL << LogOrigin("ChannelAverageTVI", __FUNCTION__)
				<< "Channel bin is " << chanbin_p << LogIO::POST;
	}
	else
	{
		ret = False;
		logger_p << LogIO::SEVERE << LogOrigin("ChannelAverageTVI", __FUNCTION__)
				<< "chanbin parameter not found in configuration "
				<< LogIO::POST;
	}

	// Check consistency between chanbin vector and selected SPW/Chan map
	if (chanbin_p.size() !=  spwInpChanIdxMap_p.size())
	{
		ret = False;
		logger_p << LogIO::SEVERE << LogOrigin("ChannelAverageTVI", __FUNCTION__)
				<< "Number of elements in chanbin vector does not match number of selected SPWs"
				<< LogIO::POST;
	}

	return ret;
}

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
void ChannelAverageTVI::initialize()
{
	// Populate input-output maps
	Int spw;
	uInt spw_idx = 0;
	map<Int,vector<Int> >::iterator iter;
	for(iter=spwInpChanIdxMap_p.begin();iter!=spwInpChanIdxMap_p.end();iter++)
	{
		spw = iter->first;

		// Make sure that chanbin does not exceed number of selected channels
		if (iter->second.size() < (uInt)chanbin_p(spw_idx))
		{
			logger_p << LogIO::WARN << LogOrigin("MSTransformManager", __FUNCTION__)
					<< "Number of selected channels " << iter->second.size()
					<< " for SPW " << spw
					<< " is smaller than specified chanbin " << chanbin_p(spw_idx) << endl
					<< "Setting chanbin to " << iter->second.size()
					<< " for SPW " << spw
					<< LogIO::POST;
			spwChanbinMap_p[spw] = iter->second.size();
		}
		else
		{
			spwChanbinMap_p[spw] = chanbin_p(spw_idx);
		}

		// Calculate number of output channels per spw
		spwOutChanNumMap_p[spw] = spwInpChanIdxMap_p[spw].size() / spwChanbinMap_p[spw];
		if (spwInpChanIdxMap_p[spw].size() % spwChanbinMap_p[spw] > 0) spwOutChanNumMap_p[spw] += 1;

		spw_idx++;
	}

	return;
}

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
void ChannelAverageTVI::flag(Cube<Bool>& flagCube) const
{
	// Get input VisBuffer and SPW
	VisBuffer2 *vb = getVii()->getVisBuffer();
	Int inputSPW = vb->spectralWindows()(0);

	// Configure Transformation Engine
	FlagChannelAverageKernel<Bool> kernel;
	uInt width = spwChanbinMap_p[inputSPW];
	ChannelAverageTransformEngine<Bool> transformer(&(kernel),width);

	// Dummy auxiliary data
	DataCubeMap auxiliaryData;

	// Transform data
	transformFreqAxis(vb->flagCube(),flagCube,auxiliaryData,transformer);

	return;
}

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
void ChannelAverageTVI::floatData (Cube<Float> & vis) const
{
	// Get input VisBuffer and SPW
	VisBuffer2 *vb = getVii()->getVisBuffer();
	Int inputSPW = vb->spectralWindows()(0);

	// Configure Transformation Engine
	DataChannelAverageKernel<Float> kernel;
	uInt width = spwChanbinMap_p[inputSPW];
	ChannelAverageTransformEngine<Float> transformer(&(kernel),width);

	// Configure auxiliary data
	DataCubeMap auxiliaryData;
	DataCubeHolder<Bool> flagCubeHolder(vb->flagCube());
	DataCubeHolder<Float> weightCubeHolder(vb->weightSpectrum());
	auxiliaryData.add(MS::FLAG,flagCubeHolder);
	auxiliaryData.add(MS::WEIGHT_SPECTRUM,weightCubeHolder);

	// Transform data
	transformFreqAxis(vb->visCubeFloat(),vis,auxiliaryData,transformer);

	return;
}

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
void ChannelAverageTVI::visibilityObserved (Cube<Complex> & vis) const
{
	// Get input VisBuffer and SPW
	VisBuffer2 *vb = getVii()->getVisBuffer();
	Int inputSPW = vb->spectralWindows()(0);

	// Configure Transformation Engine
	DataChannelAverageKernel<Complex> kernel;
	uInt width = spwChanbinMap_p[inputSPW];
	ChannelAverageTransformEngine<Complex> transformer(&(kernel),width);

	// Get weightSpectrum from sigmaSpectrum
	Cube<Float> weightSpectrum;
	weightSpectrum.resize(vb->sigmaSpectrum().shape(),False);
	weightSpectrum = vb->sigmaSpectrum(); // = Operator makes a copy
	arrayTransformInPlace (weightSpectrum,sigmaToWeight);

	// Configure auxiliary data
	DataCubeMap auxiliaryData;
	DataCubeHolder<Bool> flagCubeHolder(vb->flagCube());
	DataCubeHolder<Float> weightCubeHolder(weightSpectrum);
	auxiliaryData.add(MS::FLAG,flagCubeHolder);
	auxiliaryData.add(MS::WEIGHT_SPECTRUM,weightCubeHolder);

	// Transform data
	transformFreqAxis(vb->visCube(),vis,auxiliaryData,transformer);

	return;
}

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
void ChannelAverageTVI::visibilityCorrected (Cube<Complex> & vis) const
{
	// Get input VisBuffer and SPW
	VisBuffer2 *vb = getVii()->getVisBuffer();
	Int inputSPW = vb->spectralWindows()(0);

	// Configure Transformation Engine
	DataChannelAverageKernel<Complex> kernel;
	uInt width = spwChanbinMap_p[inputSPW];
	ChannelAverageTransformEngine<Complex> transformer(&(kernel),width);

	// Configure auxiliary data
	DataCubeMap auxiliaryData;
	DataCubeHolder<Bool> flagCubeHolder(vb->flagCube());
	DataCubeHolder<Float> weightCubeHolder(vb->weightSpectrum());
	auxiliaryData.add(MS::FLAG,flagCubeHolder);
	auxiliaryData.add(MS::WEIGHT_SPECTRUM,weightCubeHolder);

	// Transform data
	transformFreqAxis(vb->visCubeCorrected(),vis,auxiliaryData,transformer);

	return;
}

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
void ChannelAverageTVI::visibilityModel (Cube<Complex> & vis) const
{
	// Get input VisBuffer and SPW
	VisBuffer2 *vb = getVii()->getVisBuffer();
	Int inputSPW = vb->spectralWindows()(0);

	// Configure Transformation Engine
	DataChannelAverageKernel<Complex> kernel;
	uInt width = spwChanbinMap_p[inputSPW];
	ChannelAverageTransformEngine<Complex> transformer(&(kernel),width);

	// Configure auxiliary data
	DataCubeMap auxiliaryData;
	DataCubeHolder<Bool> flagCubeHolder(vb->flagCube());
	DataCubeHolder<Float> weightCubeHolder(vb->weightSpectrum());
	auxiliaryData.add(MS::FLAG,flagCubeHolder);
	auxiliaryData.add(MS::WEIGHT_SPECTRUM,weightCubeHolder);

	// Transform data
	transformFreqAxis(vb->visCubeModel(),vis,auxiliaryData,transformer);

	return;
}

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
void ChannelAverageTVI::weightSpectrum(Cube<Float> &weightSp) const
{
	// Get input VisBuffer and SPW
	VisBuffer2 *vb = getVii()->getVisBuffer();
	Int inputSPW = vb->spectralWindows()(0);

	// Configure Transformation Engine
	WeightChannelAverageKernel<Float> kernel;
	uInt width = spwChanbinMap_p[inputSPW];
	ChannelAverageTransformEngine<Float> transformer(&(kernel),width);

	// Configure auxiliary data
	DataCubeMap auxiliaryData;
	DataCubeHolder<Bool> flagCubeHolder(vb->flagCube());
	auxiliaryData.add(MS::FLAG,flagCubeHolder);

	// Transform data
	transformFreqAxis(vb->weightSpectrum(),weightSp,auxiliaryData,transformer);

	return;
}

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
void ChannelAverageTVI::sigmaSpectrum(Cube<Float> &sigmaSp) const
{
	// Get input VisBuffer and SPW
	VisBuffer2 *vb = getVii()->getVisBuffer();
	Int inputSPW = vb->spectralWindows()(0);

	// Configure Transformation Engine
	SigmaChannelAverageKernel<Float> kernel;
	uInt width = spwChanbinMap_p[inputSPW];
	ChannelAverageTransformEngine<Float> transformer(&(kernel),width);

	// Configure auxiliary data
	DataCubeMap auxiliaryData;
	DataCubeHolder<Bool> flagCubeHolder(vb->flagCube());
	auxiliaryData.add(MS::FLAG,flagCubeHolder);

	// Transform data
	transformFreqAxis(vb->sigmaSpectrum(),sigmaSp,auxiliaryData,transformer);

	return;
}

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
void ChannelAverageTVI::writeFlag (const Cube<Bool> & flag)
{
	return;
}

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
void ChannelAverageTVI::writeFlagRow (const Vector<Bool> & rowflags)
{
	return;
}

//////////////////////////////////////////////////////////////////////////
// ChannelAverageTVIFactory class
//////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
ChannelAverageTVIFactory::ChannelAverageTVIFactory (Record &configuration,
													ViImplementation2 *inputVii)
{
	inputVii_p = inputVii;
	configuration_p = configuration;
}

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
vi::ViImplementation2 * ChannelAverageTVIFactory::createVi(VisibilityIterator2 *) const
{
	return new ChannelAverageTVI(inputVii_p,configuration_p);
}

//////////////////////////////////////////////////////////////////////////
// ChannelAverageTransformEngine class
//////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
template<class T> ChannelAverageTransformEngine<T>::ChannelAverageTransformEngine
													(ChannelAverageKernel<T> *kernel,
													uInt width)
{
	width_p = width;
	chanAvgKernel_p = kernel;
}

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
template<class T> void ChannelAverageTransformEngine<T>::transform(Vector<T> &inputVector,
																	Vector<T> &outputVector,
																	DataCubeMap &auxiliaryData)
{

	uInt startChan = 0;
	uInt outChanIndex = 0;
	uInt tail = inputVector.size() % width_p;
	uInt limit = inputVector.size() - tail;
	while (startChan < limit)
	{
		chanAvgKernel_p->kernel(inputVector,outputVector,auxiliaryData,
								startChan,outChanIndex,width_p);
		startChan += width_p;
		outChanIndex += 1;
	}

	if (tail and (outChanIndex <= outputVector.size()-1) )
	{
		chanAvgKernel_p->kernel(inputVector,outputVector,auxiliaryData,
								startChan,outChanIndex,tail);
	}

	return;
}

//////////////////////////////////////////////////////////////////////////
// DataChannelAverageKernel class
//////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
template<class T> void DataChannelAverageKernel<T>::kernel(	Vector<T> &inputVector,
															Vector<T> &outputVector,
															DataCubeMap &auxiliaryData,
															uInt startInputPos,
															uInt outputPos,
															uInt width)
{
	T avg = 0;
	T normalization = 0;
	uInt inputPos = 0;
	Vector<Bool> &inputFlagVector = auxiliaryData.getVector<Bool>(MS::FLAG);
	Vector<Float> &inputWeightVector = auxiliaryData.getVector<Float>(MS::WEIGHT_SPECTRUM);
	Bool accumulatorFlag = inputFlagVector(startInputPos);

	for (uInt sample_i=0;sample_i<width;sample_i++)
	{
		// Get input index
		inputPos = startInputPos + sample_i;

		// True/True or False/False
		if (accumulatorFlag == inputFlagVector(inputPos))
		{
			normalization += inputWeightVector(inputPos);
			avg += inputVector(inputPos)*inputWeightVector(inputPos);
		}
		// True/False: Reset accumulation when accumulator switches from flagged to unflag
		else if ( (accumulatorFlag == True) and (inputFlagVector(inputPos) == False) )
		{
			accumulatorFlag = False;
			normalization = inputWeightVector(inputPos);
			avg = inputVector(inputPos)*inputWeightVector(inputPos);
		}
	}


	// Apply normalization factor
	if (normalization > 0)
	{
		avg /= normalization;
		outputVector(outputPos) = avg;
	}
	// If all weights are zero set accumulatorFlag to True
	else
	{
		accumulatorFlag = True;
		outputVector(outputPos) = 0; // If all weights are zero then the avg is 0 too
	}

	return;
}

//////////////////////////////////////////////////////////////////////////
// FlagChannelAverageKernel class
//////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
template<class T> void FlagChannelAverageKernel<T>::kernel(	Vector<T> &inputVector,
															Vector<T> &outputVector,
															DataCubeMap &,
															uInt startInputPos,
															uInt outputPos,
															uInt width)
{
	Bool outputFlag = True;
	for (uInt sample_i=0;sample_i<width;sample_i++)
	{
		if (not inputVector(startInputPos+sample_i))
		{
			outputFlag = False;
			break;
		}
	}

	outputVector(outputPos) = outputFlag;

	return;
}

//////////////////////////////////////////////////////////////////////////
// WeightChannelAverageKernel class
//////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
template<class T> void WeightChannelAverageKernel<T>::kernel(	Vector<T> &inputVector,
																Vector<T> &outputVector,
																DataCubeMap &auxiliaryData,
																uInt startInputPos,
																uInt outputPos,
																uInt width)
{
	T acc = 0;
	uInt inputPos = 0;
	Vector<Bool> &inputFlagVector = auxiliaryData.getVector<Bool>(MS::FLAG);
	Bool accumulatorFlag = inputFlagVector(startInputPos);

	for (uInt sample_i=0;sample_i<width;sample_i++)
	{
		// Get input index
		inputPos = startInputPos + sample_i;

		// True/True or False/False
		if (accumulatorFlag == inputFlagVector(inputPos))
		{
			acc += inputVector(inputPos);
		}
		// True/False: Reset accumulation when accumulator switches from flagged to unflag
		else if ( (accumulatorFlag == True) and (inputFlagVector(inputPos) == False) )
		{
			accumulatorFlag = False;
			acc = inputVector(inputPos);
		}
	}


	outputVector(outputPos) = acc;

	return;
}

//////////////////////////////////////////////////////////////////////////
// SigmaChannelAverageKernel class
//////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------
//
// -----------------------------------------------------------------------
template<class T> void SigmaChannelAverageKernel<T>::kernel(	Vector<T> &inputVector,
																Vector<T> &outputVector,
																DataCubeMap &auxiliaryData,
																uInt startInputPos,
																uInt outputPos,
																uInt width)
{
	T acc = 0;
	uInt inputPos = 0;
	Vector<Bool> &inputFlagVector = auxiliaryData.getVector<Bool>(MS::FLAG);
	Bool accumulatorFlag = inputFlagVector(startInputPos);

	for (uInt sample_i=0;sample_i<width;sample_i++)
	{
		// Get input index
		inputPos = startInputPos + sample_i;

		// True/True or False/False
		if (accumulatorFlag == inputFlagVector(inputPos))
		{
			acc += sigmaToWeight(inputVector(inputPos));
		}
		// True/False: Reset accumulation when accumulator switches from flagged to unflag
		else if ( (accumulatorFlag == True) and (inputFlagVector(inputPos) == False) )
		{
			accumulatorFlag = False;
			acc = sigmaToWeight(inputVector(inputPos));
		}
	}


	// Transform weight into sigma format
	outputVector(outputPos) = weightToSigma(acc);

	return;
}


} //# NAMESPACE VI - END

} //# NAMESPACE CASA - END


