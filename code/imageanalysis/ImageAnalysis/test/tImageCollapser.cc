//# tImageFitter.cc:  test the PagedImage class
//# Copyright (C) 1994,1995,1998,1999,2000,2001,2002
//# Associated Universities, Inc. Washington DC, USA.
//#
//# This program is free software; you can redistribute it and/or modify it
//# under the terms of the GNU General Public License as published by the Free
//# Software Foundation; either version 2 of the License, or(at your option)
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
//# $Id: $

#include <imageanalysis/ImageAnalysis/ImageCollapser.h>

#include <images/Images/ImageUtilities.h>
#include <images/Images/FITSImage.h>
#include <images/Images/PagedImage.h>

#include <casa/OS/Directory.h>
#include <casa/OS/EnvVar.h>

#include <imageanalysis/ImageAnalysis/ImageFactory.h>

#include <casa/namespace.h>

#include <sys/types.h>

#include <unistd.h>
#include <iomanip>

using namespace casa;

uInt testNumber = 0;

void writeTestString(const String& test) {
    cout << "\n" << "*** " << testNumber << ": "
    	<< test << " ***" << endl;
    testNumber++;
}

String dirName;

String outname() {
	return dirName + "/" + "test_" + String::toString(testNumber) + ".im";

}

void checkImage(
	SPCIIF gotImage, const String& expectedName
) {
	FITSImage expectedImage(expectedName);
	AlwaysAssert(gotImage->shape() == expectedImage.shape(), AipsError);
	Array<Float> diffData = gotImage->get() - expectedImage.get();
	AlwaysAssert(max(abs(diffData)) == 0, AipsError);
	CoordinateSystem gotCsys = gotImage->coordinates();
	CoordinateSystem expectedCsys = expectedImage.coordinates();
	Array<Double> diffPixels = gotCsys.referencePixel() - expectedCsys.referencePixel();
	AlwaysAssert(max(abs(diffPixels)) == 0, AipsError);
	Array<Double> fracDiffRef = (
			gotCsys.referenceValue() - expectedCsys.referenceValue()
		)/expectedCsys.referenceValue();
	AlwaysAssert(max(abs(fracDiffRef)) <= 1.5e-6, AipsError);
	GaussianBeam beam = gotImage->imageInfo().restoringBeam();
	AlwaysAssert(near(beam.getMajor().getValue("arcsec"), 1.0), AipsError);
	AlwaysAssert(near(beam.getMinor().getValue("arcsec"), 1.0), AipsError);
	AlwaysAssert(near(beam.getPA().getValue("deg"), 40.0), AipsError);

}

void checkImage(
	const String& gotName, const String& expectedName
) {
	SPCIIF gotImage(new PagedImage<Float>(gotName));
	checkImage(gotImage, expectedName);
}


void testException(
	const String& test, const String& aggString,
    SPCIIF image, const String& mask,
    const uInt compressionAxis
) {
	writeTestString(test);
	Bool exceptionThrown = true;
	IPosition axes(1, compressionAxis);
	try {
		ImageCollapser<Float> collapser(
			aggString, image, nullptr,
			mask, axes, false, outname(), false
		);

		// should not get here, fail if we do.
		exceptionThrown = false;
		AlwaysAssert(false, AipsError);
	}
	catch (const AipsError& x) {
		AlwaysAssert(exceptionThrown, AipsError);
	}
}

int main() {
    pid_t pid = getpid();
    ostringstream os;
	String *parts = new String[2];
	split(EnvironmentVariable::get("CASAPATH"), parts, 2, String(" "));
	String datadir = parts[0] + "/data/regression/unittest/imageanalysis/ImageAnalysis/";
	delete [] parts;
    os << "tImageCollapser_tmp_" << pid;
    dirName = os.str();
	Directory workdir(dirName);
    SPCIIF goodImage(new FITSImage(datadir + "collapse_in.fits"));
    const String ALL = CasacRegionManager::ALL;
	workdir.create();
	uInt retVal = 0;
    try {
    	testException(
    		"Exception if no aggregate string given", "",
    		goodImage, "", 0
    	);
    	testException(
    		"Exception if bogus aggregate string given", "bogus function",
    		goodImage, "", 0
    	);
    	{
    		writeTestString("average full image collapse along axis 0");
    		ImageCollapser<Float> collapser(
    			"mean", goodImage, nullptr, "", IPosition(1, 0),
				false, outname(), false
    		);
    		collapser.collapse();
    		checkImage(outname(), datadir + "collapse_avg_0.fits");
    	}
    	{
    		writeTestString("average full image collapse along axis 2");
    		ImageCollapser<Float> collapser(
    			"mean", goodImage, nullptr, "", IPosition(1, 2),
				false, outname(), false
    		);
    		collapser.collapse();
    		checkImage(outname(), datadir + "collapse_avg_2.fits");
    	}
    	{
    		writeTestString("sum subimage collapse along axis 1");
    		String diagnostics;
    		uInt nSelectedChannels;
    		String stokes = "qu";
    		String mask;
    		String chans = "1~2";
    		String box = "1,1,2,2";
    		CasacRegionManager rm(goodImage->coordinates());
    		auto region = rm.fromBCS(
    			diagnostics, nSelectedChannels, stokes,
				nullptr, mask, chans,
				CasacRegionManager::USE_ALL_STOKES, box,
				goodImage->shape(), goodImage->name(),
				false
    		);
    		ImageCollapser<Float> collapser(
    			"sum", goodImage, &region, "",
				IPosition(1, 2), false, outname(), false
    		);
    		collapser.collapse();
    		// and check that we can overwrite the previous output
    		ImageCollapser<Float> collapser2(
        		"sum", goodImage, &region, "", IPosition(1, 1),
				false, outname(), true
        	);
    		collapser2.collapse();
    		checkImage(outname(), datadir + "collapse_sum_1.fits");
    	}
    	{
    		writeTestString("Check not specifying out file is ok");
    		ImageCollapser<Float> collapser(
    			"mean", goodImage, nullptr, "",
				IPosition(1, 2), false, "", false
    		);
    		auto collapsed = collapser.collapse();
    		checkImage(collapsed, datadir + "collapse_avg_2.fits");
    	}
    	{
    		writeTestString("average full image collapse along all axes but 0");
    		IPosition axes(3, 1, 2, 3);

    		ImageCollapser<Float> collapser(
    			"max", goodImage, nullptr, "", axes,
				false, outname(), false
    		);
    		collapser.collapse();
    		checkImage(outname(), datadir + "collapse_max_0_a.fits");
    	}
       	{
        	writeTestString("average full temporary image collapse along axis 0");
        	auto ret = ImageFactory::fromImage("", goodImage->name(), Record(), "");
        	auto tIm = std::get<0>(ret);
        	ImageCollapser<Float> collapser(
        		"mean", tIm, nullptr, "", IPosition(1, 0),
				false, outname(), false
        	);
        	collapser.collapse();
        	checkImage(outname(), datadir + "collapse_avg_0.fits");
        }
       	{
        	writeTestString("full image collapse along axes 0, 1");
        	IPosition axes(2, 0, 1);
        	ImageCollapser<Float> collapser(
        		"mean", goodImage, nullptr, "", axes,
				false, outname(), false
        	);
        	collapser.collapse();
        	checkImage(outname(), datadir + "collapse_avg_0_1.fits");
        }
        cout << "ok" << endl;
    }
    catch (const AipsError& x) {
        cerr << "Exception caught: " << x.getMesg() << endl;
        retVal = 1;
    } 
	workdir.removeRecursive();

    return retVal;
}

