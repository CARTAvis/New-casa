//# tGJones_GT.cc: Tests the GJones viscal
//# Copyright (C) 1995,1999,2000,2001,2016
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

#include <casa/aips.h>
//#include <casa/Exceptions/Error.h>
#include <casa/iostream.h>
#include <msvis/MSVis/VisBuffer2.h>
#include <msvis/MSVis/SimpleSimVi2.h>
//#include <synthesis/MeasurementComponents/SolveDataBuffer.h>
#include <casa/Arrays/ArrayMath.h>
#include <casa/OS/Timer.h>
#include <synthesis/MeasurementComponents/StandardVisCal.h>
#include <synthesis/MeasurementComponents/DJones.h>
#include <synthesis/MeasurementComponents/KJones.h>
#include <synthesis/MeasurementComponents/MSMetaInfoForCal.h>

#include <gtest/gtest.h>

#define SHOWSTATE false
using namespace casacore;
using namespace casa;
using namespace casa::vi;

class VisCalTest : public ::testing::Test {

public:
  
  VisCalTest() :
    nFld(1),
    nScan(1),
    nSpw(1),
    nAnt(5),
    nCorr(4),
    nChan(1,32),
    ss(nFld,nScan,nSpw,nAnt,nCorr,Vector<Int>(1,1),nChan),
    msmc(ss)
  {}

  Int nFld,nScan,nSpw,nAnt,nCorr;
  Vector<Int> nChan;
  SimpleSimVi2Parameters ss;
  MSMetaInfoForCal msmc;

};

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}




TEST_F(VisCalTest, GJonesApplyState) {
  
  VisCal *G = new GJones(msmc);
  G->setApply();

  G->setMeta(0,0,0.0,
	     0,ss.freqs(0),
	     0);
  G->sizeApplyParCurrSpw(ss.nChan_(0));
  G->setDefApplyParCurrSpw(true,true);  // sync, w/ doInv=T

  if (SHOWSTATE)
    G->state();

  ASSERT_EQ(VisCalEnum::JONES,G->matrixType());
  ASSERT_EQ(VisCal::G,G->type());
  ASSERT_EQ(String("G Jones"),G->typeName());
  ASSERT_EQ(2,G->nPar());
  ASSERT_FALSE(G->freqDepPar());
  ASSERT_FALSE(G->freqDepMat());
  ASSERT_FALSE(G->freqDepCalWt());
  ASSERT_FALSE(G->timeDepMat());
  ASSERT_TRUE(G->isApplied());
  ASSERT_TRUE(G->isSolvable());

  /*
  IPosition sh(3,2,1,nAnt);  // nChan=1 for G
  ASSERT_TRUE(G->currCPar().shape()==sh);
  ASSERT_TRUE(G->currParOK().shape()==sh);
  ASSERT_TRUE(G->currJElem().shape()==sh);
  ASSERT_TRUE(G->currJElemOK().shape()==sh);
  ASSERT_EQ(G->currParOK().data(),G->currJElemOK().data()); // ok addr equal
  */

  delete G;
}


TEST_F(VisCalTest, GJonesSolveState) {

  //  MSMetaInfoForCal msmc(ss);
  SolvableVisCal *G = new GJones(msmc);

  Record solvePar;
  String caltablename("test.G"); solvePar.define("table",caltablename);
  String solint("int");          solvePar.define("solint",solint);
  Vector<Int> refantlist(1,3);   solvePar.define("refant",refantlist);

  G->setSolve(solvePar);

  G->setMeta(0,0,0.0,
	     0,ss.freqs(0),
	     0);
  G->sizeSolveParCurrSpw(ss.nChan_(0));
  G->setDefSolveParCurrSpw(true);

  if (SHOWSTATE)
    G->state();

  ASSERT_EQ(VisCalEnum::JONES,G->matrixType());
  ASSERT_EQ(VisCal::G,G->type());
  ASSERT_EQ(String("G Jones"),G->typeName());
  ASSERT_EQ(2,G->nPar());
  ASSERT_FALSE(G->freqDepPar());
  ASSERT_FALSE(G->freqDepMat());
  ASSERT_FALSE(G->freqDepCalWt());
  ASSERT_FALSE(G->timeDepMat());
  ASSERT_FALSE(G->isApplied());
  ASSERT_TRUE(G->isSolved());
  ASSERT_TRUE(G->isSolvable());
  
  ASSERT_EQ(caltablename,G->calTableName());
  ASSERT_EQ(solint,G->solint());
  ASSERT_EQ(refantlist[0],G->refant());
  ASSERT_TRUE(allEQ(refantlist,G->refantlist()));
  

  delete G;
}



TEST_F(VisCalTest, GJonesSpecifyTest) {
  
  SolvableVisJones *G = new TJones(msmc);

  String caltablename("GJonesSpecifyTest.T");
  Record spec;
  spec.define("caltable",caltablename);

  cout << "spec=" << spec << endl;

  G->setSpecify(spec);

  cout << "Hello" << endl;


  if (SHOWSTATE)
    G->state();

  ASSERT_EQ(VisCalEnum::JONES,G->matrixType());
  ASSERT_EQ(VisCal::T,G->type());
  ASSERT_EQ(String("T Jones"),G->typeName());
  ASSERT_EQ(1,G->nPar());
  ASSERT_FALSE(G->freqDepPar());
  ASSERT_FALSE(G->freqDepMat());
  ASSERT_FALSE(G->freqDepCalWt());
  ASSERT_FALSE(G->timeDepMat());
  ASSERT_FALSE(G->isApplied());
  ASSERT_FALSE(G->isSolved());
  ASSERT_TRUE(G->isSolvable());

  cout << "G->solveAllCPar().shape() = " << G->solveAllCPar().shape() << endl;
  cout << "G->solveAllCPar() = " << G->solveAllCPar() << endl;
  cout << "G->solveParOK() = " << G->solveParOK() << endl;

  Cube<Complex> g(G->solveAllCPar());
  //for (Int iant=0;iant<ss.nAnt_;++iant) {
  //  g(Slice(),Slice(),Slice(iant,1,1))=Complex(cos(iant),sin(iant));
  //}

  Int refant(3);
  for (Int itime=0;itime<35;++itime) {
    Double t(itime);
    for (Int iant=1;iant<ss.nAnt_;++iant) {
      Float ph(itime*iant*C::pi/1800.0);
      Complex dg(cos(ph),sin(ph));
      g(Slice(),Slice(),Slice(iant,1,1))=g(Slice(),Slice(),Slice(iant,1,1))*dg;
    }

    if (itime>24 && itime<30) 
      G->solveParOK()(0,0,refant)=false;
    else
      G->solveParOK()(0,0,refant)=true;
      

    G->setMeta(0,0,t,0,ss.freqs(0),0);
    G->keepNCT();
  }

  G->refantmode()="strict";
  G->refantlist().assign(Vector<Int>(1,refant));
  G->applyRefAnt();

  G->storeNCT();

  delete G;
}
