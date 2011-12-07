/*
 *  ComputationEngineOptimized.c
 *  HAADFSTEM_Xcode
 *
 *  Created by Singanallur Venkatakrishnan on 6/29/11.
 *  Copyright 2011 Purdue University. All rights reserved.
 *
 */

#include "SOCEngine.h"

// C Includes
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

// C++ Includes
#include <limits>
#include <iostream>

// MXA includes
#include "MXA/Utilities/MXADir.h"
#include "MXA/Utilities/MXAFileInfo.h"

// Our own includes
#include "TomoEngine/Common/EIMMath.h"
#include "TomoEngine/Common/allocate.h"
#include "TomoEngine/Common/EIMTime.h"
#include "TomoEngine/Common/TomoFilter.h"
#include "TomoEngine/Common/MRCSinogramInitializer.h"
#include "TomoEngine/Common/RawSinogramInitializer.h"
#include "TomoEngine/Common/GainsOffsetsReader.h"
#include "TomoEngine/Common/ComputeGainsOffsets.h"
#include "TomoEngine/Common/InitialReconstructionInitializer.h"
#include "TomoEngine/Common/InitialReconstructionBinReader.h"
#include "TomoEngine/IO/RawGeometryWriter.h"
#include "TomoEngine/mt/mt19937ar.h"
#include "TomoEngine/IO/MRCHeader.h"
#include "TomoEngine/IO/MRCReader.h"
#include "TomoEngine/SOC/SOCConstants.h"
#include "TomoEngine/Common/ComputeGainsOffsets.h"
#include "TomoEngine/IO/RawGeometryWriter.h"


#define START startm = EIMTOMO_getMilliSeconds();
#define STOP stopm = EIMTOMO_getMilliSeconds();
#define PRINTTIME printf( "%6.3f seconds used by the processor.\n", ((double)stopm-startm)/1000.0);



inline double Clip(double x, double a, double b)
{
	return (x < a) ? a : ((x > b) ? b:x);
}

inline int16_t mod(int16_t a,int16_t b)
{
	int16_t temp;
	temp=a%b;
	if(temp < 0)
		return temp + b;
	else {
		return temp;
	}

}

inline double Minimum(double a, double b)
{
	return (a < b ? a: b);
}

/**
 * @class CE_ConstraintEquation CE_ConstraintEquation.h EIMTomo/ScaleOffsetCorrectionEngine.h
 * @brief
 * @author
 * @date Nov 1, 2011
 * @version 1.0
 */
class CE_ConstraintEquation
{
  public:
    CE_ConstraintEquation(uint16_t NumOfViews,
                          DATA_TYPE** QuadraticParameters,
                          DATA_TYPE* d1, DATA_TYPE* d2,
                          DATA_TYPE** Qk_cost,
                          DATA_TYPE** bk_cost,
                          DATA_TYPE* ck_cost,
                          DATA_TYPE LogGain) :
        NumOfViews(NumOfViews),
        QuadraticParameters(QuadraticParameters),
        d1(d1),
        d2(d2),
        Qk_cost(Qk_cost),
        bk_cost(bk_cost),
        ck_cost(ck_cost),
        LogGain(LogGain)
    {
    }//constructor which assigns values to the private members of the class ; The members of the class and the parameters have the same name here

    virtual ~CE_ConstraintEquation()
    {
    }

    /**
     *
     * @param a
     * @param b
     * @param c
     * @return
     */
    DATA_TYPE* CE_RootsOfQuadraticFunction(DATA_TYPE a, DATA_TYPE b, DATA_TYPE c)
    {
      DATA_TYPE* temp = (DATA_TYPE*)get_spc(2, sizeof(double));
      DATA_TYPE value = 0, discriminant;
      temp[0] = -1;
      temp[1] = -1;
      discriminant = b * b - 4 * a * c;
      if(discriminant < 0)
      {
        printf("Quadratic has no real roots\n");
        return temp;
      }
      else
      {
        value = sqrt(discriminant);
        temp[0] = (-b + value) / (2 * a);
        temp[1] = (-b - value) / (2 * a);
      }
      return temp;

    }

    /**
     *
     * @param lamda
     * @return
     */
    double execute(DATA_TYPE lambda)
    {
      double sum = 0, temp_cost = 0, min = (double)INT64_MAX;
      double value = 0;
      double* root;
      double temp_mu;
      uint8_t i, min_index = 0;
      uint16_t i_theta;

      for (i_theta = 0; i_theta < NumOfViews; i_theta++)
      {
        //temp=((QuadraticParameters[i_theta][1]*QuadraticParameters[i_theta][1])-4*QuadraticParameters[i_theta][0]*lambda);
        root = CE_RootsOfQuadraticFunction(QuadraticParameters[i_theta][0], QuadraticParameters[i_theta][1], lambda);
        //Evaluate which root results in a lower cost function
        for (i = 0; i < 2; i++)
        {
          if(root[i] > 0) // If the value of I0[k] is positive
          {
            temp_mu = d1[i_theta] - root[i] * d2[i_theta]; //for a given lambda we can calculate I0(\lambda) and hence mu(lambda)
            temp_cost = (Qk_cost[i_theta][0] * root[i] * root[i] + 2 * Qk_cost[i_theta][1] * root[i] * temp_mu + temp_mu * temp_mu * Qk_cost[i_theta][2]
                - 2 * (bk_cost[i_theta][0] * root[i] + temp_mu * bk_cost[i_theta][1]) + ck_cost[i_theta]); //evaluating the cost function
            if(temp_cost < min)
            {
              min = temp_cost;
              min_index = i;

            }
          }
        }

        if(root[min_index] > 0)
			sum += log(root[min_index]); //max{(-b+sqrt(b^2 - 4*a*c))/2*a,(-b+sqrt(b^2 - 4*a*c))/2*a}
        else
        {
          printf("Log of a negative number\n");
			printf("View %d\n",i_theta);
			printf("Roots of the quadratic are %lf %lf \n",root[0],root[1]);
        }
        free(root);

      }
      value = sum - LogGain;
      return value;
    }

  protected:
    CE_ConstraintEquation() {}

  private:
    uint16_t NumOfViews;
    DATA_TYPE** QuadraticParameters;
    DATA_TYPE* d1;
    DATA_TYPE* d2; //hold the intermediate values needed to compute optimal mu_k
    DATA_TYPE** Qk_cost;
    DATA_TYPE** bk_cost;
    DATA_TYPE* ck_cost; //these are the terms of the quadratic cost function
    DATA_TYPE LogGain;
    CE_ConstraintEquation(const CE_ConstraintEquation&); // Copy Constructor Not Implemented
    void operator=(const CE_ConstraintEquation&); // Operator '=' Not Implemented
};


/**
 * @brief
 * @author
 * @version
 */
class DerivOfCostFunc
{

  public:
    DerivOfCostFunc(uint8_t _BOUNDARYFLAG[3][3][3],
                    DATA_TYPE _NEIGHBORHOOD[3][3][3],
                    DATA_TYPE _FILTER[3][3][3],
                    DATA_TYPE _V,
                    DATA_TYPE _THETA1,
                    DATA_TYPE _THETA2,
                    DATA_TYPE _SIGMA_X_P,
                    DATA_TYPE _MRF_P) :

                      V(_V),
                      THETA1(_THETA1),
                      THETA2(_THETA2),
                      SIGMA_X_P(_SIGMA_X_P),
                      MRF_P(_MRF_P)
    {

      for(size_t i = 0; i < 3; ++i)
      {
        for(size_t j = 0; j < 3; ++j)
        {
          for(size_t k = 0; k < 3; ++k)
          {
            BOUNDARYFLAG[i][j][k] = _BOUNDARYFLAG[i][j][k];
            NEIGHBORHOOD[i][j][k] = _NEIGHBORHOOD[i][j][k];
            FILTER[i][j][k] = _FILTER[i][j][k];
          }
        }
      }
    }

    virtual ~DerivOfCostFunc()
    {
    }

    double execute(DATA_TYPE u) const
    {
      double temp = 0;
      double value = 0;
      uint8_t i, j, k;
      for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
          for (k = 0; k < 3; k++)
          {
            if(BOUNDARYFLAG[i][j][k] == 1)
            {
              if(u - NEIGHBORHOOD[i][j][k] >= 0.0)
				  temp +=((double)FILTER[i][j][k] * (1.0) * pow(fabs(u - (double)NEIGHBORHOOD[i][j][k]), (double)(MRF_P - 1)));
              else temp += ((double)FILTER[i][j][k] * (-1.0) * pow(fabs(u - (double)NEIGHBORHOOD[i][j][k]), (double)(MRF_P - 1)));
            }
          }

      //printf("V While updating %lf\n",V);
      //scanf("Enter value %d\n",&k);
      value = THETA1 + THETA2 * (u - V) + (temp / SIGMA_X_P);

      return value;
    }

  protected:
    DerivOfCostFunc() {}

  private:
    uint8_t BOUNDARYFLAG[3][3][3];
    DATA_TYPE NEIGHBORHOOD[3][3][3];
    DATA_TYPE FILTER[3][3][3];
    DATA_TYPE V;
    DATA_TYPE THETA1;
    DATA_TYPE THETA2;
    DATA_TYPE SIGMA_X_P;
    DATA_TYPE MRF_P;

    DerivOfCostFunc(const DerivOfCostFunc&); // Copy Constructor Not Implemented
    void operator=(const DerivOfCostFunc&); // Operator '=' Not Implemented
};


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SOCEngine::SOCEngine() :
    m_Inputs(NULL),
    m_Sinogram(NULL),
    m_Geometry(NULL)
{
  initVariables();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SOCEngine::~SOCEngine()
{
//  if (m_Sinogram != NULL)
//  {
//    delete m_Sinogram;
//  }
//  if(m_Geometry != NULL)
//  {
//    delete m_Geometry;
//  }

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::initVariables()
{


  m_Inputs = NULL;
  m_Sinogram = NULL;
  m_Geometry = NULL;

  FILTER[0][0][0] = 0.0302; FILTER[0][0][1] = 0.0370; FILTER[0][0][2] = 0.0302;
  FILTER[0][1][0] = 0.0370; FILTER[0][1][1] = 0.0523; FILTER[0][1][2] = 0.0370;
  FILTER[0][2][0] = 0.0302; FILTER[0][2][1] = 0.0370; FILTER[0][2][2] = 0.0302;

  FILTER[1][0][0] = 0.0370; FILTER[1][0][1] = 0.0523; FILTER[1][0][2] = 0.0370;
  FILTER[1][1][0] = 0.0523; FILTER[1][1][1] = 0.0000; FILTER[1][1][2] = 0.0523;
  FILTER[1][2][0] = 0.0370; FILTER[1][2][1] = 0.0523; FILTER[1][2][2] = 0.0370;

  FILTER[2][0][0] = 0.0302; FILTER[2][0][1] = 0.0370; FILTER[2][0][2] = 0.0302;
  FILTER[2][1][0] = 0.0370; FILTER[2][1][1] = 0.0523; FILTER[2][1][2] = 0.0370;
  FILTER[2][2][0] = 0.0302; FILTER[2][2][1] = 0.0370; FILTER[2][2][2] = 0.0302;
}

// void CE_cancel()
// {
// CE_Cancel = 1;
// }
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------

#define MAKE_OUTPUT_FILE(Fp, err, outdir, filename)\
    {\
    std::string filepath(outdir);\
    filepath = filepath.append(MXADir::getSeparator()).append(filename);\
    errno = 0;\
    err = 0;\
    Fp = fopen(filepath.c_str(),"wb");\
    if (Fp == NULL) { std::cout << "Error " << errno << " Opening Output file " << filepath << std::endl; err = -1; }\
    }


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::execute()
{

	uint8_t err = 0;
	int16_t Iter,OuterIter;
	int16_t i,j,k,r,Idx;
//	int16_t RowIndex,ColIndex,SliceIndex,row,col,slice;
//	int16_t NumOfXPixels;
//	int16_t q,p;
//	int16_t tempindex_x,tempindex_y,tempindex_z;
	int16_t index_delta_t;
//	int16_t index_min,index_max,index_delta_r;
	//Random Indexing Parameters
	int32_t Index,ArraySize,j_new,k_new;
//	int32_t temp_index;
	int32_t* Counter;
	uint8_t **VisitCount;

	uint16_t cost_counter=0;
    uint16_t VoxelLineAccessCounter;
	uint16_t MaxNumberOfDetectorElts;

	DATA_TYPE center_t,delta_t;
//	DATA_TYPE center_r,delta_r;
	DATA_TYPE w3,w4;
	DATA_TYPE checksum = 0,temp;
	DATA_TYPE sum1,sum2;
	DATA_TYPE* cost;
	DATA_TYPE** VoxelProfile;
	DATA_TYPE*** DetectorResponse;
	DATA_TYPE*** H_t;
	DATA_TYPE ProfileCenterT;

#ifdef ROI
	//variables used to stop the process
	DATA_TYPE AverageUpdate;
	DATA_TYPE AverageMagnitudeOfRecon;
#endif

	DATA_TYPE ***Y_Est;//Estimated Sinogram
	DATA_TYPE ***Final_Sinogram;//To store and write the final sinogram resulting from our reconstruction
	DATA_TYPE ***ErrorSino;//Error Sinogram
	DATA_TYPE ***Weight;//This contains weights for each measurement = The diagonal covariance matrix in the Cost Func formulation
	RNGVars* RandomNumber;

	//File variables
	FILE* Fp1 = NULL;
	FILE* Fp2 = NULL;
	FILE* Fp4 = NULL;
	FILE* Fp5 = NULL;
//	FILE* Fp6 = NULL;
	FILE* Fp7 = NULL;
//	FILE* Fp8 = NULL;

	int fileError = 0;

	// Initialize the Sinogram
	if (m_Inputs == NULL)
	{
	  setErrorCondition(-1);
	  updateProgressAndMessage("Error: The TomoInput Structure was NULL", 100);
	  return;
	}
  //Based on the inputs , calculate the "other" variables in the structure definition
	if (m_Sinogram == NULL)
	{
	  m_Sinogram = new Sinogram;
	}
	if (m_Geometry == NULL)
	{
	  m_Geometry = new Geometry;
	}



	// Read the Input data from the supplied data file
	// We are scoping here so the various readers are automatically cleaned up before
	// the code goes any farther
	{
	  TomoFilter::Pointer initializer = TomoFilter::NullPointer();
    std::string extension = MXAFileInfo::extension(m_Inputs->SinoFile);
    if (extension.compare("mrc") == 0 || extension.compare("ali") == 0)
    {
      initializer = MRCSinogramInitializer::NewTomoFilter();
    }
    else if (extension.compare("bin") == 0 )
    {
      initializer = RawSinogramInitializer::NewTomoFilter();
    }
    else
    {
      setErrorCondition(-1);
      updateProgressAndMessage("A supported file reader for the input file was not found." , 100);
      return;
    }
    initializer->setInputs(m_Inputs);
    initializer->setSinogram(m_Sinogram);
    initializer->addObserver(this);
    initializer->execute();
    if (initializer->getErrorCondition() < 0)
    {
      updateProgressAndMessage("Error reading Input Sinogram Data file", 100);
      setErrorCondition(initializer->getErrorCondition());
      return;
    }
	}


  // Now read or generate the Gains and Offsets data. We are scoping this section
  // so the reader automactically gets cleaned up at this point.
  {
    TomoFilter::Pointer initializer = TomoFilter::NullPointer();
    if(m_Inputs->GainsOffsetsFile.empty() == true)
    {
      // Calculate the initial Gains and Offsets
      initializer = ComputeGainsOffsets::NewTomoFilter();
    }
    else
    {
      initializer = GainsOffsetsReader::NewTomoFilter();
    }

    initializer->setSinogram(m_Sinogram);
    initializer->setInputs(m_Inputs);
    initializer->addObserver(this);
    initializer->execute();
 //   std::cout << "hi" << std::endl;

    if(initializer->getErrorCondition() < 0)
    {
     updateProgressAndMessage("Error initializing Input Gains and Offsets Data file", 100);
     setErrorCondition(initializer->getErrorCondition());
     return;
    }
  }

  // Initialize the Geometry data from a rough reconstruction
  {
    InitialReconstructionInitializer::Pointer initializer = InitialReconstructionInitializer::NullPointer();
    std::string extension = MXAFileInfo::extension(m_Inputs->InitialReconFile);
    if (m_Inputs->InitialReconFile.empty() == true)
    {
      // This will just initialize all the values to Zero (0)
      initializer = InitialReconstructionInitializer::New();
    }
    else if (extension.compare("bin") == 0 )
    {
      // This will read the values from a binary file
      initializer = InitialReconstructionBinReader::NewInitialReconstructionInitializer();
    }
    initializer->setSinogram(m_Sinogram);
    initializer->setInputs(m_Inputs);
    initializer->setGeometry(m_Geometry);
    initializer->addObserver(this);
    initializer->execute();

    if(initializer->getErrorCondition() < 0)
    {
      updateProgressAndMessage("Error reading Initial Reconstruction Data from File", 100);
      setErrorCondition(initializer->getErrorCondition());
      return;
    }
  }

	MAKE_OUTPUT_FILE(Fp1, fileError, m_Inputs->outputDir, ScaleOffsetCorrection::ReconstructedSinogramFile);
	if (fileError == 1)
	{

	}

	MAKE_OUTPUT_FILE(Fp4, fileError, m_Inputs->outputDir, ScaleOffsetCorrection::FinalGainParametersFile);
	if (fileError == 1)
	{

	}

  MAKE_OUTPUT_FILE(Fp5, fileError, m_Inputs->outputDir, ScaleOffsetCorrection::FinalOffsetParametersFile);
  if (fileError == 1)
  {

  }

#ifdef FORWARD_PROJECT_MODE
  MAKE_OUTPUT_FILE(Fp6, fileError, m_Inputs->outputDir, ScaleOffsetCorrection::ForwardProjectedObjectFile);
  if (fileError == 1)
  {

  }
#endif


#ifdef DEBUG_CONSTRAINT_OPT
	FILE *Fp8 = fopen("CostFunctionCoefficients.bin","w");
  MAKE_OUTPUT_FILE(Fp8, fileError, m_Inputs->outputDir, ScaleOffsetCorrection::CostFunctionCoefficientsFile);
  if (fileError == 1)
  {

  }
#endif
//	DATA_TYPE buffer;
	//Optimization variables
	DATA_TYPE low,high;
//	DATA_TYPE dist;
	DATA_TYPE UpdatedVoxelValue;
//	DATA_TYPE SurrogateUpdate;
	DATA_TYPE accuracy =1e-7;//This is the rooting accuracy for x
//	DATA_TYPE LambdaRootingAccuracy=1e-10;//accuracy for rooting Lambda
//	DATA_TYPE perturbation=1e-30;//perturbs the rooting range
	int32_t errorcode=-1;

	//Pointer to  1-D minimization Function

	//Scale and Offset Parameter Structures
	ScaleOffsetParams NuisanceParams;
	NuisanceParams.I_0 = NULL;
	NuisanceParams.mu = NULL;
	NuisanceParams.alpha = NULL;

	DATA_TYPE numerator_sum =0;
	DATA_TYPE denominator_sum = 0;
	DATA_TYPE x,z;
	DATA_TYPE **MicroscopeImage; //This is used to store the projection of the object for each view
//	DATA_TYPE rmin,rmax; //min and max indices of a projection on the detector along the r-direction
//	DATA_TYPE w1,w2,f1,f2;
//    DATA_TYPE k1,k2,k3;//Quadratic equation coefficients
//	DATA_TYPE product;
	DATA_TYPE sum;
	DATA_TYPE a,b,c,d,e;
//	DATA_TYPE determinant;
	DATA_TYPE LagrangeMultiplier;


	MicroscopeImage = (DATA_TYPE**)get_img(m_Sinogram->N_t, m_Sinogram->N_r, sizeof(DATA_TYPE));

	NuisanceParams.I_0 = (DATA_TYPE*)get_spc(m_Sinogram->N_theta, sizeof(DATA_TYPE));
	NuisanceParams.mu = (DATA_TYPE*)get_spc(m_Sinogram->N_theta, sizeof(DATA_TYPE));
#ifdef NOISE_MODEL
	NuisanceParams.alpha=(DATA_TYPE*)get_spc(m_Sinogram->N_theta, sizeof(DATA_TYPE));
#endif

  // initialize variables
  Idx = 0;


#ifdef WRITE_INTERMEDIATE_RESULTS
	DATA_TYPE Fraction = 0.1;//write this fraction of the iterations
	int16_t NumOfWrites = floor((DATA_TYPE)(m_Inputs->NumIter)*Fraction);
	int16_t WriteCount = 0;
	char Filename[100];
	char buffer[20];
	void* TempPointer;
	size_t NumOfBytesWritten;
#endif

#ifdef ROI
	uint8_t** Mask;
	DATA_TYPE EllipseA,EllipseB;
#endif

#ifdef COST_CALCULATE
	cost=(DATA_TYPE*)get_spc((m_Inputs->NumIter+1)*m_Inputs->NumOuterIter*3,sizeof(DATA_TYPE));//the factor 3 is in there to ensure we can store 3 costs per iteration one after update x, then mu and then I
#endif

	Y_Est=(DATA_TYPE ***)get_3D(m_Sinogram->N_theta,m_Sinogram->N_r,m_Sinogram->N_t,sizeof(DATA_TYPE));
	ErrorSino=(DATA_TYPE ***)get_3D(m_Sinogram->N_theta,m_Sinogram->N_r,m_Sinogram->N_t,sizeof(DATA_TYPE));
	Weight=(DATA_TYPE ***)get_3D(m_Sinogram->N_theta,m_Sinogram->N_r,m_Sinogram->N_t,sizeof(DATA_TYPE));


	//Setting the value of all the global variables

	OffsetR = ((m_Inputs->delta_xz/sqrt(3.0)) + m_Sinogram->delta_r/2)/DETECTOR_RESPONSE_BINS;
	OffsetT = ((m_Inputs->delta_xz/2) + m_Sinogram->delta_t/2)/DETECTOR_RESPONSE_BINS;

#ifdef BEAM_CALCULATION
	BEAM_WIDTH = (0.5)*m_Sinogram->delta_r;
#else
	BEAM_WIDTH = m_Sinogram->delta_r;
#endif

#ifndef QGGMRF
	MRF_P = m_Inputs->p;
	SIGMA_X_P = pow(m_Inputs->SigmaX,MRF_P);
#else
	MRF_P=2;
	MRF_Q=1.2;
	MRF_C=30;
	MRF_ALPHA=1.5;
	SIGMA_X_P = pow(m_Inputs->SigmaX,MRF_P);
	for(i=0;i < 3;i++)
		for(j=0; j < 3;j++)
			for(k=0; k < 3;k++)
				FILTER[i][j][k]*=(1.0/SIGMA_X_P);
#endif //QGGMRF

	//globals assosiated with finding the optimal gain and offset parameters
	QuadraticParameters = (DATA_TYPE**)(get_img(3, m_Sinogram->N_theta, sizeof(DATA_TYPE)));//Hold the coefficients of a quadratic equation
	Qk_cost=(DATA_TYPE**)get_img(3, m_Sinogram->N_theta, sizeof(DATA_TYPE));
	bk_cost=(DATA_TYPE**)get_img(2, m_Sinogram->N_theta, sizeof(DATA_TYPE));
	ck_cost=(DATA_TYPE*)get_spc(m_Sinogram->N_theta, sizeof(DATA_TYPE));
	NumOfViews = m_Sinogram->N_theta;
	LogGain = m_Sinogram->N_theta*log(m_Sinogram->TargetGain);
	d1=(DATA_TYPE*)get_spc(m_Sinogram->N_theta, sizeof(DATA_TYPE));
	d2=(DATA_TYPE*)get_spc(m_Sinogram->N_theta, sizeof(DATA_TYPE));





	//calculate the trapezoidal voxel profile for each angle.Also the angles in the Sinogram Structure are converted to radians
	VoxelProfile = (DATA_TYPE**)calculateVoxelProfile(); //Verified with ML
	//Pre compute sine and cos theta to speed up computations
	calculateSinCos();
	//Initialize the e-beam
	initializeBeamProfile(); //verified with ML

	//calculate sine and cosine of all angles and store in the global arrays sine and cosine
	DetectorResponse = (DATA_TYPE***)detectorResponse(0,0,VoxelProfile);//System response
	if (NULL == DetectorResponse)
	{
	  std::cout << "Error Calling function detectorResponse in file " << __FILE__ << "(" << __LINE__ << ")" << std::endl;
	  setErrorCondition(-2);
	  return;
	}


	H_t = (DATA_TYPE***)get_3D(1, m_Sinogram->N_theta, DETECTOR_RESPONSE_BINS, sizeof(DATA_TYPE));//detector response along t

#ifdef RANDOM_ORDER_UPDATES
	VisitCount=(uint8_t **)get_img(m_Geometry->N_x, m_Geometry->N_z,sizeof(uint8_t));//width,height
	for(i=0;i<m_Geometry->N_z;i++)
		for(j=0;j < m_Geometry->N_x;j++)
			VisitCount[i][j]=0;
#endif//Random update

	#ifdef ROI
	Mask = (uint8_t **)get_img(m_Geometry->N_x, m_Geometry->N_z,sizeof(uint8_t));//width,height
	EllipseA = m_Geometry->LengthX/2;
	EllipseB = m_Inputs->LengthZ/2;
	for (i = 0; i < m_Geometry->N_z ; i++)
	{
		for (j=0; j< m_Geometry->N_x; j++)
		{
			x = m_Geometry->x0 + ((DATA_TYPE)j + 0.5)*m_Inputs->delta_xz;
			z = m_Geometry->z0 + ((DATA_TYPE)i + 0.5)*m_Inputs->delta_xz;
			if (x >= -(m_Sinogram->N_r*m_Sinogram->delta_r)/2 && x <= (m_Sinogram->N_r*m_Sinogram->delta_r)/2 && z>= -m_Inputs->LengthZ/2 && z <= m_Inputs->LengthZ/2)
			{
				Mask[i][j] = 1;
			}
			else
			{
				Mask[i][j] =0;
			}

		}
	}


#endif
	m_Sinogram->TargetGain=20000;

#ifdef BRIGHT_FIELD //Take log of the data and subtract log(Dosage) from it

	for (i_theta = 0;i_theta < m_Sinogram->N_theta; i_theta++)//slice index
		for(i_r = 0; i_r < m_Sinogram->N_r;i_r++)
			for(i_t = 0;i_t < m_Sinogram->N_t;i_t++)
			{
				m_Sinogram->counts[i_theta][i_r][i_t] = log(m_Sinogram->counts[i_theta][i_r][i_t])-log(m_Sinogram->TargetGain);
			}
#endif//Bright Field

	//Scale and Offset Parameters Initialization

	sum = 0;
	temp = 0;
	for(k=0 ; k < m_Sinogram->N_theta;k++)
	{
		//fread(&buffer, 1, sizeof(double), Fp6);
		NuisanceParams.I_0[k]= m_Sinogram->InitialGain[k];//;
#ifdef GEOMETRIC_MEAN_CONSTRAINT
		sum+=log(NuisanceParams.I_0[k]);
#else
		sum+=NuisanceParams.I_0[k];
#endif
	}
	sum/=m_Sinogram->N_theta;
#ifdef GEOMETRIC_MEAN_CONSTRAINT
	sum=exp(sum);
	printf("The geometric mean of the gains is %lf\n",sum);

	//Checking if the input parameters satisfy the target geometric mean
	if(fabs(sum - m_Sinogram->TargetGain) > 1e-5)
	{
		printf("The input paramters dont meet the constraint..Renormalizing\n");
        temp = exp(log(m_Sinogram->TargetGain) - log(sum));
		for (k = 0 ; k < m_Sinogram->N_theta; k++) {
			NuisanceParams.I_0[k]=m_Sinogram->InitialGain[k]*temp;
		}
	}
#else
	printf("The Arithmetic mean of the constraint is %lf\n",sum);
	if(sum - m_Sinogram->TargetGain > 1e-5)
	{
		printf("Arithmetic Mean Constraint not met..renormalizing\n");
		temp = m_Sinogram->TargetGain/sum;
		for (k = 0 ; k < m_Sinogram->N_theta; k++) {
			NuisanceParams.I_0[k]=m_Sinogram->InitialGain[k]*temp;
		}
	}
#endif


	for(k=0 ; k < m_Sinogram->N_theta;k++)
	{
		//fread(&buffer, 1, sizeof(double), Fp6);
		NuisanceParams.mu[k] = m_Sinogram->InitialOffset[k];
	}

	for(k = 0 ; k <m_Sinogram->N_theta; k++)
	{
		for (i = 0; i < DETECTOR_RESPONSE_BINS; i++)
		{
			ProfileCenterT = i*OffsetT;
			if(m_Inputs->delta_xy >= m_Sinogram->delta_t)
			{
				if(ProfileCenterT <= ((m_Inputs->delta_xy/2) - (m_Sinogram->delta_t/2)))
					H_t[0][k][i] = m_Sinogram->delta_t;
				else {
					H_t[0][k][i] = -1*ProfileCenterT + (m_Inputs->delta_xy/2) + m_Sinogram->delta_t/2;
				}
				if(H_t[0][k][i] < 0)
					{H_t[0][k][i] =0;}

			}
			else {
				if(ProfileCenterT <= m_Sinogram->delta_t/2 - m_Inputs->delta_xy/2)
					{H_t[0][k][i] = m_Inputs->delta_xy;}
				else {
					H_t[0][k][i] = -ProfileCenterT + (m_Inputs->delta_xy/2) + m_Sinogram->delta_t/2;
				}

				if(H_t[0][k][i] < 0) {
					H_t[0][k][i] =0;
				}

			}
		}
	}

	/*for(i=0;i<Sinogram->N_theta;i++)
		for(j=0;j< PROFILE_RESOLUTION;j++)
			checksum+=VoxelProfile[i][j];
    printf("CHK SUM%lf\n",checksum);
*/
    checksum=0;

	//Allocate space for storing columns the A-matrix; an array of pointers to columns
	//AMatrixCol** AMatrix=(AMatrixCol **)get_spc(Geometry->N_x*Geometry->N_z,sizeof(AMatrixCol*));
//	DetectorResponse = CE_DetectorResponse(0,0,Sinogram,Geometry,VoxelProfile);//System response

#ifdef STORE_A_MATRIX

	AMatrixCol**** AMatrix = (AMatrixCol ****)multialloc(sizeof(AMatrixCol*),3,m_Geometry->N_y,m_Geometry->N_z,m_Geometry->N_x);
#else
	DATA_TYPE y;
	DATA_TYPE t,tmin,tmax,ProfileThickness;
	int16_t slice_index_min,slice_index_max;
	AMatrixCol*** TempCol = (AMatrixCol***)multialloc(sizeof(AMatrixCol*),2,m_Geometry->N_z,m_Geometry->N_x);//stores 2-D forward projector
//	AMatrixCol* Aj;
	AMatrixCol* TempMemBlock;
	//T-direction response
	AMatrixCol* VoxelLineResponse=(AMatrixCol*)get_spc(m_Geometry->N_y, sizeof(AMatrixCol));
	MaxNumberOfDetectorElts = (uint16_t)((m_Inputs->delta_xy/m_Sinogram->delta_t)+2);
    for (i=0; i < m_Geometry->N_y; i++) {
		VoxelLineResponse[i].count=0;
		VoxelLineResponse[i].values=(DATA_TYPE*)get_spc(MaxNumberOfDetectorElts, sizeof(DATA_TYPE));
		VoxelLineResponse[i].index=(uint32_t*)get_spc(MaxNumberOfDetectorElts, sizeof(uint32_t));
	}
#endif

	//Calculating A-Matrix one column at a time
	//For each entry the idea is to initially allocate space for Sinogram.N_theta * Sinogram.N_x
	// And then store only the non zero entries by allocating a new array of the desired size
	//k=0;

	checksum = 0;
	//q = 0;

#ifdef STORE_A_MATRIX
	for(i = 0;i < m_Geometry->N_y; i++)
		for(j = 0;j < m_Geometry->N_z; j++)
			for (k = 0; k < m_Geometry->N_x; k++)
			{
				//  AMatrix[q++]=CE_CalculateAMatrixColumn(i,j,Sinogram,Geometry,VoxelProfile);
				AMatrix[i][j][k]=calculateAMatrixColumn(j,k,i,m_Sinogram,m_Geometry,VoxelProfile);//row,col,slice

				for(p = 0; p < AMatrix[i][j][k]->count; p++)
					checksum += AMatrix[i][j][k]->values[p];
				//   printf("(%d,%d,%d) %lf \n",i,j,k,AMatrix[i][j][k]->values);
				checksum = 0;
			}
	printf("Stored A matrix\n");
#else
	temp=0;
    for(j=0; j < m_Geometry->N_z; j++)
    for(k=0; k < m_Geometry->N_x; k++)
    {
      TempCol[j][k] = (AMatrixCol*)calculateAMatrixColumnPartial(j, k, 0, DetectorResponse);
      temp += TempCol[j][k]->count;
    }
#endif

	//Storing the response along t-direction for each voxel line

	for (i =0; i < m_Geometry->N_y; i++)
	{
		y = ((DATA_TYPE)i+0.5)*m_Inputs->delta_xy + m_Geometry->y0;
		t = y;
		tmin = (t - m_Inputs->delta_xy/2) > m_Sinogram->T0 ? t-m_Inputs->delta_xy/2 : m_Sinogram->T0;
		tmax = (t + m_Inputs->delta_xy/2) <= m_Sinogram->TMax? t + m_Inputs->delta_xy/2 : m_Sinogram->TMax;

		slice_index_min = floor((tmin - m_Sinogram->T0)/m_Sinogram->delta_t);
		slice_index_max = floor((tmax - m_Sinogram->T0)/m_Sinogram->delta_t);

		if(slice_index_min < 0)
			{slice_index_min = 0;}
		if(slice_index_max >= m_Sinogram->N_t)
			{slice_index_max = m_Sinogram->N_t-1;}


		//printf("%d %d\n",slice_index_min,slice_index_max);

		for(int i_t = slice_index_min ; i_t <= slice_index_max; i_t++)
		{
			center_t = ((DATA_TYPE)i_t + 0.5)*m_Sinogram->delta_t + m_Sinogram->T0;
			delta_t = fabs(center_t - t);
			index_delta_t = floor(delta_t/OffsetT);
			if(index_delta_t < DETECTOR_RESPONSE_BINS)
			{
				w3 = delta_t - (DATA_TYPE)(index_delta_t)*OffsetT;
				w4 = ((DATA_TYPE)index_delta_t+1)*OffsetT - delta_t;
				ProfileThickness =(w4/OffsetT)*H_t[0][0][index_delta_t] + (w3/OffsetT)*H_t[0][0][index_delta_t+1 < DETECTOR_RESPONSE_BINS ? index_delta_t+1:DETECTOR_RESPONSE_BINS-1];
			}
			else
			{
				ProfileThickness=0;
			}

			if(ProfileThickness != 0)//Store the response of this slice
			{
				printf("%d %lf\n",i_t,ProfileThickness);
				VoxelLineResponse[i].values[VoxelLineResponse[i].count]=ProfileThickness;
				VoxelLineResponse[i].index[VoxelLineResponse[i].count++]=i_t;
			}

		}

	}


	printf("Number of non zero entries of the forward projector is %lf\n",temp);
	printf("Geometry-Z %d\n",m_Geometry->N_z);


	//Forward Project Geometry->Object one slice at a time and compute the  Sinogram for each slice
	//is Y_Est initailized to zero?
	for(i = 0; i < m_Sinogram->N_theta; i++)
	 for(j = 0; j < m_Sinogram->N_r; j++)
	 for(k = 0;k < m_Sinogram->N_t; k++)
	 Y_Est[i][j][k]=0.0;


	RandomNumber=init_genrand(1ul);
//	srand(time(NULL));
	ArraySize= m_Geometry->N_z*m_Geometry->N_x;
	//ArraySizeK = Geometry->N_x;

	Counter = (int32_t*)malloc(ArraySize*sizeof(int32_t));
	//CounterK = (int*)malloc(ArraySizeK*sizeof(int));

	for(j_new = 0;j_new < ArraySize; j_new++)
		Counter[j_new]=j_new;


	START;
//Forward Projection

		for (j = 0; j < m_Geometry->N_z; j++)
  {
    for (k = 0; k < m_Geometry->N_x; k++)
    {

      if(TempCol[j][k]->count > 0)
      {
        for (i = 0; i < m_Geometry->N_y; i++) //slice index
        {
          /*  y = ((DATA_TYPE)i+0.5)*Geometry->delta_xy + Geometry->y0;
           t = y;
           tmin = (t - Geometry->delta_xy/2) > Sinogram->T0 ? t-Geometry->delta_xy/2 : Sinogram->T0;
           tmax = (t + Geometry->delta_xy/2) <= Sinogram->TMax? t + Geometry->delta_xy/2 : Sinogram->TMax;

           slice_index_min = floor((tmin - Sinogram->T0)/Sinogram->delta_t);
           slice_index_max = floor((tmax - Sinogram->T0)/Sinogram->delta_t);

           if(slice_index_min < 0)
           slice_index_min = 0;
           if(slice_index_max >= Sinogram->N_t)
           slice_index_max = Sinogram->N_t-1;
           */

          for (uint32_t q = 0; q < TempCol[j][k]->count; q++)
          {
            //calculating the footprint of the voxel in the t-direction

            int16_t i_theta = int16_t(floor(static_cast<float>(TempCol[j][k]->index[q] / (m_Sinogram->N_r))));
            int16_t i_r = (TempCol[j][k]->index[q] % (m_Sinogram->N_r));

            VoxelLineAccessCounter = 0;
            for (uint32_t i_t = VoxelLineResponse[i].index[0]; i_t < VoxelLineResponse[i].index[0] + VoxelLineResponse[i].count; i_t++) //CHANGED from <= to <
            {
              /*center_t = ((DATA_TYPE)i_t + 0.5)*Sinogram->delta_t + Sinogram->T0;
               delta_t = fabs(center_t - t);
               index_delta_t = floor(delta_t/OffsetT);
               if(index_delta_t < DETECTOR_RESPONSE_BINS)
               {
               w3 = delta_t - (DATA_TYPE)(index_delta_t)*OffsetT;
               w4 = ((DATA_TYPE)index_delta_t+1)*OffsetT - delta_t;
               ProfileThickness =(w4/OffsetT)*H_t[0][i_theta][index_delta_t] + (w3/OffsetT)*H_t[0][i_theta][index_delta_t+1 < DETECTOR_RESPONSE_BINS ? index_delta_t+1:DETECTOR_RESPONSE_BINS-1];
               }
               else
               {
               ProfileThickness=0;
               }*/
              Y_Est[i_theta][i_r][i_t] += (NuisanceParams.I_0[i_theta]
                  * (TempCol[j][k]->values[q] * VoxelLineResponse[i].values[VoxelLineAccessCounter++] * m_Geometry->Object[j][k][i]));

            }
          }

        }
      }

    }
  }
	STOP;
	PRINTTIME;

	/*
	START;

	Y_Est=ForwardProject(Sinogram,Geometry,DetectorResponse,H_t);
	STOP;
	PRINTTIME;*/

	//Calculate Error Sinogram - Can this be combined with previous loop?
	//Also compute weights of the diagonal covariance matrix

#ifdef NOISE_MODEL
	for (int16_t i_theta = 0;i_theta < m_Sinogram->N_theta; i_theta++)//slice index
	{
		NuisanceParams.alpha[i_theta]=1;//Initialize the refinement parameters to 1
		for(int16_t i_r = 0; i_r < m_Sinogram->N_r;i_r++)
			for(int16_t i_t = 0;i_t < m_Sinogram->N_t;i_t++)
				if(sum != 0)
				{
#ifdef BRIGHT_FIELD
					Weight[i_theta][i_r][i_t] = sum;
#else
					Weight[i_theta][i_r][i_t]=1.0/(m_Sinogram->counts[i_theta][i_r][i_t]*NuisanceParams.alpha[i_theta]);//The variance for each view ~=averagecounts in that view
#endif
				}
	}
#endif//Noise model


	for (int16_t i_theta = 0;i_theta < m_Sinogram->N_theta; i_theta++)//slice index
	{
    checksum = 0;
    for (int16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++)
    {
      for (uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
      {

        // checksum+=Y_Est[i][j][k];
        ErrorSino[i_theta][i_r][i_t] = m_Sinogram->counts[i_theta][i_r][i_t] - Y_Est[i_theta][i_r][i_t] - NuisanceParams.mu[i_theta];
        //	if(fabs(ErrorSino[i_theta][i_r][i_t]) > 20000)
        //		printf("%d %d %d %lf %lf %lf\n",i_theta,i_r,i_t,ErrorSino[i_theta][i_r][i_t],Weight[i_theta][i_r][i_t],Y_Est[i_theta][i_r][i_t]);

#ifndef NOISE_MODEL
        if(m_Sinogram->counts[i_theta][i_r][i_t] != 0)
        {
#ifdef BRIGHT_FIELD
          Weight[i_theta][i_r][i_t] = m_Sinogram->counts[i_theta][i_r][i_t];
#else
          Weight[i_theta][i_r][i_t] = 1.0 / m_Sinogram->counts[i_theta][i_r][i_t];
#endif //bright field check
        }
        else Weight[i_theta][i_r][i_t] = 0;
#endif//Noise model
//#ifdef DEBUG
        //writing the error sinogram
        //	if(i_t == 0)
        //	fwrite(&ErrorSino[i_theta][i_r][i_t],sizeof(DATA_TYPE),1,Fp1);
//#endif
#ifdef FORWARD_PROJECT_MODE
        temp=Y_Est[i_theta][i_r][i_t]/NuisanceParams.I_0[i_theta];
        fwrite(&temp,sizeof(DATA_TYPE),1,Fp6);
#endif
        checksum += Weight[i_theta][i_r][i_t];
      }
    }
		printf("Check sum of Diagonal Covariance Matrix= %lf\n", checksum);

  }


#ifdef FORWARD_PROJECT_MODE
	return 0;//exit the program once we finish forward projecting the object
#endif

#ifdef COST_CALCULATE
	fileError = 0;
  MAKE_OUTPUT_FILE(Fp2, fileError, m_Inputs->outputDir, ScaleOffsetCorrection::CostFunctionFile);
  if (fileError == 1)
  {

  }

	printf("Cost Function Calculation \n");
	/*********************Initial Cost Calculation***************************************************/

	cost[cost_counter] = computeCost(ErrorSino,Weight);

	printf("%lf\n",cost[cost_counter]);

	fwrite(&cost[cost_counter],sizeof(DATA_TYPE),1,Fp2);
	cost_counter++;
	/*******************************************************************************/
#endif //Cost calculation endif


	//Loop through every voxel updating it by solving a cost function

	for(OuterIter = 0; OuterIter < m_Inputs->NumOuterIter; OuterIter++)
	{
    for (Iter = 0; Iter < m_Inputs->NumIter; Iter++)
	{  
#ifdef NHICD
		if(0 == Iter%2) //During the even iterations just update all voxels
		{

      //printf("Iter %d\n",Iter);
#ifdef ROI
      AverageUpdate = 0;
      AverageMagnitudeOfRecon = 0;
#endif

#ifdef RANDOM_ORDER_UPDATES
      ArraySize = m_Geometry->N_x * m_Geometry->N_z;
      for (j_new = 0; j_new < ArraySize; j_new++)
        Counter[j_new] = j_new;

      for (j = 0; j < m_Geometry->N_z; j++)
        for (k = 0; k < m_Geometry->N_x; k++)
          VisitCount[j][k] = 0;
#endif

      START;
      for (j = 0; j < m_Geometry->N_z; j++) //Row index
        for (k = 0; k < m_Geometry->N_x; k++) //Column index
        {
#ifdef RANDOM_ORDER_UPDATES
          //RandomNumber=init_genrand(Iter);
          Index = (genrand_int31(RandomNumber)) % ArraySize;
          k_new = Counter[Index] % m_Geometry->N_x;
          j_new = Counter[Index] / m_Geometry->N_x;
          //memmove(Counter+Index,Counter+Index+1,sizeof(int32_t)*(ArraySize - Index-1));
          //TODO: Instead just swap the value in Index with the one in ArraySize
          Counter[Index] = Counter[ArraySize - 1];
          VisitCount[j_new][k_new] = 1;
          ArraySize--;
#else
          j_new=j;
          k_new=k;
#endif //Random order updates
          TempMemBlock = TempCol[j_new][k_new]; //Remove this
          if(TempMemBlock->count > 0)
          {
            for (i = 0; i < m_Geometry->N_y; i++) //slice index
            {
              //Neighborhood of (i,j,k) should be initialized to zeros each time
              for (int32_t p = 0; p <= 2; p++)
              {
                for (int32_t q = 0; q <= 2; q++)
                {
                  for (r = 0; r <= 2; r++)
                  {
                    NEIGHBORHOOD[p][q][r] = 0.0;
                    BOUNDARYFLAG[p][q][r] = 0;
                  }
                }
              }
#ifndef CIRCULAR_BOUNDARY_CONDITION

              //For a given (i,j,k) store its 26 point neighborhood
              for (int32_t p = -1; p <= 1; p++)
              {
                for (int32_t q = -1; q <= 1; q++)
                {
                  for (r = -1; r <= 1; r++)
                  {
                    if(i + p >= 0 && i + p < m_Geometry->N_y) if(j_new + q >= 0 && j_new + q < m_Geometry->N_z) if(k_new + r >= 0
                        && k_new + r < m_Geometry->N_x)
                    {
                      NEIGHBORHOOD[p + 1][q + 1][r + 1] = m_Geometry->Object[q + j_new][r + k_new][p + i];
                      BOUNDARYFLAG[p + 1][q + 1][r + 1] = 1;
                    }
                    else
                    {
                      BOUNDARYFLAG[p + 1][q + 1][r + 1] = 0;
                    }

                  }
                }
              }
#else
              for(p = -1; p <=1; p++)
              for(q = -1; q <= 1; q++)
              for(r = -1; r <= 1;r++)
              {
                tempindex_x = mod(r+k_new,m_Geometry->N_x);
                tempindex_y =mod(p+i,m_Geometry->N_y);
                tempindex_z = mod(q+j_new,m_Geometry->N_z);
                NEIGHBORHOOD[p+1][q+1][r+1] = m_Geometry->Object[tempindex_z][tempindex_x][tempindex_y];
                BOUNDARYFLAG[p+1][q+1][r+1]=1;
              }

#endif//circular boundary condition check
              NEIGHBORHOOD[1][1][1] = 0.0;

#ifdef DEBUG
              if(i == 0 && j == 31 && k == 31)
              {
                printf("***************************\n");
                printf("Geom %lf\n", m_Geometry->Object[i][31][31]);
                for (int p = 0; p <= 2; p++)
                {
                  for (int q = 0; q <= 2; q++)
                  {
                    for (r = 0; r <= 2; r++)
                    {
                      printf("%lf\n", NEIGHBORHOOD[p][q][r]);
                    }
                  }
                }
              }
#endif
              //Compute theta1 and theta2

              V = m_Geometry->Object[j_new][k_new][i]; //Store the present value of the voxel
              THETA1 = 0.0;
              THETA2 = 0.0;

              /*		y = ((DATA_TYPE)i+0.5)*Geometry->delta_xy + Geometry->y0;
               t = y;
               tmin = (t - Geometry->delta_xy/2) > Sinogram->T0 ? t-Geometry->delta_xy/2 : Sinogram->T0;
               tmax = (t + Geometry->delta_xy/2) <= Sinogram->TMax? t + Geometry->delta_xy/2 : Sinogram->TMax;

               slice_index_min = floor((tmin - Sinogram->T0)/Sinogram->delta_t);
               slice_index_max = floor((tmax - Sinogram->T0)/Sinogram->delta_t);

               if(slice_index_min < 0)
               slice_index_min = 0;
               if(slice_index_max >= Sinogram->N_t)
               slice_index_max = Sinogram->N_t-1;*/

              //TempCol = CE_CalculateAMatrixColumn(j, k, i, Sinogram, Geometry, VoxelProfile);
              for (uint32_t q = 0; q < TempMemBlock->count; q++)
              {

                uint16_t i_theta = floor(static_cast<float>(TempMemBlock->index[q] / (m_Sinogram->N_r)));
                uint16_t i_r = (TempMemBlock->index[q] % (m_Sinogram->N_r));
                VoxelLineAccessCounter = 0;
                for (uint32_t i_t = VoxelLineResponse[i].index[0]; i_t < VoxelLineResponse[i].index[0] + VoxelLineResponse[i].count; i_t++)
                // for(i_t = slice_index_min ; i_t <= slice_index_max; i_t++)
                {
                  /* center_t = ((DATA_TYPE)i_t + 0.5)*Sinogram->delta_t + Sinogram->T0;
                   delta_t = fabs(center_t - t);
                   index_delta_t = floor(delta_t/OffsetT);

                   if(index_delta_t < DETECTOR_RESPONSE_BINS)
                   {
                   w3 = delta_t - index_delta_t*OffsetT;
                   w4 = (index_delta_t+1)*OffsetT - delta_t;
                   //TODO: interpolation
                   ProfileThickness =(w4/OffsetT)*H_t[0][i_theta][index_delta_t] + (w3/OffsetT)*H_t[0][i_theta][index_delta_t+1 < DETECTOR_RESPONSE_BINS ? index_delta_t+1:DETECTOR_RESPONSE_BINS-1];
                   }
                   else
                   {
                   ProfileThickness=0;
                   }*/

                  THETA2 += ((NuisanceParams.I_0[i_theta] * NuisanceParams.I_0[i_theta])
                      * (VoxelLineResponse[i].values[VoxelLineAccessCounter] * VoxelLineResponse[i].values[VoxelLineAccessCounter]) * (TempMemBlock->values[q])
                      * (TempMemBlock->values[q]) * Weight[i_theta][i_r][i_t]);
                  THETA1 += NuisanceParams.I_0[i_theta] * ErrorSino[i_theta][i_r][i_t] * (TempMemBlock->values[q])
                      * (VoxelLineResponse[i].values[VoxelLineAccessCounter]) * Weight[i_theta][i_r][i_t];
                  VoxelLineAccessCounter++;
                }
              }
              THETA1 *= -1;
              minMax(&low, &high);

#ifdef DEBUG
              if(i == 0 && j == 31 && k == 31) printf("(%lf,%lf,%lf) \n", low, high, V - (THETA1 / THETA2));
#endif

              //Solve the 1-D optimization problem
              //printf("V before updating %lf",V);
#ifndef SURROGATE_FUNCTION
              //TODO : What if theta1 = 0 ? Then this will give error
              DerivOfCostFunc docf(BOUNDARYFLAG, NEIGHBORHOOD, FILTER, V, THETA1, THETA2, SIGMA_X_P, MRF_P);

              UpdatedVoxelValue = (DATA_TYPE)solve<DerivOfCostFunc>(&docf, (double)low, (double)high, (double)accuracy, &errorcode);

#else

              errorcode=0;
#ifdef QGGMRF
              UpdatedVoxelValue = CE_FunctionalSubstitution(low,high);

#else
              SurrogateUpdate=surrogateFunctionBasedMin();
              UpdatedVoxelValue=SurrogateUpdate;
#endif //QGGMRF

#endif//Surrogate function
              //printf("%lf\n",SurrogateUpdate);

              if(errorcode == 0)
              {
                //    printf("(%lf,%lf,%lf)\n",low,high,UpdatedVoxelValue);
                //	printf("Updated %lf\n",UpdatedVoxelValue);
#ifdef POSITIVITY_CONSTRAINT
                if(UpdatedVoxelValue < 0.0) //Enforcing positivity constraints
                UpdatedVoxelValue = 0.0;
#endif
              }
              else
              {
                if(THETA1 == 0 && low == 0 && high == 0) UpdatedVoxelValue = 0;
                else
                {
                  printf("Error \n");
                  printf("%d %d\n", j_new, k_new);
                }
              }

              //TODO Print appropriate error messages for other values of error code
              m_Geometry->Object[j_new][k_new][i] = UpdatedVoxelValue;

#ifdef ROI
              if(Mask[j_new][k_new] == 1)
              {

                AverageUpdate += fabs(m_Geometry->Object[j_new][k_new][i] - V);
                AverageMagnitudeOfRecon += fabs(V);//computing the percentage update =(Change in mag/Initial magnitude)
              }
#endif

              //Update the ErrorSinogram

              for (uint32_t q = 0; q < TempMemBlock->count; q++)
              {

                uint16_t i_theta = floor(static_cast<float>(TempMemBlock->index[q] / (m_Sinogram->N_r)));
                uint16_t i_r = (TempMemBlock->index[q] % (m_Sinogram->N_r));
                VoxelLineAccessCounter = 0;
                for (uint32_t i_t = VoxelLineResponse[i].index[0]; i_t < VoxelLineResponse[i].index[0] + VoxelLineResponse[i].count; i_t++)
                //for(i_t = slice_index_min ; i_t <= slice_index_max; i_t++)
                {
                  /*	center_t = ((DATA_TYPE)i_t + 0.5)*Sinogram->delta_t + Sinogram->T0;
                   delta_t = fabs(center_t - t);
                   index_delta_t = floor(delta_t/OffsetT);

                   if(index_delta_t < DETECTOR_RESPONSE_BINS)
                   {
                   w3 = delta_t - index_delta_t*OffsetT;
                   w4 = (index_delta_t+1)*OffsetT - delta_t;
                   //TODO: interpolation
                   ProfileThickness =(w4/OffsetT)*H_t[0][i_theta][index_delta_t] + (w3/OffsetT)*H_t[0][i_theta][index_delta_t+1 < DETECTOR_RESPONSE_BINS ? index_delta_t+1:DETECTOR_RESPONSE_BINS-1];
                   }
                   else
                   {
                   ProfileThickness=0;
                   }*/

                  ErrorSino[i_theta][i_r][i_t] -= (NuisanceParams.I_0[i_theta]
                      * (TempMemBlock->values[q] * VoxelLineResponse[i].values[VoxelLineAccessCounter] * (m_Geometry->Object[j_new][k_new][i] - V)));
                  VoxelLineAccessCounter++;
                }
              }
              Idx++;
            }

          }
          else
          {
            continue;
          }

        }
      STOP;
      PRINTTIME;

#ifdef RANDOM_ORDER_UPDATES
      for (j = 0; j < m_Geometry->N_z; j++) //Row index
        for (k = 0; k < m_Geometry->N_x; k++) //Column index
          if(VisitCount[j][k] == 0) printf("Pixel (%d %d) not visited\n", j, k);
#endif

#ifdef COST_CALCULATE
      /*********************Cost Calculation***************************************************/

      cost[cost_counter] = computeCost(ErrorSino, Weight);
      printf("%lf\n", cost[cost_counter]);

      if(cost[cost_counter] - cost[cost_counter - 1] > 0)
      {
        printf("Cost just increased!\n");
        break;
      }

      fwrite(&cost[cost_counter], sizeof(DATA_TYPE), 1, Fp2);
      cost_counter++;
      /*******************************************************************************/
#else
      printf("%d\n",Iter);
#endif //Cost calculation endif

#ifdef ROI
      if(AverageMagnitudeOfRecon > 0)
      {
        printf("%d,%lf\n", Iter + 1, AverageUpdate / AverageMagnitudeOfRecon);
        if((AverageUpdate / AverageMagnitudeOfRecon) < m_Inputs->StopThreshold)
        {
          printf("This is the terminating point %d\n", Iter);
			m_Inputs->StopThreshold*=THRESHOLD_REDUCTION_FACTOR;//Reducing the thresold for subsequent iterations
			break;
        }
      }
#endif//ROI end
      if(getCancel() == true)
      {
        setErrorCondition(err);
        return;
      }

#ifdef WRITE_INTERMEDIATE_RESULTS

      if(Iter == NumOfWrites*WriteCount)
      {
        WriteCount++;
        sprintf(buffer,"%d",Iter);
        sprintf(Filename,"ReconstructedObjectAfterIter");
        strcat(Filename,buffer);
        strcat(Filename,".bin");
        Fp3 = fopen(Filename, "w");
        //	for (i=0; i < Geometry->N_y; i++)
        //		for (j=0; j < Geometry->N_z; j++)
        //			for (k=0; k < Geometry->N_x; k++)
        TempPointer = m_Geometry->Object;
        NumOfBytesWritten=fwrite(&(m_Geometry->Object[0][0][0]), sizeof(DATA_TYPE),m_Geometry->N_x*m_Geometry->N_y*m_Geometry->N_z, Fp3);
        printf("%d\n",NumOfBytesWritten);

        fclose(Fp3);
      }
#endif
    }
		else//Update the voxels in most need for a update
		{
			
			//printf("Iter %d\n",Iter);
#ifdef ROI
			AverageUpdate = 0;
			AverageMagnitudeOfRecon = 0;
#endif
			
#ifdef RANDOM_ORDER_UPDATES
			ArraySize = m_Geometry->N_x * m_Geometry->N_z;
			for (j_new = 0; j_new < ArraySize; j_new++)
				Counter[j_new] = j_new;
			
			for (j = 0; j < m_Geometry->N_z; j++)
				for (k = 0; k < m_Geometry->N_x; k++)
					VisitCount[j][k] = 0;
#endif
			
			START;
			for (j = 0; j < m_Geometry->N_z; j++) //Row index
				for (k = 0; k < m_Geometry->N_x; k++) //Column index
				{
#ifdef RANDOM_ORDER_UPDATES
					//RandomNumber=init_genrand(Iter);
					Index = (genrand_int31(RandomNumber)) % ArraySize;
					k_new = Counter[Index] % m_Geometry->N_x;
					j_new = Counter[Index] / m_Geometry->N_x;
					//memmove(Counter+Index,Counter+Index+1,sizeof(int32_t)*(ArraySize - Index-1));
					//TODO: Instead just swap the value in Index with the one in ArraySize
					Counter[Index] = Counter[ArraySize - 1];
					VisitCount[j_new][k_new] = 1;
					ArraySize--;
#else
					j_new=j;
					k_new=k;
#endif //Random order updates
					TempMemBlock = TempCol[j_new][k_new]; //Remove this
					if(TempMemBlock->count > 0)
					{
						for (i = 0; i < m_Geometry->N_y; i++) //slice index
						{
							//Neighborhood of (i,j,k) should be initialized to zeros each time
							for (int32_t p = 0; p <= 2; p++)
							{
								for (int32_t q = 0; q <= 2; q++)
								{
									for (r = 0; r <= 2; r++)
									{
										NEIGHBORHOOD[p][q][r] = 0.0;
										BOUNDARYFLAG[p][q][r] = 0;
									}
								}
							}
#ifndef CIRCULAR_BOUNDARY_CONDITION
							
							//For a given (i,j,k) store its 26 point neighborhood
							for (int32_t p = -1; p <= 1; p++)
							{
								for (int32_t q = -1; q <= 1; q++)
								{
									for (r = -1; r <= 1; r++)
									{
										if(i + p >= 0 && i + p < m_Geometry->N_y) if(j_new + q >= 0 && j_new + q < m_Geometry->N_z) if(k_new + r >= 0
																																	   && k_new + r < m_Geometry->N_x)
										{
											NEIGHBORHOOD[p + 1][q + 1][r + 1] = m_Geometry->Object[q + j_new][r + k_new][p + i];
											BOUNDARYFLAG[p + 1][q + 1][r + 1] = 1;
										}
										else
										{
											BOUNDARYFLAG[p + 1][q + 1][r + 1] = 0;
										}
										
									}
								}
							}
#else
							for(p = -1; p <=1; p++)
								for(q = -1; q <= 1; q++)
									for(r = -1; r <= 1;r++)
									{
										tempindex_x = mod(r+k_new,m_Geometry->N_x);
										tempindex_y =mod(p+i,m_Geometry->N_y);
										tempindex_z = mod(q+j_new,m_Geometry->N_z);
										NEIGHBORHOOD[p+1][q+1][r+1] = m_Geometry->Object[tempindex_z][tempindex_x][tempindex_y];
										BOUNDARYFLAG[p+1][q+1][r+1]=1;
									}
							
#endif//circular boundary condition check
							NEIGHBORHOOD[1][1][1] = 0.0;
							
#ifdef DEBUG
							if(i == 0 && j == 31 && k == 31)
							{
								printf("***************************\n");
								printf("Geom %lf\n", m_Geometry->Object[i][31][31]);
								for (int p = 0; p <= 2; p++)
								{
									for (int q = 0; q <= 2; q++)
									{
										for (r = 0; r <= 2; r++)
										{
											printf("%lf\n", NEIGHBORHOOD[p][q][r]);
										}
									}
								}
							}
#endif
							//Compute theta1 and theta2
							
							V = m_Geometry->Object[j_new][k_new][i]; //Store the present value of the voxel
							THETA1 = 0.0;
							THETA2 = 0.0;
							
							/*		y = ((DATA_TYPE)i+0.5)*Geometry->delta_xy + Geometry->y0;
							 t = y;
							 tmin = (t - Geometry->delta_xy/2) > Sinogram->T0 ? t-Geometry->delta_xy/2 : Sinogram->T0;
							 tmax = (t + Geometry->delta_xy/2) <= Sinogram->TMax? t + Geometry->delta_xy/2 : Sinogram->TMax;
							 
							 slice_index_min = floor((tmin - Sinogram->T0)/Sinogram->delta_t);
							 slice_index_max = floor((tmax - Sinogram->T0)/Sinogram->delta_t);
							 
							 if(slice_index_min < 0)
							 slice_index_min = 0;
							 if(slice_index_max >= Sinogram->N_t)
							 slice_index_max = Sinogram->N_t-1;*/
							
							//TempCol = CE_CalculateAMatrixColumn(j, k, i, Sinogram, Geometry, VoxelProfile);
							for (uint32_t q = 0; q < TempMemBlock->count; q++)
							{
								
								uint16_t i_theta = floor(static_cast<float>(TempMemBlock->index[q] / (m_Sinogram->N_r)));
								uint16_t i_r = (TempMemBlock->index[q] % (m_Sinogram->N_r));
								VoxelLineAccessCounter = 0;
								for (uint32_t i_t = VoxelLineResponse[i].index[0]; i_t < VoxelLineResponse[i].index[0] + VoxelLineResponse[i].count; i_t++)
									// for(i_t = slice_index_min ; i_t <= slice_index_max; i_t++)
								{
									/* center_t = ((DATA_TYPE)i_t + 0.5)*Sinogram->delta_t + Sinogram->T0;
									 delta_t = fabs(center_t - t);
									 index_delta_t = floor(delta_t/OffsetT);
									 
									 if(index_delta_t < DETECTOR_RESPONSE_BINS)
									 {
									 w3 = delta_t - index_delta_t*OffsetT;
									 w4 = (index_delta_t+1)*OffsetT - delta_t;
									 //TODO: interpolation
									 ProfileThickness =(w4/OffsetT)*H_t[0][i_theta][index_delta_t] + (w3/OffsetT)*H_t[0][i_theta][index_delta_t+1 < DETECTOR_RESPONSE_BINS ? index_delta_t+1:DETECTOR_RESPONSE_BINS-1];
									 }
									 else
									 {
									 ProfileThickness=0;
									 }*/
									
									THETA2 += ((NuisanceParams.I_0[i_theta] * NuisanceParams.I_0[i_theta])
											   * (VoxelLineResponse[i].values[VoxelLineAccessCounter] * VoxelLineResponse[i].values[VoxelLineAccessCounter]) * (TempMemBlock->values[q])
											   * (TempMemBlock->values[q]) * Weight[i_theta][i_r][i_t]);
									THETA1 += NuisanceParams.I_0[i_theta] * ErrorSino[i_theta][i_r][i_t] * (TempMemBlock->values[q])
									* (VoxelLineResponse[i].values[VoxelLineAccessCounter]) * Weight[i_theta][i_r][i_t];
									VoxelLineAccessCounter++;
								}
							}
							THETA1 *= -1;
							minMax(&low, &high);
							
#ifdef DEBUG
							if(i == 0 && j == 31 && k == 31) printf("(%lf,%lf,%lf) \n", low, high, V - (THETA1 / THETA2));
#endif
							
							//Solve the 1-D optimization problem
							//printf("V before updating %lf",V);
#ifndef SURROGATE_FUNCTION
							//TODO : What if theta1 = 0 ? Then this will give error
							DerivOfCostFunc docf(BOUNDARYFLAG, NEIGHBORHOOD, FILTER, V, THETA1, THETA2, SIGMA_X_P, MRF_P);
							
							UpdatedVoxelValue = (DATA_TYPE)solve<DerivOfCostFunc>(&docf, (double)low, (double)high, (double)accuracy, &errorcode);
							
#else
							
							errorcode=0;
#ifdef QGGMRF
							UpdatedVoxelValue = CE_FunctionalSubstitution(low,high);
							
#else
							SurrogateUpdate=surrogateFunctionBasedMin();
							UpdatedVoxelValue=SurrogateUpdate;
#endif //QGGMRF
							
#endif//Surrogate function
							//printf("%lf\n",SurrogateUpdate);
							
							if(errorcode == 0)
							{
								//    printf("(%lf,%lf,%lf)\n",low,high,UpdatedVoxelValue);
								//	printf("Updated %lf\n",UpdatedVoxelValue);
#ifdef POSITIVITY_CONSTRAINT
								if(UpdatedVoxelValue < 0.0) //Enforcing positivity constraints
									UpdatedVoxelValue = 0.0;
#endif
							}
							else
							{
								if(THETA1 == 0 && low == 0 && high == 0) UpdatedVoxelValue = 0;
								else
								{
									printf("Error \n");
									printf("%d %d\n", j_new, k_new);
								}
							}
							
							//TODO Print appropriate error messages for other values of error code
							m_Geometry->Object[j_new][k_new][i] = UpdatedVoxelValue;
							
#ifdef ROI
							if(Mask[j_new][k_new] == 1)
							{
								
								AverageUpdate += fabs(m_Geometry->Object[j_new][k_new][i] - V);
								AverageMagnitudeOfRecon += fabs(V);//computing the percentage update =(Change in mag/Initial magnitude)
							}
#endif
							
							//Update the ErrorSinogram
							
							for (uint32_t q = 0; q < TempMemBlock->count; q++)
							{
								
								uint16_t i_theta = floor(static_cast<float>(TempMemBlock->index[q] / (m_Sinogram->N_r)));
								uint16_t i_r = (TempMemBlock->index[q] % (m_Sinogram->N_r));
								VoxelLineAccessCounter = 0;
								for (uint32_t i_t = VoxelLineResponse[i].index[0]; i_t < VoxelLineResponse[i].index[0] + VoxelLineResponse[i].count; i_t++)
									//for(i_t = slice_index_min ; i_t <= slice_index_max; i_t++)
								{
									/*	center_t = ((DATA_TYPE)i_t + 0.5)*Sinogram->delta_t + Sinogram->T0;
									 delta_t = fabs(center_t - t);
									 index_delta_t = floor(delta_t/OffsetT);
									 
									 if(index_delta_t < DETECTOR_RESPONSE_BINS)
									 {
									 w3 = delta_t - index_delta_t*OffsetT;
									 w4 = (index_delta_t+1)*OffsetT - delta_t;
									 //TODO: interpolation
									 ProfileThickness =(w4/OffsetT)*H_t[0][i_theta][index_delta_t] + (w3/OffsetT)*H_t[0][i_theta][index_delta_t+1 < DETECTOR_RESPONSE_BINS ? index_delta_t+1:DETECTOR_RESPONSE_BINS-1];
									 }
									 else
									 {
									 ProfileThickness=0;
									 }*/
									
									ErrorSino[i_theta][i_r][i_t] -= (NuisanceParams.I_0[i_theta]
																	 * (TempMemBlock->values[q] * VoxelLineResponse[i].values[VoxelLineAccessCounter] * (m_Geometry->Object[j_new][k_new][i] - V)));
									VoxelLineAccessCounter++;
								}
							}
							Idx++;
						}
						
					}
					else
					{
						continue;
					}
					
				}
			STOP;
			PRINTTIME;
			
#ifdef RANDOM_ORDER_UPDATES
			for (j = 0; j < m_Geometry->N_z; j++) //Row index
				for (k = 0; k < m_Geometry->N_x; k++) //Column index
					if(VisitCount[j][k] == 0) printf("Pixel (%d %d) not visited\n", j, k);
#endif
			
#ifdef COST_CALCULATE
			/*********************Cost Calculation***************************************************/
			
			cost[cost_counter] = computeCost(ErrorSino, Weight);
			printf("%lf\n", cost[cost_counter]);
			
			if(cost[cost_counter] - cost[cost_counter - 1] > 0)
			{
				printf("Cost just increased!\n");
				break;
			}
			
			fwrite(&cost[cost_counter], sizeof(DATA_TYPE), 1, Fp2);
			cost_counter++;
			/*******************************************************************************/
#else
			printf("%d\n",Iter);
#endif //Cost calculation endif
			
#ifdef ROI
			if(AverageMagnitudeOfRecon > 0)
			{
				printf("%d,%lf\n", Iter + 1, AverageUpdate / AverageMagnitudeOfRecon);
				if((AverageUpdate / AverageMagnitudeOfRecon) < m_Inputs->StopThreshold)
				{
					printf("This is the terminating point %d\n", Iter);
					m_Inputs->StopThreshold*=THRESHOLD_REDUCTION_FACTOR;//Reducing the thresold for subsequent iterations
					break;
				}
			}
#endif//ROI end
			if(getCancel() == true)
			{
				setErrorCondition(err);
				return;
			}
			
#ifdef WRITE_INTERMEDIATE_RESULTS
			
			if(Iter == NumOfWrites*WriteCount)
			{
				WriteCount++;
				sprintf(buffer,"%d",Iter);
				sprintf(Filename,"ReconstructedObjectAfterIter");
				strcat(Filename,buffer);
				strcat(Filename,".bin");
				Fp3 = fopen(Filename, "w");
				//	for (i=0; i < Geometry->N_y; i++)
				//		for (j=0; j < Geometry->N_z; j++)
				//			for (k=0; k < Geometry->N_x; k++)
				TempPointer = m_Geometry->Object;
				NumOfBytesWritten=fwrite(&(m_Geometry->Object[0][0][0]), sizeof(DATA_TYPE),m_Geometry->N_x*m_Geometry->N_y*m_Geometry->N_z, Fp3);
				printf("%d\n",NumOfBytesWritten);
				
				fclose(Fp3);
			}
#endif
		}
#else
		{
			
			//printf("Iter %d\n",Iter);
#ifdef ROI
			AverageUpdate = 0;
			AverageMagnitudeOfRecon = 0;
#endif
			
#ifdef RANDOM_ORDER_UPDATES
			ArraySize = m_Geometry->N_x * m_Geometry->N_z;
			for (j_new = 0; j_new < ArraySize; j_new++)
				Counter[j_new] = j_new;
			
			for (j = 0; j < m_Geometry->N_z; j++)
				for (k = 0; k < m_Geometry->N_x; k++)
					VisitCount[j][k] = 0;
#endif
			
			START;
			for (j = 0; j < m_Geometry->N_z; j++) //Row index
				for (k = 0; k < m_Geometry->N_x; k++) //Column index
				{
#ifdef RANDOM_ORDER_UPDATES
					//RandomNumber=init_genrand(Iter);
					Index = (genrand_int31(RandomNumber)) % ArraySize;
					k_new = Counter[Index] % m_Geometry->N_x;
					j_new = Counter[Index] / m_Geometry->N_x;
					//memmove(Counter+Index,Counter+Index+1,sizeof(int32_t)*(ArraySize - Index-1));
					//TODO: Instead just swap the value in Index with the one in ArraySize
					Counter[Index] = Counter[ArraySize - 1];
					VisitCount[j_new][k_new] = 1;
					ArraySize--;
#else
					j_new=j;
					k_new=k;
#endif //Random order updates
					TempMemBlock = TempCol[j_new][k_new]; //Remove this
					if(TempMemBlock->count > 0)
					{
						for (i = 0; i < m_Geometry->N_y; i++) //slice index
						{
							//Neighborhood of (i,j,k) should be initialized to zeros each time
							for (int32_t p = 0; p <= 2; p++)
							{
								for (int32_t q = 0; q <= 2; q++)
								{
									for (r = 0; r <= 2; r++)
									{
										NEIGHBORHOOD[p][q][r] = 0.0;
										BOUNDARYFLAG[p][q][r] = 0;
									}
								}
							}
#ifndef CIRCULAR_BOUNDARY_CONDITION
							
							//For a given (i,j,k) store its 26 point neighborhood
							for (int32_t p = -1; p <= 1; p++)
							{
								for (int32_t q = -1; q <= 1; q++)
								{
									for (r = -1; r <= 1; r++)
									{
										if(i + p >= 0 && i + p < m_Geometry->N_y) if(j_new + q >= 0 && j_new + q < m_Geometry->N_z) if(k_new + r >= 0
																																	   && k_new + r < m_Geometry->N_x)
										{
											NEIGHBORHOOD[p + 1][q + 1][r + 1] = m_Geometry->Object[q + j_new][r + k_new][p + i];
											BOUNDARYFLAG[p + 1][q + 1][r + 1] = 1;
										}
										else
										{
											BOUNDARYFLAG[p + 1][q + 1][r + 1] = 0;
										}
										
									}
								}
							}
#else
							for(p = -1; p <=1; p++)
								for(q = -1; q <= 1; q++)
									for(r = -1; r <= 1;r++)
									{
										tempindex_x = mod(r+k_new,m_Geometry->N_x);
										tempindex_y =mod(p+i,m_Geometry->N_y);
										tempindex_z = mod(q+j_new,m_Geometry->N_z);
										NEIGHBORHOOD[p+1][q+1][r+1] = m_Geometry->Object[tempindex_z][tempindex_x][tempindex_y];
										BOUNDARYFLAG[p+1][q+1][r+1]=1;
									}
							
#endif//circular boundary condition check
							NEIGHBORHOOD[1][1][1] = 0.0;
							
#ifdef DEBUG
							if(i == 0 && j == 31 && k == 31)
							{
								printf("***************************\n");
								printf("Geom %lf\n", m_Geometry->Object[i][31][31]);
								for (int p = 0; p <= 2; p++)
								{
									for (int q = 0; q <= 2; q++)
									{
										for (r = 0; r <= 2; r++)
										{
											printf("%lf\n", NEIGHBORHOOD[p][q][r]);
										}
									}
								}
							}
#endif
							//Compute theta1 and theta2
							
							V = m_Geometry->Object[j_new][k_new][i]; //Store the present value of the voxel
							THETA1 = 0.0;
							THETA2 = 0.0;
							
							/*		y = ((DATA_TYPE)i+0.5)*Geometry->delta_xy + Geometry->y0;
							 t = y;
							 tmin = (t - Geometry->delta_xy/2) > Sinogram->T0 ? t-Geometry->delta_xy/2 : Sinogram->T0;
							 tmax = (t + Geometry->delta_xy/2) <= Sinogram->TMax? t + Geometry->delta_xy/2 : Sinogram->TMax;
							 
							 slice_index_min = floor((tmin - Sinogram->T0)/Sinogram->delta_t);
							 slice_index_max = floor((tmax - Sinogram->T0)/Sinogram->delta_t);
							 
							 if(slice_index_min < 0)
							 slice_index_min = 0;
							 if(slice_index_max >= Sinogram->N_t)
							 slice_index_max = Sinogram->N_t-1;*/
							
							//TempCol = CE_CalculateAMatrixColumn(j, k, i, Sinogram, Geometry, VoxelProfile);
							for (uint32_t q = 0; q < TempMemBlock->count; q++)
							{
								
								uint16_t i_theta = floor(static_cast<float>(TempMemBlock->index[q] / (m_Sinogram->N_r)));
								uint16_t i_r = (TempMemBlock->index[q] % (m_Sinogram->N_r));
								VoxelLineAccessCounter = 0;
								for (uint32_t i_t = VoxelLineResponse[i].index[0]; i_t < VoxelLineResponse[i].index[0] + VoxelLineResponse[i].count; i_t++)
									// for(i_t = slice_index_min ; i_t <= slice_index_max; i_t++)
								{
									/* center_t = ((DATA_TYPE)i_t + 0.5)*Sinogram->delta_t + Sinogram->T0;
									 delta_t = fabs(center_t - t);
									 index_delta_t = floor(delta_t/OffsetT);
									 
									 if(index_delta_t < DETECTOR_RESPONSE_BINS)
									 {
									 w3 = delta_t - index_delta_t*OffsetT;
									 w4 = (index_delta_t+1)*OffsetT - delta_t;
									 //TODO: interpolation
									 ProfileThickness =(w4/OffsetT)*H_t[0][i_theta][index_delta_t] + (w3/OffsetT)*H_t[0][i_theta][index_delta_t+1 < DETECTOR_RESPONSE_BINS ? index_delta_t+1:DETECTOR_RESPONSE_BINS-1];
									 }
									 else
									 {
									 ProfileThickness=0;
									 }*/
									
									THETA2 += ((NuisanceParams.I_0[i_theta] * NuisanceParams.I_0[i_theta])
											   * (VoxelLineResponse[i].values[VoxelLineAccessCounter] * VoxelLineResponse[i].values[VoxelLineAccessCounter]) * (TempMemBlock->values[q])
											   * (TempMemBlock->values[q]) * Weight[i_theta][i_r][i_t]);
									THETA1 += NuisanceParams.I_0[i_theta] * ErrorSino[i_theta][i_r][i_t] * (TempMemBlock->values[q])
									* (VoxelLineResponse[i].values[VoxelLineAccessCounter]) * Weight[i_theta][i_r][i_t];
									VoxelLineAccessCounter++;
								}
							}
							THETA1 *= -1;
							minMax(&low, &high);
							
#ifdef DEBUG
							if(i == 0 && j == 31 && k == 31) printf("(%lf,%lf,%lf) \n", low, high, V - (THETA1 / THETA2));
#endif
							
							//Solve the 1-D optimization problem
							//printf("V before updating %lf",V);
#ifndef SURROGATE_FUNCTION
							//TODO : What if theta1 = 0 ? Then this will give error
							DerivOfCostFunc docf(BOUNDARYFLAG, NEIGHBORHOOD, FILTER, V, THETA1, THETA2, SIGMA_X_P, MRF_P);
							
							UpdatedVoxelValue = (DATA_TYPE)solve<DerivOfCostFunc>(&docf, (double)low, (double)high, (double)accuracy, &errorcode);
							
#else
							
							errorcode=0;
#ifdef QGGMRF
							UpdatedVoxelValue = CE_FunctionalSubstitution(low,high);
							
#else
							SurrogateUpdate=surrogateFunctionBasedMin();
							UpdatedVoxelValue=SurrogateUpdate;
#endif //QGGMRF
							
#endif//Surrogate function
							//printf("%lf\n",SurrogateUpdate);
							
							if(errorcode == 0)
							{
								//    printf("(%lf,%lf,%lf)\n",low,high,UpdatedVoxelValue);
								//	printf("Updated %lf\n",UpdatedVoxelValue);
#ifdef POSITIVITY_CONSTRAINT
								if(UpdatedVoxelValue < 0.0) //Enforcing positivity constraints
									UpdatedVoxelValue = 0.0;
#endif
							}
							else
							{
								if(THETA1 == 0 && low == 0 && high == 0) UpdatedVoxelValue = 0;
								else
								{
									printf("Error \n");
									printf("%d %d\n", j_new, k_new);
								}
							}
							
							//TODO Print appropriate error messages for other values of error code
							m_Geometry->Object[j_new][k_new][i] = UpdatedVoxelValue;
							
#ifdef ROI
							if(Mask[j_new][k_new] == 1)
							{
								
								AverageUpdate += fabs(m_Geometry->Object[j_new][k_new][i] - V);
								AverageMagnitudeOfRecon += fabs(V);//computing the percentage update =(Change in mag/Initial magnitude)
							}
#endif
							
							//Update the ErrorSinogram
							
							for (uint32_t q = 0; q < TempMemBlock->count; q++)
							{
								
								uint16_t i_theta = floor(static_cast<float>(TempMemBlock->index[q] / (m_Sinogram->N_r)));
								uint16_t i_r = (TempMemBlock->index[q] % (m_Sinogram->N_r));
								VoxelLineAccessCounter = 0;
								for (uint32_t i_t = VoxelLineResponse[i].index[0]; i_t < VoxelLineResponse[i].index[0] + VoxelLineResponse[i].count; i_t++)
									//for(i_t = slice_index_min ; i_t <= slice_index_max; i_t++)
								{
									/*	center_t = ((DATA_TYPE)i_t + 0.5)*Sinogram->delta_t + Sinogram->T0;
									 delta_t = fabs(center_t - t);
									 index_delta_t = floor(delta_t/OffsetT);
									 
									 if(index_delta_t < DETECTOR_RESPONSE_BINS)
									 {
									 w3 = delta_t - index_delta_t*OffsetT;
									 w4 = (index_delta_t+1)*OffsetT - delta_t;
									 //TODO: interpolation
									 ProfileThickness =(w4/OffsetT)*H_t[0][i_theta][index_delta_t] + (w3/OffsetT)*H_t[0][i_theta][index_delta_t+1 < DETECTOR_RESPONSE_BINS ? index_delta_t+1:DETECTOR_RESPONSE_BINS-1];
									 }
									 else
									 {
									 ProfileThickness=0;
									 }*/
									
									ErrorSino[i_theta][i_r][i_t] -= (NuisanceParams.I_0[i_theta]
																	 * (TempMemBlock->values[q] * VoxelLineResponse[i].values[VoxelLineAccessCounter] * (m_Geometry->Object[j_new][k_new][i] - V)));
									VoxelLineAccessCounter++;
								}
							}
							Idx++;
						}
						
					}
					else
					{
						continue;
					}
					
				}
			STOP;
			PRINTTIME;
			
#ifdef RANDOM_ORDER_UPDATES
			for (j = 0; j < m_Geometry->N_z; j++) //Row index
				for (k = 0; k < m_Geometry->N_x; k++) //Column index
					if(VisitCount[j][k] == 0) printf("Pixel (%d %d) not visited\n", j, k);
#endif
			
#ifdef COST_CALCULATE
			/*********************Cost Calculation***************************************************/
			
			cost[cost_counter] = computeCost(ErrorSino, Weight);
			printf("%lf\n", cost[cost_counter]);
			
			if(cost[cost_counter] - cost[cost_counter - 1] > 0)
			{
				printf("Cost just increased!\n");
				break;
			}
			
			fwrite(&cost[cost_counter], sizeof(DATA_TYPE), 1, Fp2);
			cost_counter++;
			/*******************************************************************************/
#else
			printf("%d\n",Iter);
#endif //Cost calculation endif
			
#ifdef ROI
			if(AverageMagnitudeOfRecon > 0)
			{
				printf("%d,%lf\n", Iter + 1, AverageUpdate / AverageMagnitudeOfRecon);
				if((AverageUpdate / AverageMagnitudeOfRecon) < m_Inputs->StopThreshold)
				{
					printf("This is the terminating point %d\n", Iter);
					m_Inputs->StopThreshold*=THRESHOLD_REDUCTION_FACTOR;//Reducing the thresold for subsequent iterations
					break;
				}
			}
#endif//ROI end
			if(getCancel() == true)
			{
				setErrorCondition(err);
				return;
			}
			
#ifdef WRITE_INTERMEDIATE_RESULTS
			
			if(Iter == NumOfWrites*WriteCount)
			{
				WriteCount++;
				sprintf(buffer,"%d",Iter);
				sprintf(Filename,"ReconstructedObjectAfterIter");
				strcat(Filename,buffer);
				strcat(Filename,".bin");
				Fp3 = fopen(Filename, "w");
				//	for (i=0; i < Geometry->N_y; i++)
				//		for (j=0; j < Geometry->N_z; j++)
				//			for (k=0; k < Geometry->N_x; k++)
				TempPointer = m_Geometry->Object;
				NumOfBytesWritten=fwrite(&(m_Geometry->Object[0][0][0]), sizeof(DATA_TYPE),m_Geometry->N_x*m_Geometry->N_y*m_Geometry->N_z, Fp3);
				printf("%d\n",NumOfBytesWritten);
				
				fclose(Fp3);
			}
#endif
		}
#endif//Non Homogenous ICD
	}	
#ifdef JOINT_ESTIMATION

    //high=5e100;//this maintains the max and min bracket values for rooting lambda
    high = (DATA_TYPE)INT64_MAX;
    low = (DATA_TYPE)INT64_MIN;
    //Joint Scale And Offset Estimation

    //forward project
    for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
    {
      for (uint16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++)
      {
        for (uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
        {
          Y_Est[i_theta][i_r][i_t] = 0.0;
        }
      }
    }

    for (j = 0; j < m_Geometry->N_z; j++)
    {
      for (k = 0; k < m_Geometry->N_x; k++)
      {
        if(TempCol[j][k]->count > 0)
        {
          for (i = 0; i < m_Geometry->N_y; i++) //slice index
          {

            for (uint32_t q = 0; q < TempCol[j][k]->count; q++)
            {
              //calculating the footprint of the voxel in the t-direction

              uint16_t i_theta = int16_t(floor(static_cast<float>(TempCol[j][k]->index[q] / (m_Sinogram->N_r))));
              uint16_t i_r = (TempCol[j][k]->index[q] % (m_Sinogram->N_r));

              VoxelLineAccessCounter = 0;
              for (uint32_t i_t = VoxelLineResponse[i].index[0]; i_t < VoxelLineResponse[i].index[0] + VoxelLineResponse[i].count; i_t++)
              {
                Y_Est[i_theta][i_r][i_t] += ((TempCol[j][k]->values[q] * VoxelLineResponse[i].values[VoxelLineAccessCounter++] * m_Geometry->Object[j][k][i]));
              }
            }

          }
        }

      }
    }

    for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
    {
      a = 0;
      b = 0;
      c = 0;
      d = 0;
      e = 0;
      numerator_sum = 0;
      denominator_sum = 0;

      //compute the parameters of the quadratic for each view
      for (uint16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++)
      {
        for (uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
        {

          numerator_sum += (m_Sinogram->counts[i_theta][i_r][i_t] * Weight[i_theta][i_r][i_t]);
          denominator_sum += (Weight[i_theta][i_r][i_t]);

          a += (Y_Est[i_theta][i_r][i_t] * Weight[i_theta][i_r][i_t]);
          b += (Y_Est[i_theta][i_r][i_t] * Weight[i_theta][i_r][i_t] * m_Sinogram->counts[i_theta][i_r][i_t]);
          c += (m_Sinogram->counts[i_theta][i_r][i_t] * m_Sinogram->counts[i_theta][i_r][i_t] * Weight[i_theta][i_r][i_t]);
          d += (Y_Est[i_theta][i_r][i_t] * Y_Est[i_theta][i_r][i_t] * Weight[i_theta][i_r][i_t]);

        }
      }

      bk_cost[i_theta][1] = numerator_sum; //yt*\lambda*1
      bk_cost[i_theta][0] = b; //yt*\lambda*(Ax)
      ck_cost[i_theta] = c; //yt*\lambda*y
      Qk_cost[i_theta][2] = denominator_sum;
      Qk_cost[i_theta][1] = a;
      Qk_cost[i_theta][0] = d;

      d1[i_theta] = numerator_sum / denominator_sum;
      d2[i_theta] = a / denominator_sum;

      a = 0;
      b = 0;
      for (uint16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++) {
        for (uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
        {
          a += ((Y_Est[i_theta][i_r][i_t] - d2[i_theta]) * Weight[i_theta][i_r][i_t] * Y_Est[i_theta][i_r][i_t]);
          b -= ((m_Sinogram->counts[i_theta][i_r][i_t] - d1[i_theta]) * Weight[i_theta][i_r][i_t] * Y_Est[i_theta][i_r][i_t]);
        }
      }
      QuadraticParameters[i_theta][0] = a;
      QuadraticParameters[i_theta][1] = b;

      temp = (QuadraticParameters[i_theta][1] * QuadraticParameters[i_theta][1]) / (4 * QuadraticParameters[i_theta][0]);

      if(temp > 0 && temp < high) high = temp; //high holds the maximum value for the rooting operation. beyond this value discriminants become negative. Basically high = min{b^2/4*a}
      else if(temp < 0 && temp > low) low = temp;

    }
    //compute cost
    /********************************************************************************************/
    sum = 0;
    for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
    {
      sum += (Qk_cost[i_theta][0] * NuisanceParams.I_0[i_theta] * NuisanceParams.I_0[i_theta]
          + 2 * Qk_cost[i_theta][1] * NuisanceParams.I_0[i_theta] * NuisanceParams.mu[i_theta]
          + NuisanceParams.mu[i_theta] * NuisanceParams.mu[i_theta] * Qk_cost[i_theta][2]
          - 2 * (bk_cost[i_theta][0] * NuisanceParams.I_0[i_theta] + NuisanceParams.mu[i_theta] * bk_cost[i_theta][1]) + ck_cost[i_theta]); //evaluating the cost function
    }
    sum /= 2;
    printf("The value of the data match error prior to updating the I and mu =%lf\n", sum);

    /********************************************************************************************/

#ifdef GEOMETRIC_MEAN_CONSTRAINT
    calculateGeometricMeanConstraint();
#else
    sum1 = 0;
    sum2 = 0;
    for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
    {
      sum1 += (1.0 / (Qk_cost[i_theta][0] - Qk_cost[i_theta][1] * d2[i_theta]));
      sum2 += ((bk_cost[i_theta][0] - Qk_cost[i_theta][1] * d1[i_theta]) / (Qk_cost[i_theta][0] - Qk_cost[i_theta][1] * d2[i_theta]));
    }
    LagrangeMultiplier = (-m_Sinogram->N_theta * m_Sinogram->TargetGain + sum2) / sum1;
    for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
    {

      NuisanceParams.I_0[i_theta] = (-1 * LagrangeMultiplier - Qk_cost[i_theta][1] * d1[i_theta] + bk_cost[i_theta][0])
          / (Qk_cost[i_theta][0] - Qk_cost[i_theta][1] * d2[i_theta]);
      NuisanceParams.mu[i_theta] = d1[i_theta] - d2[i_theta] * NuisanceParams.I_0[i_theta]; //some function of I_0[i_theta]
    }

#endif //Type of constraing Geometric or arithmetic
    //Re normalization

    /*sum=0;
     for(i_theta = 0 ; i_theta < Sinogram->N_theta; i_theta++)
     sum+=(log(NuisanceParams.I_0[i_theta]));
     sum/=Sinogram->N_theta;
     temp=exp(sum) - Sinogram->TargetGain;

     printf("%lf\n",temp);*/

    /********************************************************************************************/
    //checking to see if the cost went down
    sum = 0;
    for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
    {
      sum += (Qk_cost[i_theta][0] * NuisanceParams.I_0[i_theta] * NuisanceParams.I_0[i_theta]
          + 2 * Qk_cost[i_theta][1] * NuisanceParams.I_0[i_theta] * NuisanceParams.mu[i_theta]
          + NuisanceParams.mu[i_theta] * NuisanceParams.mu[i_theta] * Qk_cost[i_theta][2]
          - 2 * (bk_cost[i_theta][0] * NuisanceParams.I_0[i_theta] + NuisanceParams.mu[i_theta] * bk_cost[i_theta][1]) + ck_cost[i_theta]); //evaluating the cost function
    }
    sum /= 2;

    printf("The value of the data match error after updating the I and mu =%lf\n", sum);

    /*****************************************************************************************************/

    //Reproject to compute Error Sinogram for ICD
    for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
    {
      for (uint16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++)
      {
        for (uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
        {
          ErrorSino[i_theta][i_r][i_t] = m_Sinogram->counts[i_theta][i_r][i_t] - NuisanceParams.mu[i_theta]
              - (NuisanceParams.I_0[i_theta] * Y_Est[i_theta][i_r][i_t]);
        }
      }
    }

    //Printing the gain and offset after updating
    /*
     printf("Offsets\n");
     for(i_theta = 0 ; i_theta < Sinogram->N_theta; i_theta++)
     {
     printf("%lf\n",NuisanceParams.mu[i_theta]);
     }
     printf("Gain\n");
     for(i_theta = 0 ; i_theta < Sinogram->N_theta; i_theta++)
     {
     printf("%lf\n",NuisanceParams.I_0[i_theta]);
     }*/

#ifdef COST_CALCULATE
    cost[cost_counter] = computeCost(ErrorSino, Weight);
    printf("After Gain Update %lf\n", cost[cost_counter]);

    if(cost[cost_counter] - cost[cost_counter - 1] > 0)
    {
      printf("Cost just increased!\n");
      break;
    }
    fwrite(&cost[cost_counter], sizeof(DATA_TYPE), 1, Fp2);
    cost_counter++;

#endif
    printf("Lagrange Multiplier = %lf\n", LagrangeMultiplier);

    printf("Offsets\n");
    for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
    {
      printf("%lf\n", NuisanceParams.mu[i_theta]);
    }
    printf("Gain\n");
    for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
    {
      printf("%lf\n", NuisanceParams.I_0[i_theta]);
    }

#ifdef NOISE_MODEL
    //Updating the Weights
    for(uint16_t i_theta=0;i_theta < m_Sinogram->N_theta;i_theta++)
    {
      sum=0;
      for(uint16_t i_r=0; i_r < m_Sinogram->N_r; i_r++)
      for(uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
      sum+=(ErrorSino[i_theta][i_r][i_t]*ErrorSino[i_theta][i_r][i_t]*Weight[i_theta][i_r][i_t]);
      sum/=(m_Sinogram->N_r*m_Sinogram->N_t);

		NuisanceParams.alpha[i_theta]=sum;

      for(uint16_t i_r=0; i_r < m_Sinogram->N_r; i_r++)
      for(uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
      if(NuisanceParams.alpha[i_theta] != 0)
		  Weight[i_theta][i_r][i_t]/=NuisanceParams.alpha[i_theta];

    }

#ifdef COST_CALCULATE
    cost[cost_counter]=computeCost(ErrorSino,Weight);
    printf("After Noise Variance Update %lf\n",cost[cost_counter]);
    fwrite(&cost[cost_counter],sizeof(DATA_TYPE),1,Fp2);
    if(cost[cost_counter]-cost[cost_counter-1] > 0)
    {
      printf("Cost just increased!\n");
      break;
    }

    cost_counter++;

#endif//cost
#endif//NOISE_MODEL
#endif//Joint estimation endif
    printf("Outer Iter %d\n", OuterIter + 1);
  }

	printf("Offsets\n");
	for(uint16_t i_theta = 0 ; i_theta < m_Sinogram->N_theta; i_theta++)
	{
		printf("%lf\n",NuisanceParams.mu[i_theta]);
		fwrite(&NuisanceParams.mu[i_theta],sizeof(DATA_TYPE),1,Fp5);
	}
	printf("Gain\n");
	for(uint16_t i_theta = 0 ; i_theta < m_Sinogram->N_theta; i_theta++)
	{
		printf("%lf\n",NuisanceParams.I_0[i_theta]);
		fwrite(&NuisanceParams.I_0[i_theta],sizeof(DATA_TYPE),1,Fp4);
	}
	printf("Number of costs recorded %d\n",cost_counter);
	printf("Final Cost\n");
	for(i = 0 ; i < cost_counter; i++)
	{
		printf("%lf\n",cost[i]);
	}
	
#ifdef NOISE_MODEL
	if(NuisanceParams.alpha != NULL)
  {
    MAKE_OUTPUT_FILE(Fp7, fileError, m_Inputs->outputDir, ScaleOffsetCorrection::FinalVariancesFile);
    if(fileError >= 0)
    {
      updateProgressAndMessage("NuisanceParams ", 50);
      for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
      {
        printf("%lf\n", NuisanceParams.alpha[i_theta]);
        fwrite(&(NuisanceParams.alpha[i_theta]), sizeof(DATA_TYPE), 1, Fp7);
      }
      fclose(Fp7);
    }
  }
#endif//Noise Model


  START;
	//calculates Ax and returns a pointer to the memory block
	Final_Sinogram=forwardProject(DetectorResponse, H_t);
	STOP;
	PRINTTIME;

  updateProgressAndMessage("Writing Final Sinogram", 50);

	//Writing the final sinogram
	for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
  {
    for (uint16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++)
    {
      for (uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
      {
        Final_Sinogram[i_theta][i_r][i_t] *= NuisanceParams.I_0[i_theta];
        Final_Sinogram[i_theta][i_r][i_t] += NuisanceParams.mu[i_theta];
        fwrite(&Final_Sinogram[i_theta][i_r][i_t], sizeof(DATA_TYPE), 1, Fp1);
      }
    }
  }
	fclose(Fp1);

	RawGeometryWriter::Pointer writer = RawGeometryWriter::New();
	writer->setGeometry(m_Geometry);
	writer->setFilePath(m_Inputs->OutputFile);
	writer->execute();
	if (writer->getErrorCondition() < 0)
	{
	  setErrorCondition(writer->getErrorCondition());
	  updateProgressAndMessage("Error Writing the Raw Geometry", 100);
	}

  std::cout << "Final Dimensions of Object: " << std::endl;
  std::cout << "  Nx = " << m_Geometry->N_x << std::endl;
  std::cout << "  Ny = " << m_Geometry->N_y << std::endl;
  std::cout << "  Nz = " << m_Geometry->N_z << std::endl;


	free_img((void**)VoxelProfile);
	//free(AMatrix);
#ifdef STORE_A_MATRIX
	multifree(AMatrix,2);
	//#else
	//	free((void*)TempCol);
#endif
	free_3D((void***)ErrorSino);
	free_3D((void***)Weight);

#ifdef COST_CALCULATE
	fclose(Fp2);// writing cost function
#endif
	//fclose(Fp3);
	fclose(Fp4);
	fclose(Fp5);
	//fclose(Fp6);

	free_3D((void***)m_Geometry->Object);
	//free_3D(neighborhood);
	// Get values from ComputationInputs and perform calculation
	// Return any error code
	setErrorCondition(err);
	return;
}


/*****************************************************************************
 //Finds the min and max of the neighborhood . This is required prior to calling
 solve()
 *****************************************************************************/
void SOCEngine::minMax(DATA_TYPE *low,DATA_TYPE *high)
{
	uint8_t i,j,k;
	*low=NEIGHBORHOOD[0][0][0];
	*high=NEIGHBORHOOD[0][0][0];

	for(i = 0; i < 3;i++)
	{
		for(j=0; j < 3; j++)
		{
			for(k = 0; k < 3; k++)
			{
				//  if(NEIGHBORHOOD[i][j][k] != 0)
				//  printf("%lf ", NEIGHBORHOOD[i][j][k]);

				if(NEIGHBORHOOD[i][j][k] < *low)
					*low = NEIGHBORHOOD[i][j][k];
				if(NEIGHBORHOOD[i][j][k] > *high)
					*high=NEIGHBORHOOD[i][j][k];
			}
			//	printf("\n");
		}
	}


	if(THETA2 !=0)
	{
	*low = (*low > (V - (THETA1/THETA2)) ? (V - (THETA1/THETA2)): *low);
	*high = (*high < (V - (THETA1/THETA2)) ? (V - (THETA1/THETA2)): *high);
	}
}



void* SOCEngine::calculateVoxelProfile()
{
	DATA_TYPE angle,MaxValLineIntegral;
	DATA_TYPE temp,dist1,dist2,LeftCorner,LeftNear,RightNear,RightCorner,t;
	DATA_TYPE** VoxProfile = (DATA_TYPE**)multialloc(sizeof(DATA_TYPE),2,m_Sinogram->N_theta,PROFILE_RESOLUTION);
	DATA_TYPE checksum=0;
	uint16_t i,j;
	FILE* Fp = NULL;
	int fileError = 0;
  MAKE_OUTPUT_FILE(Fp, fileError, m_Inputs->outputDir, ScaleOffsetCorrection::VoxelProfileFile);
  if (fileError == 1)
  {

  }

	for (i=0;i<m_Sinogram->N_theta;i++)
	{
		m_Sinogram->angles[i]=m_Sinogram->angles[i]*(M_PI/180.0);
		angle=m_Sinogram->angles[i];
		while(angle > M_PI_2)
			angle -= M_PI_2;

		while(angle < 0)
			angle +=M_PI_2;

		if(angle <= M_PI_4)
		{
			MaxValLineIntegral = m_Inputs->delta_xz/cos(angle);
		}
		else
		{
			MaxValLineIntegral = m_Inputs->delta_xz/cos(M_PI_2-angle);
		}
		temp=cos(M_PI_4);
		dist1 = temp * cos((M_PI_4 - angle));
		dist2 = temp * fabs((cos((M_PI_4 + angle))));
		LeftCorner = 1-dist1;
		LeftNear = 1-dist2;
		RightNear = 1+dist2;
		RightCorner = 1+dist1;

		for(j = 0;j<PROFILE_RESOLUTION;j++)
		{
			t = 2.0*j / PROFILE_RESOLUTION;//2 is the normalized length of the profile (basically equl to 2*delta_xz)
			if(t <= LeftCorner || t >= RightCorner)
				VoxProfile[i][j] = 0;
			else if(t > RightNear)
				VoxProfile[i][j] = MaxValLineIntegral*(RightCorner-t)/(RightCorner-RightNear);
			else if(t >= LeftNear)
				VoxProfile[i][j] = MaxValLineIntegral;
			else
				VoxProfile[i][j] = MaxValLineIntegral*(t-LeftCorner)/(LeftNear-LeftCorner);

			fwrite(&VoxProfile[i][j],sizeof(DATA_TYPE),1,Fp);
			checksum+=VoxProfile[i][j];
		}

	}

	//printf("Pixel Profile Check sum =%lf\n",checksum);
	fclose(Fp);
	return VoxProfile;
}

/*******************************************************************
 Forwards Projects the Object and stores it in a 3-D matrix
 ********************************************************************/
DATA_TYPE*** SOCEngine::forwardProject(DATA_TYPE*** DetectorResponse,DATA_TYPE*** H_t)
{
  updateProgressAndMessage("Executing Forward Projection", 50);

	DATA_TYPE x,z,y;
	DATA_TYPE r,rmin,rmax,t,tmin,tmax;
	DATA_TYPE w1,w2,f1,f2;
	DATA_TYPE center_r,delta_r,center_t,delta_t;
	int16_t index_min,index_max,slice_index_min,slice_index_max,i_r,i_t,i_theta;
	int16_t i,j,k;
	int16_t index_delta_t,index_delta_r;
	DATA_TYPE*** Y_Est;
	Y_Est=(DATA_TYPE ***)get_3D(m_Sinogram->N_theta,m_Sinogram->N_r,m_Sinogram->N_t,sizeof(DATA_TYPE));

	for (j = 0; j < m_Geometry->N_z; j++)
	{
		for (k=0; k < m_Geometry->N_x; k++)
		{
			x = m_Geometry->x0 + ((DATA_TYPE)k+0.5)*m_Inputs->delta_xz;//0.5 is for center of voxel. x_0 is the left corner
			z = m_Geometry->z0 + ((DATA_TYPE)j+0.5)*m_Inputs->delta_xz;//0.5 is for center of voxel. z_0 is the left corner

			for (i = 0; i < m_Geometry->N_y ; i++)
			{
				for(i_theta = 0;i_theta < m_Sinogram->N_theta;i_theta++)
				{
					r = x*cosine[i_theta] - z*sine[i_theta];
					rmin = r - m_Inputs->delta_xz;
					rmax = r + m_Inputs->delta_xz;

					if(rmax < m_Sinogram->R0 || rmin > m_Sinogram->RMax)
						continue;

					index_min = floor(((rmin - m_Sinogram->R0)/m_Sinogram->delta_r));
					index_max = floor((rmax - m_Sinogram->R0)/m_Sinogram->delta_r);

					if(index_max >= m_Sinogram->N_r)
						index_max = m_Sinogram->N_r - 1;

					if(index_min < 0)
						index_min = 0;

					y = m_Geometry->y0 + ((double)i+ 0.5)*m_Inputs->delta_xy;
					t = y;


					tmin = (t - m_Inputs->delta_xy/2) > m_Sinogram->T0 ? t-m_Inputs->delta_xy/2 : m_Sinogram->T0;
					tmax = (t + m_Inputs->delta_xy/2) <= m_Sinogram->TMax? t + m_Inputs->delta_xy/2 : m_Sinogram->TMax;

					slice_index_min = floor((tmin - m_Sinogram->T0)/m_Sinogram->delta_t);
					slice_index_max = floor((tmax - m_Sinogram->T0)/m_Sinogram->delta_t);

					if(slice_index_min < 0)
						slice_index_min = 0;
					if(slice_index_max >= m_Sinogram->N_t)
						slice_index_max = m_Sinogram->N_t-1;

					for (i_r = index_min; i_r <= index_max; i_r++)
					{
						center_r = m_Sinogram->R0 + ((double)i_r + 0.5)*m_Sinogram->delta_r ;
						delta_r = fabs(center_r - r);
						index_delta_r = floor((delta_r/OffsetR));

						if(index_delta_r < DETECTOR_RESPONSE_BINS)
						{
							w1 = delta_r - index_delta_r*OffsetR;
							w2 = (index_delta_r+1)*OffsetR - delta_r;
							f1 = (w2/OffsetR)*DetectorResponse[0][i_theta][index_delta_r] + (w1/OffsetR)*DetectorResponse[0][i_theta][index_delta_r+1 < DETECTOR_RESPONSE_BINS ? index_delta_r+1:DETECTOR_RESPONSE_BINS-1];
						}
						else
						{
							f1=0;
						}



						for (i_t = slice_index_min; i_t <= slice_index_max; i_t ++)
						{
							center_t = m_Sinogram->T0 + ((double)i_t + 0.5)*m_Sinogram->delta_t;
							delta_t = fabs(center_t - t);
							index_delta_t = floor((delta_t/OffsetT));

							if(index_delta_t < DETECTOR_RESPONSE_BINS)
							{
								w1 = delta_t - index_delta_t*OffsetT;
								w2 = (index_delta_t+1)*OffsetT - delta_t;
								f2 = (w2/OffsetT)*H_t[0][i_theta][index_delta_t] + (w1/OffsetT)*H_t[0][i_theta][index_delta_t+1 < DETECTOR_RESPONSE_BINS ? index_delta_t+1:DETECTOR_RESPONSE_BINS-1];
							}
							else
							{
								f2 = 0;
							}

							Y_Est[i_theta][i_r][i_t]+=(f1*f2*m_Geometry->Object[j][k][i]);

						}
					}
				}
			}

		}

	}

	return Y_Est;
}


/* Initializes the global variables cosine and sine to speed up computation
 */
void SOCEngine::calculateSinCos()
{
	uint16_t i;
	cosine=(DATA_TYPE*)get_spc(m_Sinogram->N_theta,sizeof(DATA_TYPE));
	sine=(DATA_TYPE*)get_spc(m_Sinogram->N_theta,sizeof(DATA_TYPE));
	for(i=0;i<m_Sinogram->N_theta;i++)
	{
		cosine[i]=cos(m_Sinogram->angles[i]);
		sine[i]=sin(m_Sinogram->angles[i]);
	}
}

void SOCEngine::initializeBeamProfile()
{
	uint16_t i;
	DATA_TYPE sum=0,W;
	BeamProfile=(DATA_TYPE*)get_spc(BEAM_RESOLUTION,sizeof(DATA_TYPE));
	W=BEAM_WIDTH/2;
	for (i=0; i < BEAM_RESOLUTION ;i++)
	{
		//BeamProfile[i] = (1.0/(BEAM_WIDTH)) * ( 1 + cos ((PI/W)*fabs(-W + i*(BEAM_WIDTH/BEAM_RESOLUTION))));
		BeamProfile[i] = 0.54 - 0.46*cos((2*M_PI/BEAM_RESOLUTION)*i);
		sum=sum+BeamProfile[i];
	}

	//Normalize the beam to have an area of 1

	for (i=0; i < BEAM_RESOLUTION ;i++)
	{

		BeamProfile[i]/=sum;
		BeamProfile[i]/=m_Sinogram->delta_t;
		// printf("%lf\n",BeamProfile[i]);
	}



}


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
DATA_TYPE SOCEngine::computeCost(DATA_TYPE*** ErrorSino,DATA_TYPE*** Weight)
{
	DATA_TYPE cost=0,temp=0;
//	DATA_TYPE delta;
	int16_t i,j,k;
//	int16_t p,q,r;

//Data Mismatch Error
	for (i = 0; i < m_Sinogram->N_theta; i++)
		for (j = 0; j < m_Sinogram->N_r; j++)
			for( k = 0; k < m_Sinogram->N_t; k++)
			{
				cost+=(ErrorSino[i][j][k] * ErrorSino[i][j][k] * Weight[i][j][k]);

			}

	cost/=2;

	printf("Data mismatch term =%lf\n",cost);

//Prior Model Error
#ifndef QGGMRF
	for (i = 0; i < m_Geometry->N_z; i++)
		for (j = 0; j < m_Geometry->N_x; j++)
			for(k = 0; k < m_Geometry->N_y; k++)
			{

				if(k+1 <  m_Geometry->N_y)
					temp += FILTER[2][1][1]*pow(fabs(m_Geometry->Object[i][j][k]-m_Geometry->Object[i][j][k+1]),MRF_P);


				if(j+1 < m_Geometry->N_x)
				{
					if(k-1 >= 0)
						temp += FILTER[0][1][2]*pow(fabs(m_Geometry->Object[i][j][k]-m_Geometry->Object[i][j+1][k-1]),MRF_P);


					temp += FILTER[1][1][2]*pow(fabs(m_Geometry->Object[i][j][k]-m_Geometry->Object[i][j+1][k]),MRF_P);


					if(k+1 < m_Geometry->N_y)
						temp += FILTER[2][1][2]*pow(fabs(m_Geometry->Object[i][j][k]-m_Geometry->Object[i][j+1][k+1]),MRF_P);

				}

				if(i+1 < m_Geometry->N_z)
				{

					if(j-1 >= 0)
						temp += FILTER[1][2][0]*pow(fabs(m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j-1][k]),MRF_P);

					temp += FILTER[1][2][1]*pow(fabs(m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j][k]),MRF_P);

					if(j+1 < m_Geometry->N_x)
						temp += FILTER[1][2][2]*pow(fabs(m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j+1][k]),MRF_P);


					if(j-1 >= 0)
					{
						if(k-1 >= 0)
							temp += FILTER[0][2][0]*pow(fabs(m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j-1][k-1]),MRF_P);

						if(k+1 < m_Geometry->N_y)
							temp += FILTER[2][2][0]*pow(fabs(m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j-1][k+1]),MRF_P);

					}

					if(k-1 >= 0)
						temp += FILTER[0][2][1]*pow(fabs(m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j][k-1]),MRF_P);

					if(j+1 < m_Geometry->N_x)
					{
						if(k-1 >= 0)
							temp += FILTER[0][2][2]*pow(fabs(m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j+1][k-1]),MRF_P);

						if(k+1 < m_Geometry->N_y)
							temp+= FILTER[2][2][2]*pow(fabs(m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j+1][k+1]),MRF_P);
					}

					if(k+1 < m_Geometry->N_y)
						temp+= FILTER[2][2][1]*pow(fabs(m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j][k+1]),MRF_P);
				}
			}
		cost+=(temp/(MRF_P*SIGMA_X_P));
#else
	for (i = 0; i < m_Geometry->N_z; i++)
		for (j = 0; j < m_Geometry->N_x; j++)
			for(k = 0; k < m_Geometry->N_y; k++)
			{

				if(k+1 <  m_Geometry->N_y)
				{
					delta=m_Geometry->Object[i][j][k]-m_Geometry->Object[i][j][k+1];
					temp += FILTER[2][1][1]*(pow(fabs(delta),MRF_P))/(1+pow(fabs(delta/MRF_C), MRF_P-MRF_Q));
				}



				if(j+1 < m_Geometry->N_x)
				{
					if(k-1 >= 0)
					{
						delta=m_Geometry->Object[i][j][k]-m_Geometry->Object[i][j+1][k-1];
						temp += FILTER[0][1][2]*(pow(fabs(delta),MRF_P))/(1+pow(fabs(delta/MRF_C), MRF_P-MRF_Q));
					}

					delta=m_Geometry->Object[i][j][k]-m_Geometry->Object[i][j+1][k];
					temp += FILTER[1][1][2]*(pow(fabs(delta),MRF_P))/(1+pow(fabs(delta/MRF_C), MRF_P-MRF_Q));


					if(k+1 < m_Geometry->N_y)
					{
						delta=m_Geometry->Object[i][j][k]-m_Geometry->Object[i][j+1][k+1];
						temp += FILTER[2][1][2]*(pow(fabs(delta),MRF_P))/(1+pow(fabs(delta/MRF_C), MRF_P-MRF_Q));
					}

				}

				if(i+1 < m_Geometry->N_z)
				{

					if(j-1 >= 0)
					{
						delta = m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j-1][k];
						temp += FILTER[1][2][0]*(pow(fabs(delta),MRF_P))/(1+pow(fabs(delta/MRF_C), MRF_P-MRF_Q));
					}

					delta=m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j][k];
					temp += FILTER[1][2][1]*(pow(fabs(delta),MRF_P))/(1+pow(fabs(delta/MRF_C), MRF_P-MRF_Q));

					if(j+1 < m_Geometry->N_x)
					{
						delta=m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j+1][k];
						temp += FILTER[1][2][2]*(pow(fabs(delta),MRF_P))/(1+pow(fabs(delta/MRF_C), MRF_P-MRF_Q));
					}


					if(j-1 >= 0)
					{
						if(k-1 >= 0)
						{
							delta=m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j-1][k-1];
							temp += FILTER[0][2][0]*(pow(fabs(delta),MRF_P))/(1+pow(fabs(delta/MRF_C), MRF_P-MRF_Q));
						}

						if(k+1 < m_Geometry->N_y)
						{
							delta=m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j-1][k+1];
							temp += FILTER[2][2][0]*(pow(fabs(delta),MRF_P))/(1+pow(fabs(delta/MRF_C), MRF_P-MRF_Q));
						}

					}

					if(k-1 >= 0)
					{
						delta = m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j][k-1];
						temp += FILTER[0][2][1]*(pow(fabs(delta),MRF_P))/(1+pow(fabs(delta/MRF_C), MRF_P-MRF_Q));
					}

					if(j+1 < m_Geometry->N_x)
					{
						if(k-1 >= 0)
						{
							delta = m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j+1][k-1];
							temp += FILTER[0][2][2]*(pow(fabs(delta),MRF_P))/(1+pow(fabs(delta/MRF_C), MRF_P-MRF_Q));
						}

						if(k+1 < m_Geometry->N_y)
						{
							delta = m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j+1][k+1];
							temp+= FILTER[2][2][2]*(pow(fabs(delta),MRF_P))/(1+pow(fabs(delta/MRF_C), MRF_P-MRF_Q));
						}
					}

					if(k+1 < m_Geometry->N_y)
					{
						delta = m_Geometry->Object[i][j][k]-m_Geometry->Object[i+1][j][k+1];
						temp+= FILTER[2][2][1]*(pow(fabs(delta),MRF_P))/(1+pow(fabs(delta/MRF_C), MRF_P-MRF_Q));
					}
				}
			}
			cost+=(temp);
#endif //QGGMRF

	//printf("Cost calculation End..\n");


//Noise Error
#ifdef NOISE_MODEL
	temp=0;
	for(i=0;i< m_Sinogram->N_theta;i++)
		if(Weight[i][0][0] != 0)
		temp += log(2*M_PI*(1.0/Weight[i][0][0]));//2*pi*sigma_k^{2}

	temp*=((m_Sinogram->N_r*m_Sinogram->N_t)/2);

	cost+=temp;
#endif//noise model
	return cost;
}

void* SOCEngine::detectorResponse(uint16_t row,uint16_t col, DATA_TYPE** VoxelProfile)
{

  FILE* Fp = NULL;
  int err = 0;
  MAKE_OUTPUT_FILE(Fp, err, m_Inputs->outputDir, ScaleOffsetCorrection::DetectorResponseFile)
  if (err == 1)
  {
    std::cout << "Error creating output file for Detector Response." << std::endl;
    return NULL;
  }


	DATA_TYPE r,sum=0,rmin,ProfileCenterR,ProfileCenterT,TempConst,tmin;
//	DATA_TYPE t;
	//DATA_TYPE OffsetR,OffsetT,Left;
	DATA_TYPE ***H;
	DATA_TYPE r0 = -(BEAM_WIDTH)/2;
//	DATA_TYPE t0 = -(BEAM_WIDTH)/2;
	DATA_TYPE StepSize = BEAM_WIDTH/BEAM_RESOLUTION;
	int16_t i,j,k,p,ProfileIndex;
//	int16_t l,NumOfDisplacements;

	//NumOfDisplacements=32;
	H = (DATA_TYPE***)get_3D(1, m_Sinogram->N_theta,DETECTOR_RESPONSE_BINS, sizeof(DATA_TYPE));//change from 1 to DETECTOR_RESPONSE_BINS
	TempConst=(PROFILE_RESOLUTION)/(2*m_Inputs->delta_xz);

	for(k = 0 ; k < m_Sinogram->N_theta; k++)
	{
		for (i = 0; i < DETECTOR_RESPONSE_BINS; i++) //displacement along r
		{
			ProfileCenterR = i*OffsetR;
			rmin = ProfileCenterR - m_Inputs->delta_xz;
			for (j = 0 ; j < 1; j++)//displacement along t ;change to DETECTOR_RESPONSE_BINS later
			{
				ProfileCenterT = j*OffsetT;
				tmin = ProfileCenterT - m_Inputs->delta_xy/2;
				sum = 0;
				for (p=0; p < BEAM_RESOLUTION; p++)
				{
					r = r0 + p*StepSize;
					if(r < rmin)
						continue;

					ProfileIndex = (int32_t)floor((r-rmin)*TempConst);
					if(ProfileIndex < 0)
						ProfileIndex = 0;
					if(ProfileIndex >= PROFILE_RESOLUTION)
						ProfileIndex = PROFILE_RESOLUTION-1;

					//for (l =0; l < BEAM_RESOLUTION; l++)
					//{
					//t = t0 + l*StepSize;
					//if( t < tmin)
					//   continue;
					sum += (VoxelProfile[k][ProfileIndex] * BeamProfile[p]);//;*BeamProfile[l]);
					//}

				}
				H[j][k][i] = sum;

			}
		}
	}
	printf("Detector Done\n");
	fwrite(&H[0][0][0], sizeof(DATA_TYPE),m_Sinogram->N_theta*DETECTOR_RESPONSE_BINS, Fp);
	fclose(Fp);
	return H;
}

void* SOCEngine::calculateAMatrixColumnPartial(uint16_t row,uint16_t col, uint16_t slice, DATA_TYPE*** DetectorResponse)
{
	int32_t j,k,sliceidx;
	DATA_TYPE x,z,y;
	DATA_TYPE r;//this is used to find where does the ray passing through the voxel at certain angle hit the detector
	DATA_TYPE t; //this is similar to r but along the y direction
	DATA_TYPE tmin,tmax;
	DATA_TYPE rmax,rmin;//stores the start and end points of the pixel profile on the detector
	DATA_TYPE R_Center,TempConst,checksum = 0,delta_r;
//	DATA_TYPE Integral = 0;
	DATA_TYPE T_Center,delta_t;
	DATA_TYPE MaximumSpacePerColumn;//we will use this to allocate space
	DATA_TYPE AvgNumXElements,AvgNumYElements;//This is a measure of the expected amount of space per Amatrixcolumn. We will make a overestimate to avoid seg faults
//	DATA_TYPE ProfileThickness,stepsize;

	//interpolation variables
	DATA_TYPE w1,w2,w3,w4,f1,InterpolatedValue,ContributionAlongT;
//	DATA_TYPE f2;
	int32_t index_min,index_max,slice_index_min,slice_index_max,index_delta_r,index_delta_t;//stores the detector index in which the profile lies
	int32_t BaseIndex,FinalIndex;
//	int32_t ProfileIndex=0;
//	int32_t NumOfDisplacements=32;
	uint32_t count = 0;

  sliceidx = 0;

	AMatrixCol* Ai = (AMatrixCol*)get_spc(1,sizeof(AMatrixCol));
	AMatrixCol* Temp = (AMatrixCol*)get_spc(1,sizeof(AMatrixCol));//This will assume we have a total of N_theta*N_x entries . We will freeuname -m this space at the end

	x = m_Geometry->x0 + ((DATA_TYPE)col+0.5)*m_Inputs->delta_xz;//0.5 is for center of voxel. x_0 is the left corner
	z = m_Geometry->z0 + ((DATA_TYPE)row+0.5)*m_Inputs->delta_xz;//0.5 is for center of voxel. x_0 is the left corner
	y = m_Geometry->y0 + ((DATA_TYPE)slice + 0.5)*m_Inputs->delta_xy;

	TempConst=(PROFILE_RESOLUTION)/(2*m_Inputs->delta_xz);

	//alternately over estimate the maximum size require for a single AMatrix column
	AvgNumXElements = ceil(3*m_Inputs->delta_xz/m_Sinogram->delta_r);
	AvgNumYElements = ceil(3*m_Inputs->delta_xy/m_Sinogram->delta_t);
	MaximumSpacePerColumn = (AvgNumXElements * AvgNumYElements)*m_Sinogram->N_theta;

	Temp->values = (DATA_TYPE*)get_spc((uint32_t)MaximumSpacePerColumn,sizeof(DATA_TYPE));
	Temp->index  = (uint32_t*)get_spc((uint32_t)MaximumSpacePerColumn,sizeof(uint32_t));


#ifdef AREA_WEIGHTED
	for(uint32_t i=0;i<m_Sinogram->N_theta;i++)
	{

		r = x*cosine[i] - z*sine[i];
		t = y;

		rmin = r - m_Inputs->delta_xz;
		rmax = r + m_Inputs->delta_xz;

		tmin = (t - m_Inputs->delta_xy/2) > m_Sinogram->T0 ? t-m_Inputs->delta_xy/2 : m_Sinogram->T0;
		tmax = (t + m_Inputs->delta_xy/2) <= m_Sinogram->TMax ? t + m_Inputs->delta_xy/2 : m_Sinogram->TMax;

		if(rmax < m_Sinogram->R0 || rmin > m_Sinogram->RMax)
			continue;



		index_min = floor(((rmin - m_Sinogram->R0)/m_Sinogram->delta_r));
		index_max = floor((rmax - m_Sinogram->R0)/m_Sinogram->delta_r);


		if(index_max >= m_Sinogram->N_r)
			index_max = m_Sinogram->N_r - 1;

		if(index_min < 0)
			index_min = 0;

		slice_index_min = floor((tmin - m_Sinogram->T0)/m_Sinogram->delta_t);
		slice_index_max = floor((tmax - m_Sinogram->T0)/m_Sinogram->delta_t);

		if(slice_index_min < 0)
			slice_index_min = 0;
		if(slice_index_max >= m_Sinogram->N_t)
			slice_index_max = m_Sinogram->N_t -1;

		BaseIndex = i*m_Sinogram->N_r;//*Sinogram->N_t;

		for(j = index_min;j <= index_max; j++)//Check
		{

			//Accounting for Beam width
			R_Center = (m_Sinogram->R0 + (((DATA_TYPE)j) + 0.5) *(m_Sinogram->delta_r));//the 0.5 is to get to the center of the detector

			//Find the difference between the center of detector and center of projection and compute the Index to look up into
			delta_r = fabs(r - R_Center);
			index_delta_r = floor((delta_r/OffsetR));


			if (index_delta_r >= 0 && index_delta_r < DETECTOR_RESPONSE_BINS)
			{

		//		for (sliceidx = slice_index_min; sliceidx <= slice_index_max; sliceidx++)
		//		{
					T_Center = (m_Sinogram->T0 + (((DATA_TYPE)sliceidx) + 0.5) *(m_Sinogram->delta_t));
					delta_t = fabs(t - T_Center);
					index_delta_t = 0;//floor(delta_t/OffsetT);



					if (index_delta_t >= 0 && index_delta_t < DETECTOR_RESPONSE_BINS)
					{

						//Using index_delta_t,index_delta_t+1,index_delta_r and index_delta_r+1 do bilinear interpolation
						w1 = delta_r - index_delta_r*OffsetR;
						w2 = (index_delta_r+1)*OffsetR - delta_r;

						w3 = delta_t - index_delta_t*OffsetT;
						w4 = (index_delta_r+1)*OffsetT - delta_t;


						f1 = (w2/OffsetR)*DetectorResponse[index_delta_t][i][index_delta_r] + (w1/OffsetR)*DetectorResponse[index_delta_t][i][index_delta_r+1 < DETECTOR_RESPONSE_BINS ? index_delta_r+1:DETECTOR_RESPONSE_BINS-1];
						//	f2 = (w2/OffsetR)*DetectorResponse[index_delta_t+1 < DETECTOR_RESPONSE_BINS ?index_delta_t+1 : DETECTOR_RESPONSE_BINS-1][i][index_delta_r] + (w1/OffsetR)*DetectorResponse[index_delta_t+1 < DETECTOR_RESPONSE_BINS? index_delta_t+1:DETECTOR_RESPONSE_BINS][i][index_delta_r+1 < DETECTOR_RESPONSE_BINS? index_delta_r+1:DETECTOR_RESPONSE_BINS-1];

						if(sliceidx == slice_index_min)
							ContributionAlongT = (sliceidx + 1)*m_Sinogram->delta_t - tmin;
						else if(sliceidx == slice_index_max)
							ContributionAlongT = tmax - (sliceidx)*m_Sinogram->delta_t;
						else {
							ContributionAlongT = m_Sinogram->delta_t;
						}


					    InterpolatedValue = f1;//*ContributionAlongT;//(w3/OffsetT)*f2 + (w4/OffsetT)*f2;
						if(InterpolatedValue > 0)
						{
							FinalIndex = BaseIndex + (int32_t)j ;//+ (int32_t)sliceidx * Sinogram->N_r;
							Temp->values[count] = InterpolatedValue;//DetectorResponse[index_delta_t][i][index_delta_r];
							Temp->index[count] = FinalIndex;//can instead store a triple (row,col,slice) for the sinogram
							count++;
						}
					}


				//}
			}


		}



	}

#endif


	Ai->values=(DATA_TYPE*)get_spc(count,sizeof(DATA_TYPE));
	Ai->index=(uint32_t*)get_spc(count,sizeof(uint32_t));
	k=0;
	for(uint32_t i = 0; i < count; i++)
	{
		if(Temp->values[i] > 0.0)
		{
			Ai->values[k]=Temp->values[i];
			checksum+=Ai->values[k];
			Ai->index[k++]=Temp->index[i];
		}

	}

	Ai->count=k;


	free(Temp->values);
	free(Temp->index);
	free(Temp);
	return Ai;
}


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
double SOCEngine::surrogateFunctionBasedMin()
{
	double numerator_sum=0;
	double denominator_sum=0;
	double alpha,update=0;
	double product=1;
	uint8_t i,j,k;

	for (i = 0;i < 3;i++)
		for (j = 0; j <3; j++)
			for (k = 0;k < 3; k++)
			{
				if( V != NEIGHBORHOOD[i][j][k])
				{
				product=((double)FILTER[i][j][k]*pow(fabs(V-NEIGHBORHOOD[i][j][k]),(MRF_P-2.0)));
				numerator_sum+=(product*(V-NEIGHBORHOOD[i][j][k]));
				denominator_sum+=product;
				}
			}
	numerator_sum/=SIGMA_X_P;
	denominator_sum/=SIGMA_X_P;

	numerator_sum+=THETA1;
	denominator_sum+=THETA2;

	if(THETA2 > 0)
	{
	  alpha=(-1*numerator_sum)/(denominator_sum);
	  update = V+Clip(alpha, -V, std::numeric_limits<float>::infinity() );
	}
	else
	{
		update=0;
	}

	if(update > 70000)
		printf("%lf\n",update);

	return update;

}


// -----------------------------------------------------------------------------
//Finds the maximum of absolute value elements in an array
// -----------------------------------------------------------------------------
DATA_TYPE SOCEngine::absMaxArray(std::vector<DATA_TYPE> &Array)
{
  uint16_t i;
  DATA_TYPE max;
  max = fabs(Array[0]);
  for(i =1; i < Array.size();i++)
    if(fabs(Array[i]) > max)
      max=fabs(Array[i]);
  return max;

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::calculateGeometricMeanConstraint(ScaleOffsetParams* NuisanceParams)
{
  DATA_TYPE low,high,dist;
  DATA_TYPE perturbation=1e-30;//perturbs the rooting range
  uint16_t rooting_attempt_counter;
  int32_t errorcode=-1;
  DATA_TYPE LagrangeMultiplier;
  DATA_TYPE LambdaRootingAccuracy=1e-10;//accuracy for rooting Lambda
  uint16_t MaxNumRootingAttempts = 1000;//for lambda this corresponds to a distance of 2^1000
  DATA_TYPE* root;
  DATA_TYPE a,b;
  DATA_TYPE temp_mu;

  //Root the expression using the derived quadratic parameters. Need to choose min and max values
     printf("Rooting the equation to solve for the optimal lagrange multiplier\n");

     if(high != (DATA_TYPE)INT64_MAX)
     {
       high-=perturbation; //Since the high value is set to make all discriminants exactly >=0 there are some issues when it is very close due to round off issues. So we get sqrt(-6e-20) for example. So subtract an arbitrary value like 0.5
       //we need to find a window within which we need to root the expression . the upper bound is clear but lower bound we need to look for one
       //low=high;
       dist=-1;
     }
     else if (low != (DATA_TYPE)INT64_MIN)
     {
       low +=perturbation;
       //high=low;
       dist=1;
     }

     /*  fa=CE_ConstraintEquation(high);
      signa=fa>0;
      fb=CE_ConstraintEquation(low);
      signb = fb>0;
      dist=1;
      while (signb != signa )
      {
      low = low - dist;
      fb=CE_ConstraintEquation(low);
      signb = fb>0;
      dist*=2;
      }*/
     rooting_attempt_counter=0;
     errorcode=-1;
     CE_ConstraintEquation ce(NumOfViews, QuadraticParameters, d1, d2, Qk_cost, bk_cost, ck_cost, LogGain);

     while(errorcode != 0 && low <= high && rooting_attempt_counter < MaxNumRootingAttempts) //0 implies the signof the function at low and high is the same
     {

       LagrangeMultiplier=solve<CE_ConstraintEquation>(&ce, low, high, LambdaRootingAccuracy, &errorcode);
       low=low+dist;
       dist*=2;
       rooting_attempt_counter++;
     }

     //Something went wrong and the algorithm was unable to bracket the root within the given interval
     if(rooting_attempt_counter == MaxNumRootingAttempts && errorcode != 0)
     {
       printf("The rooting for lambda was unsuccesful\n");
       printf("Low = %lf High = %lf\n",low,high);
       printf("Quadratic Parameters\n");
       for(int i_theta =0; i_theta < m_Sinogram->N_theta;i_theta++)
       {
         printf("%lf %lf %lf\n",QuadraticParameters[i_theta][0],QuadraticParameters[i_theta][1],QuadraticParameters[i_theta][2]);
       }
 #ifdef DEBUG_CONSTRAINT_OPT

       //for(i_theta =0; i_theta < Sinogram->N_theta;i_theta++)
       //{
       fwrite(&Qk_cost[0][0], sizeof(DATA_TYPE), m_Sinogram->N_theta*3, Fp8);
       //}
       //for(i_theta =0; i_theta < Sinogram->N_theta;i_theta++)
       //{
       fwrite(&bk_cost[0][0], sizeof(DATA_TYPE), m_Sinogram->N_theta*2, Fp8);
       //}
       fwrite(&ck_cost[0], sizeof(DATA_TYPE), m_Sinogram->N_theta, Fp8);
 #endif

       return;
     }

     CE_ConstraintEquation conEqn(NumOfViews, QuadraticParameters, d1, d2, Qk_cost, bk_cost, ck_cost, LogGain);

     //Based on the optimal lambda compute the optimal mu and I0 values
     for (int i_theta =0; i_theta < m_Sinogram->N_theta; i_theta++)
     {

       root = conEqn.CE_RootsOfQuadraticFunction(QuadraticParameters[i_theta][0],QuadraticParameters[i_theta][1],LagrangeMultiplier); //returns the 2 roots of the quadratic parameterized by a,b,c
       a=root[0];
       b=root[0];
       if(root[0] >= 0 && root[1] >= 0)
       {
         temp_mu = d1[i_theta] - root[0]*d2[i_theta]; //for a given lambda we can calculate I0(\lambda) and hence mu(lambda)
         a = (Qk_cost[i_theta][0]*root[0]*root[0] + 2*Qk_cost[i_theta][1]*root[0]*temp_mu + temp_mu*temp_mu*Qk_cost[i_theta][2] - 2*(bk_cost[i_theta][0]*root[0] + temp_mu*bk_cost[i_theta][1]) + ck_cost[i_theta]);//evaluating the cost function

         temp_mu = d1[i_theta] - root[1]*d2[i_theta];//for a given lambda we can calculate I0(\lambda) and hence mu(lambda)
         b = (Qk_cost[i_theta][0]*root[1]*root[1] + 2*Qk_cost[i_theta][1]*root[1]*temp_mu + temp_mu*temp_mu*Qk_cost[i_theta][2] - 2*(bk_cost[i_theta][0]*root[1] + temp_mu*bk_cost[i_theta][1]) + ck_cost[i_theta]);//evaluating the cost function
       }

       if(a == Minimum(a, b))
       NuisanceParams->I_0[i_theta] = root[0];
       else
       {
         NuisanceParams->I_0[i_theta] = root[1];
       }

       free(root); //freeing the memory holding the roots of the quadratic equation

       NuisanceParams->mu[i_theta] = d1[i_theta] - d2[i_theta]*NuisanceParams->I_0[i_theta];//some function of I_0[i_theta]
     }
}
