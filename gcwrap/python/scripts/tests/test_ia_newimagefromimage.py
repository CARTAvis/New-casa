##########################################################################
# test_ia_newimagefromimage.py
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
# Test suite for the CASA tool method ia.newimagefromimage()
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
# Test for the ia.newimagefromimage() tool method
# </etymology>
#
# <synopsis>
# Test for the ia.newimagefromimage() tool method
# </synopsis> 
#
# <example>
#
# This test runs as part of the CASA python unit test suite and can be run from
# the command line via eg
# 
# `echo $CASAPATH/bin/casa | sed -e 's$ $/$'` --nologger --log2term -c `echo $CASAPATH | awk '{print $1}'`/code/xmlcasa/scripts/regressions/admin/runUnitTest.py test_ia_newimagefromimage[test1,test2,...]
#
# </example>
#
# <motivation>
# To provide a test standard for the ia.newimagefromimage() tool method to ensure
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

class ia_newimagefromimage_test(unittest.TestCase):
    
    def setUp(self):
        self._myia = iatool()
    
    def tearDown(self):
        self._myia.done()
    
    def test_history(self):
        """verify history writing"""
        myia = self._myia
        myia.fromshape("zz",[20, 20])
        myia = myia.newimagefromimage("zz")
        msgs = myia.history()
        myia.done()       
        self.assertTrue("ia.newimagefromimage" in msgs[-2])
        self.assertTrue("ia.newimagefromimage" in msgs[-1])

    def test_pixeltype(self):
        myia = self._myia
        name = "myim.im"
        etype = {
            'f': 'float', 'd': 'double', 'c': 'complex',
            'cd': 'dcomplex'
        }
        for t in ("f", "c", "d", "cd"):
            myia.fromshape(name, [20,20], type=t, overwrite=True)
            myia.done()
            myia = myia.newimagefromimage(name)
            self.assertEquals(
                myia.pixeltype(), etype[t], "data tpye check failed"
            )
            myia.done()

def suite():
    return [ia_newimagefromimage_test]
