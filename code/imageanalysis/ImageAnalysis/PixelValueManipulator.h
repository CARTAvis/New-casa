#ifndef IMAGEANALYSIS_PIXELVALUEMANIPULATOR_H
#define IMAGEANALYSIS_PIXELVALUEMANIPULATOR_H

#include <imageanalysis/ImageAnalysis/ImageTask.h>

#include <imageanalysis/ImageAnalysis/ImageCollapserData.h>
#include <imageanalysis/ImageAnalysis/PixelValueManipulatorData.h>


namespace casa {

template <class T> class PixelValueManipulator : public ImageTask<T> {
	// <summary>
	// Top level interface for getting and setting image pixel values.
	// </summary>

	// <reviewed reviewer="" date="" tests="" demos="">
	// </reviewed>

	// <prerequisite>
	// </prerequisite>

	// <etymology>
	// Manipulates pixel values.
	// </etymology>

	// <synopsis>
	// Top level interface for getting and setting image pixel values.
	// </synopsis>

	// <example>
	// </example>

public:

	PixelValueManipulator() = delete;

	// regionRec = 0 => no region selected, full image used
	// mask = "" => No additional mask specification, although image default mask will be used
	// if one exists.
	PixelValueManipulator(
		const SPCIIT image, const Record *const regionRec,
		const String& mask, Bool verboseDuringConstruction=True
	);

	~PixelValueManipulator() {}

	static void addNoise(
		SPIIT image, const String& type, const Record& region, const Vector<Double>& pars,
		Bool zero, const std::pair<Int, Int> *const &seeds
	);

	// set axes to average over. If invert is True, select all axes other than
	// the specified axes to average over.
	void setAxes(const IPosition& axes, Bool invert=False);

	// Get pixel values, pixel mask values, etc.
	// The return Record has the following fields:
	// 'values' => Array<T> of pixel values
	// 'mask'   => Array<Bool> of pixel mask values
	Record get() const;

	static void put(
		SPIIT image, const Array<T>& pixelsArray, const Vector<Int>& blc,
		const Vector<Int>& inc, Bool list,
		Bool locking, Bool replicate
	);

	static Bool putRegion(
	    SPIIT image, const Array<T>& pixels,
	    const Array<Bool>& mask, Record& region, Bool list,
	    Bool usemask, Bool replicateArray
	);

	// get the aggregated values along the specified pixel axis using the region and mask at construction
	// and any other mask the image may have. Supported values of <src>function</src> are (case-insensitive,
	// minimum match) those supported by ImageCollapser, ie "flux", "max", "mean", "median", "min", "rms",
	// "sqrtsum", "sqrtsum_npix", sqrtsum_npix_beam", "stddev", "sum", "variance",  and "zero". Aggregation
	// of values occurs along all axes orthogonal to the one specified. One may specify the unit in which
	// coordinate values are calculated using the <src>unit</src> parameter. If unit starts with "pix", then
	// pixel coordinates are calculated, world coordinates otherwise. If pixel coordinates, the values are
	// relative to the zeroth pixel on the corresponding axis of the input image.  If specified and it
	// doesn't start with "pix", the unit must be conformant with
	// the unit of <src>axis</src> in the coordinate system of the image, or it must be a unit that this axis
	// can be readily converted to (eg km/s if the axis is a frequency axis with base unit of Hz).
	// If the selected axis is the spectral axis and if the unit is chosen to be something other
	// than the native spectral coordinate unit (such as velocity or wavelength for a native frequency unit),
	// <src>specType</src> indicates the system to use when converting the frequency. Values of RELATVISTIC,
	// RADIO_VELOCITY, and OPTICAL_VELOCITY are only permitted if <src>unit</src> represents a velocity unit.
	// Values of WAVELENGTH and AIR_WAVELENGTH are only permitted if <src>unit</src> represents a length unit.
	// For a velocity unit, DEFAULT is equivalent to RELATIVISTIC. For a length unit, DEFAULT is equivalent to
	// WAVELENGTH.
	// If the selected axis is the spectral axis and <src>unit</src> is a velocity unit,
	// <src>restFreq</src> represents the rest frequency with respect to which the velocity scale should be
	// calculated. If null, the rest frequency associated with the spectral coordinate is used.
	// If the selected axis is the spectral axis, and <src>unit</src> is a frequency unit,
	// <src>frame</src> represents the frame of reference with respect to which the frequency scale should be
	// calculated. If empty, the reference frame associated with the spectral coordinate is used.
	// The return Record has the following keys: "values" is a Vector<T> containing the aggregate pixel values,
	// "mask" is the associated mask values (Vector<Bool>), "coords" is a Vector<Double> of coordinate values,
	// and "xUnit" is a String containing the coordinate unit.
	Record getProfile(
		uInt axis, const String& function, const String& unit,
		PixelValueManipulatorData::SpectralType specType=PixelValueManipulatorData::DEFAULT,
		const Quantity *const restFreq=nullptr, const String& frame=""
	);

	Record getProfile(
		uInt axis, ImageCollapserData::AggregateType function, const String& unit,
		PixelValueManipulatorData::SpectralType specType=PixelValueManipulatorData::DEFAULT,
		const Quantity *const restFreq=nullptr, const String& frame=""
	);

	String getClass() const { return _className; }

	// region refers to the region in the image to be inserted, not the
	// region that was chosen at object construction
	static void insert(
		ImageInterface<T>& target, const ImageInterface<T>& image, const Record& region,
		const Vector<Double>& locatePixel, Bool verbose
	);

	// Make a block of regions from a Record
	// public so ImageAnalysis can use it, once those methods have been
	// excised, make private
	static void makeRegionBlock(
	    PtrBlock<const ImageRegion*>& regions,
	    const Record& Regions
	);

	Record pixelValue(const Vector<Int>& pixel) const;

	void pixelValue(
	    Bool& offImage, Quantum<T>& value, Bool& mask,
	    Vector<Int>& pos
	) const;

	// set specified pixels or mask equal to provided scalar value
	static Bool set(
	    SPIIF image, const String& pixels, const Int pixelmask,
	    Record& region, const Bool list = false
	);

	// set region name for logging purposes. Only used if the logfile is set.
	void setRegionName(const String& rname) { _regionName = rname; }

protected:
	CasacRegionManager::StokesControl _getStokesControl() const {
		return CasacRegionManager::USE_ALL_STOKES;
	}

	std::vector<Coordinate::Type> _getNecessaryCoordinates() const {
		return std::vector<Coordinate::Type>();
	}

	Bool _hasLogfileSupport() const { return True; }

    Bool _supportsMultipleRegions() const {return True;}

private:
    IPosition _axes, _lastChunkShape;
    String _regionName;
    const static String _className;

	void _checkUnit(
		const String& unit, const CoordinateSystem& csys,
		PixelValueManipulatorData::SpectralType specType
	) const;

	Record _doWorld(
		SPIIT collapsed, const String& unit,
		PixelValueManipulatorData::SpectralType specType,
		const Quantity *const restFreq,	const String& frame,
		uInt axis
	) const;

	void _doNoncomformantUnit(
		Vector<Double>& coords, const CoordinateSystem& csys,
		const String& unit, PixelValueManipulatorData::SpectralType specType,
		const Quantity *const restFreq, const String& axisUnit
	) const;




};

}

#ifndef AIPS_NO_TEMPLATE_SRC
#include <imageanalysis/ImageAnalysis/PixelValueManipulator.tcc>
#endif //# AIPS_NO_TEMPLATE_SRC

#endif
