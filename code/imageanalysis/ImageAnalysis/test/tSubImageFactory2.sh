#!/bin/sh
`which casa` --nologger --log2term  -c `echo $CASAPATH | awk '{print $1}'`/gcwrap/python/scripts/regressions/admin/runUnitTest.py test_ia_subimage

