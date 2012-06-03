/* ============================================================================
 * Copyright (c) 2011 Michael A. Jackson (BlueQuartz Software)
 * Copyright (c) 2011 Singanallur Venkatakrishnan (Purdue University)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * Neither the name of Singanallur Venkatakrishnan, Michael A. Jackson, the Pudue
 * Univeristy, BlueQuartz Software nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  This code was written under United States Air Force Contract number
 *                           FA8650-07-D-5800
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */



#ifndef COMPUTATIONENGINE_H_
#define COMPUTATIONENGINE_H_

#include <stdio.h>

#include "MXA/Common/MXASetGetMacros.h"

#include "TomoEngine/TomoEngine.h"
#include "TomoEngine/Common/AbstractFilter.h"
#include "TomoEngine/Common/Observer.h"
#include "TomoEngine/mt/mt19937ar.h"

#include "TomoEngine/SOC/SOCStructures.h"
#include "TomoEngine/Filters/CostData.h"
#include "TomoEngine/SOC/SOCConstants.h"


/**
 * @class SOCEngine SOCEngine.h TomoEngine/SOC/SOCEngine.h
 * @brief
 * @author Michael A. Jackson for BlueQuartz Software
 * @author Singanallur Venkatakrishnan (Purdue University)
 * @version 1.0
 */
class TomoEngine_EXPORT SOCEngine : public AbstractFilter
{

  public:
    MXA_SHARED_POINTERS(SOCEngine);
    MXA_TYPE_MACRO(SOCEngine);
    MXA_STATIC_NEW_MACRO(SOCEngine);

    MXA_INSTANCE_PROPERTY(TomoInputsPtr, TomoInputs)
    MXA_INSTANCE_PROPERTY(SinogramPtr, Sinogram)
    MXA_INSTANCE_PROPERTY(GeometryPtr, Geometry)
    MXA_INSTANCE_PROPERTY(ScaleOffsetParamsPtr, NuisanceParams)

    MXA_INSTANCE_PROPERTY(bool, UseBrightFieldData)
    MXA_INSTANCE_PROPERTY(TomoInputsPtr, BFTomoInputs)
    MXA_INSTANCE_PROPERTY(SinogramPtr, BFSinogram)

    static void InitializeTomoInputs(TomoInputsPtr);
    static void InitializeSinogram(SinogramPtr);
    static void InitializeGeometry(GeometryPtr);
    static void InitializeScaleOffsetParams(ScaleOffsetParamsPtr);

    virtual ~SOCEngine();

    enum VoxelUpdateType {
      RegularRandomOrderUpdate = 0,
      HomogeniousUpdate = 1,
      NonHomogeniousUpdate = 2
    };

    /**
     * @brief overload from super class
     * @return
     */
    void execute();

    Real_t absMaxArray(std::vector<Real_t> &Array);

  protected:
    // Protect this constructor because we want to force the use of the other
    SOCEngine();

    void initVariables();

    void calculateGeometricMeanConstraint(ScaleOffsetParams* NuisanceParams);
    void calculateArithmeticMean();

    /**
     * @brief This is to be implemented at some point
     */
    void updateVoxelValues_NHICD();

    uint8_t updateVoxels(int16_t OuterIter, int16_t Iter, VoxelUpdateType updateType, UInt8Image_t::Pointer VisitCount,
                      RNGVars* RandomNumber, AMatrixCol** TempCol,
                      RealVolumeType::Pointer ErrorSino, RealVolumeType::Pointer Weight,
                      AMatrixCol* VoxelLineResponse, ScaleOffsetParams* NuisanceParams,
                      UInt8Image_t::Pointer Mask, CostData::Pointer cost, uint16_t yStart,uint16_t yEnd);


    int readInputData();
    int initializeBrightFieldData();
    int createInitialGainsData();
    int createInitialOffsetsData();
    int createInitialVariancesData();
    int initializeRoughReconstructionData();
    void initializeROIMask(UInt8Image_t::Pointer Mask);
    void gainAndOffsetInitialization(ScaleOffsetParamsPtr NuisanceParams);
    void initializeHt(RealVolumeType::Pointer H_t);
    void storeVoxelResponse(RealVolumeType::Pointer H_t, AMatrixCol* VoxelLineResponse);
    void initializeVolume(RealVolumeType::Pointer Y_Est, double value);
    void calculateMeasurementWeight(RealVolumeType::Pointer Weight,
                                               ScaleOffsetParamsPtr NuisanceParams,
                                               RealVolumeType::Pointer ErrorSino,
                                               RealVolumeType::Pointer Y_Est);
    int jointEstimation(RealVolumeType::Pointer Weight,
                         ScaleOffsetParamsPtr NuisanceParams,
                         RealVolumeType::Pointer ErrorSino,
                         RealVolumeType::Pointer Y_Est,
                         CostData::Pointer cost);
    int calculateCost(CostData::Pointer cost,
                      RealVolumeType::Pointer Weight,
                      RealVolumeType::Pointer ErrorSino);
    void updateWeights(RealVolumeType::Pointer Weight,
                       ScaleOffsetParamsPtr NuisanceParams,
                       RealVolumeType::Pointer ErrorSino);
    void writeNuisanceParameters(ScaleOffsetParamsPtr NuisanceParams);

    void writeSinogramFile(ScaleOffsetParamsPtr NuisanceParams, RealVolumeType::Pointer Final_Sinogram);
    void writeReconstructionFile();
    void writeVtkFile();
    void writeMRCFile();


  #ifdef QGGMRF
    Real_t CE_FunctionalSubstitution(Real_t umin,Real_t umax);
    void CE_ComputeQGGMRFParameters(Real_t umin,Real_t umax,Real_t RefValue);
    Real_t CE_QGGMRF_Value(Real_t delta);
    Real_t CE_QGGMRF_Derivative(Real_t delta);
    Real_t CE_QGGMRF_SecondDerivative(Real_t delta);
  #endif //QGGMRF


  private:
    //if 1 then this is NOT outside the support region; If 0 then that pixel should not be considered
    uint8_t BOUNDARYFLAG[3][3][3];
    //Markov Random Field Prior parameters - Globals DATA_TYPE
    Real_t FILTER[3][3][3];
    Real_t HAMMING_WINDOW[5][5];
    Real_t THETA1;
    Real_t THETA2;
    Real_t NEIGHBORHOOD[3][3][3];
    Real_t V;
    Real_t MRF_P;
    Real_t SIGMA_X_P;
#ifdef QGGMRF
    //QGGMRF extras
    Real_t MRF_Q,MRF_C;
    Real_t QGGMRF_Params[26][3];
    Real_t MRF_ALPHA;
	Real_t SIGMA_X_P_Q;
	Real_t SIGMA_X_Q;
#endif //QGGMRF
    //used to store cosine and sine of all angles through which sample is tilted
    RealArrayType::Pointer cosine;
    RealArrayType::Pointer sine;
    RealArrayType::Pointer BeamProfile; //used to store the shape of the e-beam
    Real_t BEAM_WIDTH;
    Real_t OffsetR;
    Real_t OffsetT;

    RealImage_t::Pointer QuadraticParameters; //holds the coefficients of N_theta quadratic equations. This will be initialized inside the MAPICDREconstruct function

    RealImage_t::Pointer MagUpdateMap;//Hold the magnitude of the reconstuction along each voxel line
    RealImage_t::Pointer FiltMagUpdateMap;//Filters the above to compute threshold
    UInt8Image_t::Pointer MagUpdateMask;//Masks only the voxels of interest

    RealImage_t::Pointer Qk_cost;
    RealImage_t::Pointer bk_cost;
    RealArrayType::Pointer ck_cost; //these are the terms of the quadratic cost function
    RealArrayType::Pointer d1;
    RealArrayType::Pointer d2; //hold the intermediate values needed to compute optimal mu_k
    uint16_t NumOfViews; //this is kind of redundant but in order to avoid repeatedly send this info to the rooting function we save number of views
    Real_t LogGain; //again these information  are available but to prevent repeatedly sending it to the rooting functions we store it in a variable

    uint64_t startm;
    uint64_t stopm;

    /**
     * @brief
     * @param DetectorResponse
     * @param H_t
     * @return
     */
    RealVolumeType::Pointer forwardProject(RealVolumeType::Pointer DetectorResponse, RealVolumeType::Pointer H_t);

    /**
     * @brief
     */
    void calculateSinCos();

    /**
     * @brief
     */
    void initializeBeamProfile();

    /**
     * @brief
     * @param low
     * @param high
     */
    void minMax(Real_t *low, Real_t *high);

    /**
     * @brief
     */
    RealImage_t::Pointer calculateVoxelProfile();

    /**
     * @brief
     * @param row
     * @param col
     * @param slice
     * @param VoxelProfile
     */
   // void* calculateAMatrixColumn(uint16_t row, uint16_t col, uint16_t slice, DATA_TYPE** VoxelProfile);

    /**
     * @brief
     * @param ErrorSino
     * @param Weight
     * @return
     */
    Real_t computeCost(RealVolumeType::Pointer ErrorSino, RealVolumeType::Pointer Weight);

    /**
     * @brief
     * @param row
     * @param col
     * @param slice
     * @param DetectorResponse
     */
    void* calculateAMatrixColumnPartial(uint16_t row,uint16_t col, uint16_t slice, RealVolumeType::Pointer DetectorResponse);

    /**
     * @brief
     * @return
     */
    double surrogateFunctionBasedMin();

	//Updates a single line of voxels along y-axis
	void UpdateVoxelLine(uint16_t j_new,uint16_t k_new);


	/**
     * Code to take the magnitude map and filter it with a hamming window
     * Returns the filtered magnitude map
     */
    void ComputeVSC();

	//Sort the entries of FiltMagUpdateMap and set the threshold to be ? percentile
	Real_t SetNonHomThreshold();


    template<typename T>
    double solve(T* f, double a, double b, double err, int32_t *code,uint32_t iteration_count)

    {

      int signa, signb, signc;
      double fa, fb, fc, c, signaling_nan();
      double dist;
	  uint32_t num_iter=0;


      fa = f->execute(a);
      signa = fa > 0;
      fb = f->execute(b);
      signb = fb > 0;

      /* check starting conditions */
      if(signa == signb)
      {
        if(signa == 1) *code = 1;
        else *code = -1;
        return (0.0);
      }
      else *code = 0;

      /* half interval search */
      if((dist = b - a) < 0) dist = -dist;

      while (num_iter < iteration_count)//(dist > err)
      {
		num_iter++;
        c = (b + a) / 2;
        fc = f->execute(c);
        signc = fc > 0;
        if(signa == signc)
        {
          a = c;
          fa = fc;
        }
        else
        {
          b = c;
          fb = fc;
        }
        if((dist = b - a) < 0) dist = -dist;
      }

      /* linear interpolation */
      if((fb - fa) == 0) return (a);
      else
      {
        c = (a * fb - b * fa) / (fb - fa);
        return (c);
      }
    }

    SOCEngine(const SOCEngine&); // Copy Constructor Not Implemented
    void operator=(const SOCEngine&); // Operator '=' Not Implemented
};

#endif //CompEngine
