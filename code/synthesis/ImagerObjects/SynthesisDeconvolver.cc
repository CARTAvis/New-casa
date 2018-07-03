//# SynthesisDeconvolver.cc: Implementation of Imager.h
//# Copyright (C) 1997-2008
//# Associated Universities, Inc. Washington DC, USA.
//#
//# This program is free software; you can redistribute it and/or modify it
//# under the terms of the GNU General Public License as published by the Free
//# Software Foundation; either version 2 of the License, or (at your option)
//# any later version.
//#
//# This program is distributed in the hope that it will be useful, but WITHOUT
//# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
//# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
//# more details.
//#
//# You should have received a copy of the GNU General Public License along
//# with this program; if not, write to the Free Software Foundation, Inc.,
//# 675 Massachusetts Ave, Cambridge, MA 02139, USA.
//#
//# Correspondence concerning AIPS++ should be addressed as follows:
//#        Internet email: aips2-request@nrao.edu.
//#        Postal address: AIPS++ Project Office
//#                        National Radio Astronomy Observatory
//#                        520 Edgemont Road
//#                        Charlottesville, VA 22903-2475 USA
//#
//# $Id$

#include <casa/Exceptions/Error.h>
#include <casa/iostream.h>
#include <casa/sstream.h>

#include <casa/Arrays/Matrix.h>
#include <casa/Arrays/ArrayMath.h>
#include <casa/Arrays/ArrayLogical.h>

#include <casa/Logging.h>
#include <casa/Logging/LogIO.h>
#include <casa/Logging/LogMessage.h>
#include <casa/Logging/LogSink.h>
#include <casa/Logging/LogMessage.h>

#include <casa/OS/DirectoryIterator.h>
#include <casa/OS/File.h>
#include <casa/OS/Path.h>

#include <casa/OS/HostInfo.h>

#include <images/Images/TempImage.h>
#include <images/Images/SubImage.h>
#include <images/Regions/ImageRegion.h>

#include <imageanalysis/ImageAnalysis/CasaImageBeamSet.h>
#include <synthesis/ImagerObjects/SynthesisDeconvolver.h>

#include <sys/types.h>
#include <unistd.h>
using namespace std;

using namespace casacore;
namespace casa { //# NAMESPACE CASA - BEGIN
  
  SynthesisDeconvolver::SynthesisDeconvolver() : 
				       itsDeconvolver( ), 
				       itsMaskHandler( ),
                                       itsImageName(""),
				       //                                       itsPartImageNames(Vector<String>(0)),
				       itsBeam(0.0),
				       itsDeconvolverId(0),
				       itsScales(Vector<Float>()),
                                       itsMaskType(""),
                                       itsPBMask(0.0),
				       //itsMaskString(String("")),
                                       itsIterDone(0.0),
                                       itsChanFlag(Vector<Bool>(False)),
                                       initializeChanMaskFlag(false),
				       itsIsMaskLoaded(false),
				       itsMaskSum(-1e+9)
  {
  }
  
  SynthesisDeconvolver::~SynthesisDeconvolver() 
  {
        LogIO os( LogOrigin("SynthesisDeconvolver","descructor",WHERE) );
	os << LogIO::DEBUG1 << "SynthesisDeconvolver destroyed" << LogIO::POST;
	SynthesisUtilMethods::getResource("End SynthesisDeconvolver");

  }

  void SynthesisDeconvolver::setupDeconvolution(const SynthesisParamsDeconv& decpars)
  {
    LogIO os( LogOrigin("SynthesisDeconvolver","setupDeconvolution",WHERE) );

    itsImageName = decpars.imageName;
    itsStartingModelNames = decpars.startModel;
    itsDeconvolverId = decpars.deconvolverId;
    
    os << "Set Deconvolution Options for [" << itsImageName << "] : " << decpars.algorithm ;
    if( itsStartingModelNames.nelements()>0 && itsStartingModelNames[0].length() > 0 ) 
      os << " , starting from model : " << itsStartingModelNames;
    os << LogIO::POST;

    try
      {
	if(decpars.algorithm==String("hogbom"))
	  {
	    itsDeconvolver.reset(new SDAlgorithmHogbomClean()); 
	  }
	else if(decpars.algorithm==String("mtmfs"))
	  {
	    itsDeconvolver.reset(new SDAlgorithmMSMFS( decpars.nTaylorTerms, decpars.scales )); 
	  } 
	else if(decpars.algorithm==String("clark_exp"))
	  {
	    itsDeconvolver.reset(new SDAlgorithmClarkClean("clark")); 
	  } 
	else if(decpars.algorithm==String("clarkstokes_exp"))
	  {
	    itsDeconvolver.reset(new SDAlgorithmClarkClean("clarkstokes")); 
	  } 
	else if(decpars.algorithm==String("clark"))
	  {
	    itsDeconvolver.reset(new SDAlgorithmClarkClean2("clark")); 
	  } 
	else if(decpars.algorithm==String("clarkstokes"))
	  {
	    itsDeconvolver.reset(new SDAlgorithmClarkClean2("clarkstokes")); 
	  } 
	else if(decpars.algorithm==String("multiscale"))
	  {
	    itsDeconvolver.reset(new SDAlgorithmMSClean( decpars.scales, decpars.scalebias )); 
	  } 
	else if(decpars.algorithm==String("mem"))
	  {
	    itsDeconvolver.reset(new SDAlgorithmMEM( "entropy" )); 
	  } 
	else if (decpars.algorithm==String("aasp"))
	  {
	    itsDeconvolver.reset(new SDAlgorithmAAspClean());
	  }
	else
	  {
	    throw( AipsError("Un-known algorithm : "+decpars.algorithm) );
	  }

	// Set restoring beam options
	itsDeconvolver->setRestoringBeam( decpars.restoringbeam, decpars.usebeam );
        itsUseBeam = decpars.usebeam;// store this info also here.

	// Set Masking options
	//	itsDeconvolver->setMaskOptions( decpars.maskType );
	itsMaskHandler.reset(new SDMaskHandler());
	//itsMaskString = decpars.maskString;
	itsMaskType = decpars.maskType;
        if(itsMaskType=="auto-thresh") 
          {
            itsAutoMaskAlgorithm="thresh";
          }
        else if(itsMaskType=="auto-thresh2")
          {
            itsAutoMaskAlgorithm="thresh2";
          }
        else if(itsMaskType=="auto-multithresh")
          { 
            itsAutoMaskAlgorithm="multithresh";
          }
        else if(itsMaskType=="auto-onebox")
          {
            itsAutoMaskAlgorithm="onebox";
          }
        else {
            itsAutoMaskAlgorithm="";
        }
        itsPBMask = decpars.pbMask;
        itsMaskString = decpars.maskString;
        if(decpars.maskList.nelements()==0 || 
            (decpars.maskList.nelements()==1 && decpars.maskList[0]==""))
          {
            itsMaskList.resize(1);
            itsMaskList[0] = itsMaskString;
          }
        else 
          {
            itsMaskList = decpars.maskList;
          }
        itsMaskThreshold = decpars.maskThreshold;
        itsFracOfPeak = decpars.fracOfPeak;
        itsMaskResolution = decpars.maskResolution;
        itsMaskResByBeam = decpars.maskResByBeam;
        itsNMask = decpars.nMask;
        //itsAutoAdjust = decpars.autoAdjust;
        //desable autoadjust 
        itsAutoAdjust = false;
        itsSidelobeThreshold = decpars.sidelobeThreshold;
        itsNoiseThreshold = decpars.noiseThreshold;
        itsLowNoiseThreshold = decpars.lowNoiseThreshold;
        itsNegativeThreshold = decpars.negativeThreshold;
        itsSmoothFactor = decpars.smoothFactor;
        itsMinBeamFrac = decpars.minBeamFrac;
        itsCutThreshold = decpars.cutThreshold;
        itsGrowIterations = decpars.growIterations;
        itsDoGrowPrune = decpars.doGrowPrune;
        itsMinPercentChange = decpars.minPercentChange;
        itsVerbose = decpars.verbose;
	itsIsInteractive = decpars.interactive;
        itsNsigma = decpars.nsigma;
      }
    catch(AipsError &x)
      {
	throw( AipsError("Error in constructing a Deconvolver : "+x.getMesg()) );
      }
    
    itsAddedModel=false;
  }
  
   Record SynthesisDeconvolver::initMinorCycle( )
  { 
    LogIO os( LogOrigin("SynthesisDeconvolver","initMinorCycle",WHERE) );
    Record returnRecord;

    try {
      
      //os << "---------------------------------------------------- Init (?) Minor Cycles ---------------------------------------------" << LogIO::POST;

      itsImages = makeImageStore( itsImageName );

      // If a starting model exists, this will initialize the ImageStore with it. Will do this only once.
      setStartingModel();

      //itsIterDone is currently only used by automask code so move this to inside setAutomask
      //itsIterDone += itsLoopController.getIterDone();

      //      setupMask();

      Float masksum;
      if( ! itsImages->hasMask() ) // i.e. if there is no existing mask to re-use...
	{ masksum = -1.0;}
      else 
	{
	  masksum = itsImages->getMaskSum();
	  itsImages->mask()->unlock();
	}
      Bool validMask = ( masksum > 0 );

      // Calculate Peak Residual and Max Psf Sidelobe, and fill into SubIterBot.
      Float peakresnomask = itsImages->getPeakResidual();
      itsLoopController.setPeakResidual( validMask ? itsImages->getPeakResidualWithinMask() : peakresnomask );
      itsLoopController.setPeakResidualNoMask( peakresnomask );
      itsLoopController.setMaxPsfSidelobe( itsImages->getPSFSidelobeLevel() );

      //re-calculate current nsigma threhold
      Array<Double> robustrms = itsImages->calcRobustRMS();
      // Before the first iteration the iteration parameters have not been
      // set in SIMinorCycleController
      // Use nsigma pass to SynthesisDeconvolver directly for now...
      //Float nsigma = itsLoopController.getNsigma();
      Double maxrobustrms = max(robustrms);
      //Float nsigmathresh = nsigma * (Float)robustrms(IPosition(1,0));
      Float nsigmathresh = itsNsigma * (Float)maxrobustrms;
      if (itsNsigma>0.0 ) os << "Current nsigma threshold (maximum along spectral channels) ="<<nsigmathresh<<LogIO::POST;
      itsLoopController.setNsigmaThreshold(nsigmathresh);


      if ( itsAutoMaskAlgorithm=="multithresh" && !initializeChanMaskFlag ) {
        IPosition maskshp = itsImages->mask()->shape();
        Int nchan = maskshp(3);
        itsChanFlag=Vector<Bool>(nchan,False);
        initializeChanMaskFlag=True;
        // also initialize posmask, which tracks only positive (emission) 
        itsPosMask = TempImage<Float> (maskshp, itsImages->mask()->coordinates(),SDMaskHandler::memoryToUse());
        itsPosMask.set(0);
      }
      os<<LogIO::DEBUG1<<"itsChanFlag.shape="<<itsChanFlag.shape()<<LogIO::POST;

      /*
      Array<Double> rmss = itsImages->calcRobustRMS();
      AlwaysAssert( rmss.shape()[0]>0 , AipsError);
      cout  << "madRMS : " << rmss << "  with shape : " << rmss.shape() << endl;
      //itsLoopController.setMadRMS( rmss[0] );
      */

      if( itsMaskSum != masksum || masksum == 0.0 ) // i.e. mask has changed. 
	{ 
	  itsMaskSum = masksum; 
	  itsLoopController.setMaskSum( masksum );
	}
      else // mask has not changed...
	{
	  itsLoopController.setMaskSum( -1.0 );
	}
      

      returnRecord = itsLoopController.getCycleInitializationRecord();

      itsImages->printImageStats(); 

      os << LogIO::DEBUG2 << "Initialized minor cycle. Returning returnRec" << LogIO::POST;

    } catch(AipsError &x) {
      throw( AipsError("Error initializing the Minor Cycle for "  + itsImageName + " : "+x.getMesg()) );
    }
    
    return returnRecord;
  }

  Record SynthesisDeconvolver::interactiveGUI(Record& iterRec)
  {
    LogIO os( LogOrigin("SynthesisDeconvolver","interactiveGUI",WHERE) );
    Record returnRecord;
    
    try {
      
      // Check that required parameters are present in the iterRec.
      Int niter=0,cycleniter=0,iterdone;
      Float threshold=0.0, cyclethreshold=0.0;
      if( iterRec.isDefined("niter") &&  
	  iterRec.isDefined("cycleniter") &&  
	  iterRec.isDefined("threshold") && 
	  iterRec.isDefined("cyclethreshold") &&
	  iterRec.isDefined("iterdone") )
	{
	  iterRec.get("niter", niter);
	  iterRec.get("cycleniter", cycleniter);
	  iterRec.get("threshold", threshold);
	  iterRec.get("cyclethreshold", cyclethreshold);
	  iterRec.get("iterdone",iterdone);
	}
      else throw(AipsError("SD::interactiveGui() needs valid niter, cycleniter, threshold to start up."));
      
      if( ! itsImages ) itsImages = makeImageStore( itsImageName );
      
      //      SDMaskHandler masker;
      String strthresh = String::toString(threshold)+"Jy";
      String strcycthresh = String::toString(cyclethreshold)+"Jy";

      if( itsMaskString.length()>0 ) {
	  itsMaskHandler->fillMask( itsImages, itsMaskString );
      }
      
      Int iterleft = niter - iterdone;
      if( iterleft<0 ) iterleft=0;
      
      Int stopcode = itsMaskHandler->makeInteractiveMask( itsImages, iterleft, cycleniter, strthresh, strcycthresh );
      
      Quantity qa;
      casacore::Quantity::read(qa,strthresh);
      threshold = qa.getValue(Unit("Jy"));
      casacore::Quantity::read(qa,strcycthresh);
      cyclethreshold = qa.getValue(Unit("Jy"));
      
      itsIsMaskLoaded=true;

      returnRecord.define( RecordFieldId("actioncode"), stopcode );
      returnRecord.define( RecordFieldId("niter"), iterdone + iterleft );
      returnRecord.define( RecordFieldId("cycleniter"), cycleniter );
      returnRecord.define( RecordFieldId("threshold"), threshold );
      returnRecord.define( RecordFieldId("cyclethreshold"), cyclethreshold );

    } catch(AipsError &x) {
      throw( AipsError("Error in Interactive GUI : "+x.getMesg()) );
    }
    return returnRecord;
  }

  
  
  Record SynthesisDeconvolver::executeMinorCycle(Record& minorCycleControlRec)
  {
    LogIO os( LogOrigin("SynthesisDeconvolver","executeMinorCycle",WHERE) );
    Record returnRecord;

    //    itsImages->printImageStats();

    os << "---------------------------------------------------- Run Minor Cycle Iterations  ---------------------------------------------" << LogIO::POST;

    SynthesisUtilMethods::getResource("Start Deconvolver");

    try {
      //      if ( !itsIsInteractive ) setAutoMask();
      itsLoopController.setCycleControls(minorCycleControlRec);

      itsDeconvolver->deconvolve( itsLoopController, itsImages, itsDeconvolverId );
      returnRecord = itsLoopController.getCycleExecutionRecord();

      //scatterModel(); // This is a no-op for the single-node case.

      itsImages->releaseLocks();

    } catch(AipsError &x) {
      throw( AipsError("Error in running Minor Cycle : "+x.getMesg()) );
    }

    SynthesisUtilMethods::getResource("End Deconvolver");

    return returnRecord;
  }

  // Restore Image.
  void SynthesisDeconvolver::restore()
  {
    LogIO os( LogOrigin("SynthesisDeconvolver","restoreImage",WHERE) );

    if( ! itsImages )
      {
	itsImages = makeImageStore( itsImageName );
      }

    SynthesisUtilMethods::getResource("Restoration");

    itsDeconvolver->restore(itsImages);
    itsImages->releaseLocks();

  }

  // Restore Image.
  void SynthesisDeconvolver::pbcor()
  {
    LogIO os( LogOrigin("SynthesisDeconvolver","pbcor",WHERE) );

    if( ! itsImages )
      {
	itsImages = makeImageStore( itsImageName );
      }

    itsDeconvolver->pbcor(itsImages);
    itsImages->releaseLocks();

  }



  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ////    Internal Functions start here.  These are not visible to the tool layer.
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  SHARED_PTR<SIImageStore> SynthesisDeconvolver::makeImageStore( String imagename )
  {
    SHARED_PTR<SIImageStore> imstore;
    if( itsDeconvolver->getAlgorithmName() == "mtmfs" )
      {  imstore.reset( new SIImageStoreMultiTerm( imagename, itsDeconvolver->getNTaylorTerms(), true ) ); }
    else
      {  imstore.reset( new SIImageStore( imagename, true ) ); }

    return imstore;

  }


  // #############################################
  // #############################################
  // #############################################
  // #############################################

  // Set a starting model.
  void SynthesisDeconvolver::setStartingModel()
  {
    LogIO os( LogOrigin("SynthesisDeconvolver","setStartingModel",WHERE) );

    if(itsAddedModel==true) {return;}
    
    try
      {
	
	if( itsStartingModelNames.nelements()>0 && itsImages )
	  {
	    //	    os << "Setting " << itsStartingModelNames << " as starting model for deconvolution " << LogIO::POST;
	    itsImages->setModelImage( itsStartingModelNames );
	  }

	itsAddedModel=true;
	
      }
    catch(AipsError &x)
      {
	throw( AipsError("Error in setting  starting model for deconvolution: "+x.getMesg()) );
      }

  }

  // Set mask
  Bool SynthesisDeconvolver::setupMask()
  {
    LogIO os( LogOrigin("SynthesisDeconvolver","setupMask",WHERE) );
    Bool maskchanged=False;
    if( itsIsMaskLoaded==false ) {
      // use mask(s) 
      if(  itsMaskList[0] != "" || itsMaskType == "pb" || itsAutoMaskAlgorithm != "" ) {
        // Skip automask for non-interactive mode. 
        if ( itsAutoMaskAlgorithm != "") { // && itsIsInteractive) {
	  os << "[" << itsImages->getName() << "] Setting up an auto-mask"<<  ((itsPBMask>0.0)?" within PB mask limit ":"") << LogIO::POST;

          setAutoMask();
          /***
          if ( itsPBMask > 0.0 ) {
            itsMaskHandler->autoMaskWithinPB( itsImages, itsAutoMaskAlgorithm, itsMaskThreshold, itsFracOfPeak, itsMaskResolution, itsMaskResByBeam, itsPBMask);
          }
          else { 
            cerr<<"automask by automask.."<<endl;
            itsMaskHandler->autoMask( itsImages, itsAutoMaskAlgorithm, itsMaskThreshold, itsFracOfPeak, itsMaskResolution, itsMaskResByBeam);
          }
          ***/
        }
        //else if( itsMaskType=="user" && itsMaskList[0] != "" ) {
        if( itsMaskType=="user" && itsMaskList[0] != "" ) {
	  os << "[" << itsImages->getName() << "] Setting up a mask from " << itsMaskList  <<  ((itsPBMask>0.0)?" within PB mask limit ":"") << LogIO::POST;

          itsMaskHandler->fillMask( itsImages, itsMaskList);
          if( itsPBMask > 0.0 ) {  
            itsMaskHandler->makePBMask(itsImages, itsPBMask, True);
          }
        }
        else if( itsMaskType=="pb") {
	  os << "[" << itsImages->getName() << "] Setting up a PB based mask" << LogIO::POST;
          itsMaskHandler->makePBMask(itsImages, itsPBMask);
        }

	os << "----------------------------------------------------------------------------------------------------------------------------------------" << LogIO::POST;

      } else {

	if( ! itsImages->hasMask() ) // i.e. if there is no existing mask to re-use...
	  {
	    if( itsIsInteractive ) itsImages->mask()->set(0.0);
	    else itsImages->mask()->set(1.0);
	    os << "[" << itsImages->getName() << "] Initializing new mask to " << (itsIsInteractive?"0.0 for interactive drawing":"1.0 for the full image") << LogIO::POST;
	  }
	else {
	  os << "[" << itsImages->getName() << "] Initializing to existing mask" << LogIO::POST;
	}

      }
	
      // If anything other than automasking, don't re-make the mask here.
      if ( itsAutoMaskAlgorithm == "" )
        {	itsIsMaskLoaded=true; }
   

      // Get the number of mask pixels (sum) and send to the logger.
      Float masksum = itsImages->getMaskSum();
      Float npix = (itsImages->getShape()).product();

      //Int npix2 = 20000*20000*16000*4;
      //Float npix2f = 20000*20000*16000*4;

      //cout << " bigval : " << npix2 << " and " << npix2f << endl;

      os << "[" << itsImages->getName() << "] Number of pixels in the clean mask : " << masksum << " out of a total of " << npix << " pixels. [ " << 100.0 * masksum/npix << " % ]" << LogIO::POST;

      maskchanged=True;
    }
    else {
    }

    itsImages->mask()->unlock();
    return maskchanged;
}

  void SynthesisDeconvolver::setAutoMask()
  {
     //modify mask using automask otherwise no-op
     if ( itsAutoMaskAlgorithm != "" )  {
       itsIterDone += itsLoopController.getIterDone();

       Bool isThresholdReached = itsLoopController.isThresholdReached();

       LogIO os( LogOrigin("SynthesisDeconvolver","setAutoMask",WHERE) );
       os << "Generating AutoMask" << LogIO::POST;
       //os << "itsMinPercentChnage = " << itsMinPercentChange<< LogIO::POST;

       if ( itsPBMask > 0.0 ) {
         itsMaskHandler->autoMaskWithinPB( itsImages, itsPosMask, itsIterDone, itsChanFlag, itsAutoMaskAlgorithm, itsMaskThreshold, itsFracOfPeak, itsMaskResolution, itsMaskResByBeam, itsNMask, itsAutoAdjust,  itsSidelobeThreshold, itsNoiseThreshold, itsLowNoiseThreshold, itsNegativeThreshold,itsCutThreshold, itsSmoothFactor, itsMinBeamFrac, itsGrowIterations, itsDoGrowPrune, itsMinPercentChange, itsVerbose, isThresholdReached, itsPBMask);
       }
       else {
         itsMaskHandler->autoMask( itsImages, itsPosMask, itsIterDone, itsChanFlag,itsAutoMaskAlgorithm, itsMaskThreshold, itsFracOfPeak, itsMaskResolution, itsMaskResByBeam, itsNMask, itsAutoAdjust, itsSidelobeThreshold, itsNoiseThreshold, itsLowNoiseThreshold, itsNegativeThreshold, itsCutThreshold, itsSmoothFactor, itsMinBeamFrac, itsGrowIterations, itsDoGrowPrune, itsMinPercentChange, itsVerbose, isThresholdReached );
       }
     }
  }

  // check if restoring beam is reasonable 
  void SynthesisDeconvolver::checkRestoringBeam() 
  {
    LogIO os( LogOrigin("SynthesisDeconvolver","checkRestoringBeam",WHERE) );
    //check for a bad restoring beam
    GaussianBeam beam;
    
    if( ! itsImages ) itsImages = makeImageStore( itsImageName );
    ImageInfo psfInfo = itsImages->psf()->imageInfo();
    if (psfInfo.hasSingleBeam()) {
      beam = psfInfo.restoringBeam();  
    }
    else if (psfInfo.hasMultipleBeams() && itsUseBeam=="common") {
      beam = CasaImageBeamSet(psfInfo.getBeamSet()).getCommonBeam(); 
    }
    Double beammaj = beam.getMajor(Unit("arcsec"));
    Double beammin = beam.getMinor(Unit("arcsec"));
    if (std::isinf(beammaj) || std::isinf(beammin)) {
      String msg;
      if (itsUseBeam=="common") {
        msg+="Bad restoring beam using the common beam (at least one of the beam axes has an infinite size)  ";
        throw(AipsError(msg));
      }
    }
  }
    
  // This is for interactive-clean.
  void SynthesisDeconvolver::getCopyOfResidualAndMask( TempImage<Float> &/*residual*/,
                                           TempImage<Float> &/*mask*/ )
  {
    // Actually all I think we need here are filenames JSK 12/12/12
    // resize/shape and copy the residual image and mask image to these in/out variables.
    // Allocate Memory here.
  }
  void SynthesisDeconvolver::setMask( TempImage<Float> &/*mask*/ )
  {
    // Here we will just pass in the new names
    // Copy the input mask to the local main image mask
  }

} //# NAMESPACE CASA - END

