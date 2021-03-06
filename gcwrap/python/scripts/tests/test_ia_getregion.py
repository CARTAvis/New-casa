##########################################################################
# imfit_test.py
#
# Copyright (C) 2008, 2009
# Associated Universities, Inc. Washington DC, USA.
#
# This script is free software; you can redistribute it and/or modify it
# under the terms of the GNU Library General Public License as published by
# the Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This library is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
# License for more details.
#
# You should have received a copy of the GNU Library General Public License
# along with this library; if not, write to the Free Software Foundation,
# Inc., 675 Massachusetts Ave, Cambridge, MA 02139, USA.
#
# Correspondence concerning AIPS++ should be adressed as follows:
#        Internet email: aips2-request@nrao.edu.
#        Postal address: AIPS++ Project Office
#                        National Radio Astronomy Observatory
#                        520 Edgemont Road
#                        Charlottesville, VA 22903-2475 USA
#
# <author>
# Dave Mehringer
# </author>
#
# <summary>
# Test suite for the CASA tool method ia.getregion()
# </summary>
#
# <reviewed reviwer="" date="" tests="" demos="">
# </reviewed
#
# <prerequisite>
# <ul>
# </ul>
# </prerequisite>
#
# <etymology>
# Test for the ia.getregion() tool method
# </etymology>
#
# <synopsis>
# Test the ia.getregion() tool method
# </synopsis> 
#
# <example>
#
# This test runs as part of the CASA python unit test suite and can be run from
# the command line via eg
# 
# `echo $CASAPATH/bin/casa | sed -e 's$ $/$'` --nologger --log2term -c `echo $CASAPATH | awk '{print $1}'`/code/xmlcasa/scripts/regressions/admin/runUnitTest.py test_ia_getregion[test1,test2,...]
#
# </example>
#
# <motivation>
# To provide a test standard for the ia.getregion() tool method to ensure
# coding changes do not break the associated bits 
# </motivation>
#

###########################################################################
import shutil
import casac
from tasks import *
from taskinit import *
from __main__ import *
import unittest
import numpy

class ia_getregion_test(unittest.TestCase):
    
    def setUp(self):
        pass
    
    def tearDown(self):
        pass
    
    def test_stretch(self):
        """ ia.getregion(): Test stretch parameter"""
        yy = iatool()
        mymask = "maskim"
        yy.fromshape(mymask, [200, 200, 1, 1])
        yy.addnoise()
        yy.done()
        shape = [200,200,1,20]
        yy.fromshape("", shape)
        yy.addnoise()
        self.assertRaises(
            Exception,
            yy.getregion, mask=mymask + ">0", stretch=False
        )
        zz = yy.getregion(
            mask=mymask + ">0", stretch=True
        )
        self.assertTrue(type(zz) == type(yy.getchunk()))
        yy.done()
        
    def test_precision(self):
        """Test support for images with pixel values of various precisions"""
        myia = iatool()
        jj = 1.2345678901234567890123456789
        for mytype in ('f', 'd'):
            myia.fromshape("", [4,4], type=mytype)
            gg = myia.getchunk()
            gg[:] = jj
            myia.putchunk(gg)
            reg = myia.getregion()
            if mytype == 'f':
                self.assertTrue(numpy.isclose(reg, jj, 1e-8, 1e-8).all())
                self.assertFalse(numpy.isclose(reg, jj, 1e-9, 1e-9).all())
            else:
                self.assertTrue((reg == jj).all())
        jj = jj*(1+1j)
        for mytype in ('c', 'cd'):
            myia.fromshape("", [4,4], type=mytype)
            gg = myia.getchunk()
            gg[:] = jj
            myia.putchunk(gg)
            reg = myia.getregion()
            if mytype == 'c':
                self.assertTrue(numpy.isclose(reg, jj, 1e-8, 1e-8).all())
                self.assertFalse(numpy.isclose(reg, jj, 1e-9, 1e-9).all())
            else:
                self.assertTrue((reg == jj).all())
        
def suite():
    return [ia_getregion_test]
