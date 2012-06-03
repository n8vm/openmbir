/* ============================================================================
 * Copyright (c) 2012 Michael A. Jackson (BlueQuartz Software)
 * Copyright (c) 2012 Singanallur Venkatakrishnan (Purdue University)
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

#include "SOCEngine.h"

// Read the Input data from the supplied data file
// We are scoping here so the various readers are automatically cleaned up before
// the code goes any farther
int SOCEngine::readInputData()
{
  TomoFilter::Pointer dataReader = TomoFilter::NullPointer();
  std::string extension = MXAFileInfo::extension(m_TomoInputs->sinoFile);
  if(extension.compare("mrc") == 0 || extension.compare("ali") == 0)
  {
    dataReader = MRCSinogramInitializer::NewTomoFilter();
  }
  else if(extension.compare("bin") == 0)
  {
    dataReader = RawSinogramInitializer::NewTomoFilter();
  }
  else
  {
    setErrorCondition(-1);
    notify("A supported file reader for the input file was not found.", 100, Observable::UpdateProgressValueAndMessage);
    return -1;
  }
  dataReader->setTomoInputs(m_TomoInputs);
  dataReader->setSinogram(m_Sinogram);
  dataReader->setObservers(getObservers());
  dataReader->execute();
  if(dataReader->getErrorCondition() < 0)
  {
    notify("Error reading Input Sinogram Data file", 100, Observable::UpdateProgressValueAndMessage);
    setErrorCondition(dataReader->getErrorCondition());
    return -1;
  }
  return 0;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int SOCEngine::initializeBrightFieldData()
{

  if(m_BFTomoInputs.get() != NULL && m_BFSinogram.get() != NULL && m_BFTomoInputs->sinoFile.empty() == false)
  {
    std::cout << "Initializing BF data" << std::endl;
    TomoFilter::Pointer dataReader = TomoFilter::NullPointer();
    std::string extension = MXAFileInfo::extension(m_BFTomoInputs->sinoFile);
    if(extension.compare("mrc") == 0 || extension.compare("ali") == 0)
    {
      dataReader = MRCSinogramInitializer::NewTomoFilter();
    }
    else
    {
      setErrorCondition(-1);
      notify("A supported file reader for the Bright Field file was not found.", 100, Observable::UpdateProgressValueAndMessage);
      return -1;
    }
    dataReader->setTomoInputs(m_BFTomoInputs);
    dataReader->setSinogram(m_BFSinogram);
    dataReader->setObservers(getObservers());
    dataReader->execute();
    if(dataReader->getErrorCondition() < 0)
    {
      notify("Error reading Input Sinogram Data file", 100, Observable::UpdateProgressValueAndMessage);
      setErrorCondition(dataReader->getErrorCondition());
      return -1;
    }

    //Normalize the HAADF image
    for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
    { //slice index
      for (uint16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++)
      {
        for (uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
        {
          //1000 is for Marc De Graef data which needed to multiplied
        //  Real_t ttmp = (m_BFSinogram->counts->getValue(i_theta, i_r, i_t) * 1000);
        //  m_Sinogram->counts->divideByValue(ttmp, i_theta, i_r, i_t);
          //100 is for Marc De Graef data which needed to multiplied
          m_BFSinogram->counts->multiplyByValue(100, i_theta, i_r, i_t);
        }
      }
    }

    m_Sinogram->BF_Flag = true;
    notify("BF initialization complete", 0, Observable::UpdateProgressMessage);

  }
  else
  {
    m_Sinogram->BF_Flag = false;
  }
  return 0;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int SOCEngine::createInitialGainsData()
{
  /* ********************* Initialize the Gains Array **************************/
  size_t gains_dims[1] =
  { m_Sinogram->N_theta };
  m_Sinogram->InitialGain = RealArrayType::New(gains_dims, "sinogram->InitialGain");
  if(m_TomoInputs->gainsInputFile.empty() == false)
  {
    // Read the initial Gains from a File
    NuisanceParamReader::Pointer gainsInitializer = NuisanceParamReader::New();
    gainsInitializer->setFileName(m_TomoInputs->gainsInputFile);
    gainsInitializer->setData(m_Sinogram->InitialGain);
    gainsInitializer->setSinogram(m_Sinogram);
    gainsInitializer->setTomoInputs(m_TomoInputs);
    gainsInitializer->setGeometry(m_Geometry);
    gainsInitializer->setObservers(getObservers());
    gainsInitializer->execute();
    if(gainsInitializer->getErrorCondition() < 0)
    {
      notify("Error initializing Input Gains from Data file", 100, Observable::UpdateProgressValueAndMessage);
      setErrorCondition(gainsInitializer->getErrorCondition());
      return -1;
    }
  }
  else
  {
    // Set the values to the target gain value set by the user
    for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
    {
      m_Sinogram->InitialGain->d[i_theta] = m_TomoInputs->targetGain;
    }
  }
  /********************REMOVE************************/
  std::cout << "HARD WIRED TARGET GAIN" << std::endl;
  m_Sinogram->targetGain = m_TomoInputs->targetGain; //TARGET_GAIN;
  std::cout << "Target Gain: " << m_Sinogram->targetGain << std::endl;
  /*************************************************/

  return 0;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int SOCEngine::createInitialOffsetsData()
{
  /* ********************* Initialize the Offsets Array **************************/
  size_t offsets_dims[1] =
  { m_Sinogram->N_theta };
  m_Sinogram->InitialOffset = RealArrayType::New(offsets_dims, "sinogram->InitialOffset");
  if(m_TomoInputs->offsetsInputFile.empty() == false)
  {
    // Read the initial offsets from a File
    NuisanceParamReader::Pointer offsetsInitializer = NuisanceParamReader::New();
    offsetsInitializer->setFileName(m_TomoInputs->offsetsInputFile);
    offsetsInitializer->setData(m_Sinogram->InitialOffset);
    offsetsInitializer->setSinogram(m_Sinogram);
    offsetsInitializer->setTomoInputs(m_TomoInputs);
    offsetsInitializer->setGeometry(m_Geometry);
    offsetsInitializer->setObservers(getObservers());
    offsetsInitializer->execute();
    if(offsetsInitializer->getErrorCondition() < 0)
    {
      notify("Error initializing Input Offsets from Data file", 100, Observable::UpdateProgressValueAndMessage);
      setErrorCondition(offsetsInitializer->getErrorCondition());
      return -1;
    }
  }
  else if(m_TomoInputs->useDefaultOffset == true)
  {
    // Set the values to the default offset value set by the user
    for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
    {
      m_Sinogram->InitialOffset->d[i_theta] = m_TomoInputs->defaultOffset;
    }
  }
  else
  {
    // Compute the initial offset values from the data
    ComputeInitialOffsets::Pointer initializer = ComputeInitialOffsets::New();
    initializer->setSinogram(m_Sinogram);
    initializer->setTomoInputs(m_TomoInputs);
    initializer->setObservers(getObservers());
    initializer->execute();
    if(initializer->getErrorCondition() < 0)
    {
      notify("Error initializing Input Offsets Data file", 100, Observable::UpdateProgressValueAndMessage);
      setErrorCondition(initializer->getErrorCondition());
      return -1;
    }
  }
  return 0;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int SOCEngine::createInitialVariancesData()
{

  /* ********************* Initialize the Variances Array **************************/
  size_t variance_dims[1] =
  { m_Sinogram->N_theta };
  m_Sinogram->InitialVariance = RealArrayType::New(variance_dims, "sinogram->InitialVariance");
  if(m_TomoInputs->varianceInputFile.empty() == false)
  {
    // Read the initial variances from a File
    NuisanceParamReader::Pointer variancesInitializer = NuisanceParamReader::New();
    variancesInitializer->setFileName(m_TomoInputs->varianceInputFile);
    variancesInitializer->setData(m_Sinogram->InitialVariance);
    variancesInitializer->setSinogram(m_Sinogram);
    variancesInitializer->setTomoInputs(m_TomoInputs);
    variancesInitializer->setGeometry(m_Geometry);
    variancesInitializer->setObservers(getObservers());
    variancesInitializer->execute();
    if(variancesInitializer->getErrorCondition() < 0)
    {
      notify("Error initializing Input Variances from Data file", 100, Observable::UpdateProgressValueAndMessage);
      setErrorCondition(variancesInitializer->getErrorCondition());
      return -1;
    }
  }
  else
  {
    //  std::cout << "------------Initial Variance-----------" << std::endl;
    for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
    {
		m_Sinogram->InitialVariance->d[i_theta] = m_TomoInputs->defaultVariance;
       std::cout << "Tilt: " << i_theta << "  Variance: " << m_Sinogram->InitialVariance->d[i_theta] << std::endl;
    }
  }

  return 0;
}

// -----------------------------------------------------------------------------
// Initialize the Geometry data from a rough reconstruction
// -----------------------------------------------------------------------------
int SOCEngine::initializeRoughReconstructionData()
{
  InitialReconstructionInitializer::Pointer geomInitializer = InitialReconstructionInitializer::NullPointer();
  std::string extension = MXAFileInfo::extension(m_TomoInputs->initialReconFile);
  if (m_TomoInputs->initialReconFile.empty() == true)
  {
    // This will just initialize all the values to Zero (0) or a DefaultValue Set by user
    geomInitializer = InitialReconstructionInitializer::New();
  }
  else if (extension.compare("bin") == 0 )
  {
    // This will read the values from a binary file
    geomInitializer = InitialReconstructionBinReader::NewInitialReconstructionInitializer();
  }
  else if (extension.compare(".mrc") == 0)
  {
    notify("We are not dealing with mrc volume files. The program will now end.", 0, Observable::UpdateErrorMessage);
    return -1;
  }
  else
  {
    notify("Could not find a compatible reader for the initial reconstruction data file. The program will now end.", 0, Observable::UpdateErrorMessage);
    return -1;
  }
  geomInitializer->setSinogram(m_Sinogram);
  geomInitializer->setTomoInputs(m_TomoInputs);
  geomInitializer->setGeometry(m_Geometry);
  geomInitializer->setObservers(getObservers());
  geomInitializer->execute();

  if(geomInitializer->getErrorCondition() < 0)
  {
    notify("Error reading Initial Reconstruction Data from File", 100, Observable::UpdateProgressValueAndMessage);
    setErrorCondition(geomInitializer->getErrorCondition());
    return -1;
  }
  return 0;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::initializeROIMask(UInt8Image_t::Pointer Mask)
{
  Real_t x = 0.0;
  Real_t z = 0.0;
  for (uint16_t i = 0; i < m_Geometry->N_z; i++)
  {
    for (uint16_t j = 0; j < m_Geometry->N_x; j++)
    {
      x = m_Geometry->x0 + ((Real_t)j + 0.5) * m_TomoInputs->delta_xz;
      z = m_Geometry->z0 + ((Real_t)i + 0.5) * m_TomoInputs->delta_xz;
      if(x >= -(m_Sinogram->N_r * m_Sinogram->delta_r) / 2 && x <= (m_Sinogram->N_r * m_Sinogram->delta_r) / 2 && z >= -m_TomoInputs->LengthZ / 2
          && z <= m_TomoInputs->LengthZ / 2)
      {
        Mask->setValue(1, i, j);
      }
      else
      {
        Mask->setValue(0, i, j);
      }
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::gainAndOffsetInitialization(ScaleOffsetParamsPtr NuisanceParams)
{
  Real_t sum = 0;
  Real_t temp = 0;
  for (uint16_t k = 0; k < m_Sinogram->N_theta; k++)
  {
    // Gains
    NuisanceParams->I_0->d[k] = m_Sinogram->InitialGain->d[k];
    // Offsets
    NuisanceParams->mu->d[k] = m_Sinogram->InitialOffset->d[k];
#ifdef GEOMETRIC_MEAN_CONSTRAINT
    sum += log(NuisanceParams->I_0->d[k]);
#else
    sum += NuisanceParams->I_0->d[k];
#endif
  }
  sum /= m_Sinogram->N_theta;

#ifdef GEOMETRIC_MEAN_CONSTRAINT
  sum=exp(sum);
  printf("The geometric mean of the gains is %lf\n",sum);

  //Checking if the input parameters satisfy the target geometric mean
  if(fabs(sum - m_Sinogram->targetGain) > 1e-5)
  {
    printf("The input paramters dont meet the constraint..Renormalizing\n");
    temp = exp(log(m_Sinogram->targetGain) - log(sum));
    for (k = 0; k < m_Sinogram->N_theta; k++)
    {
      NuisanceParams->I_0->d[k]=m_Sinogram->InitialGain->d[k]*temp;
    }
  }
#else
  printf("The Arithmetic mean of the constraint is %lf\n", sum);
  if(sum - m_Sinogram->targetGain > 1e-5)
  {
    printf("Arithmetic Mean Constraint not met..renormalizing\n");
    temp = m_Sinogram->targetGain / sum;
    for (uint16_t k = 0; k < m_Sinogram->N_theta; k++)
    {
      NuisanceParams->I_0->d[k] = m_Sinogram->InitialGain->d[k] * temp;
    }
  }
#endif

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::initializeHt(RealVolumeType::Pointer H_t)
{
  Real_t ProfileCenterT;
  for (uint16_t k = 0; k < m_Sinogram->N_theta; k++)
  {
    for (int i = 0; i < DETECTOR_RESPONSE_BINS; i++)
    {
      ProfileCenterT = i * OffsetT;
      if(m_TomoInputs->delta_xy >= m_Sinogram->delta_t)
      {
        if(ProfileCenterT <= ((m_TomoInputs->delta_xy / 2) - (m_Sinogram->delta_t / 2)))
        {
          H_t->setValue(m_Sinogram->delta_t, 0, k, i);
        }
        else
        {
          H_t->setValue(-1 * ProfileCenterT + (m_TomoInputs->delta_xy / 2) + m_Sinogram->delta_t / 2, 0, k, i);
        }
        if(H_t->getValue(0, k, i) < 0)
        {
          H_t->setValue(0, 0, k, i);
        }

      }
      else
      {
        if(ProfileCenterT <= m_Sinogram->delta_t / 2 - m_TomoInputs->delta_xy / 2)
        {
          H_t->setValue(m_TomoInputs->delta_xy, 0, k, i);
        }
        else
        {
          H_t->setValue(-ProfileCenterT + (m_TomoInputs->delta_xy / 2) + m_Sinogram->delta_t / 2, 0, k, i);
        }

        if(H_t->getValue(0, k, i) < 0)
        {
          H_t->setValue(0, 0, k, i);
        }

      }
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::initializeVolume(RealVolumeType::Pointer Y_Est, double value)
{
  for (uint16_t i = 0; i < m_Sinogram->N_theta; i++)
  {
    for (uint16_t j = 0; j < m_Sinogram->N_r; j++)
    {
      for (uint16_t k = 0; k < m_Sinogram->N_t; k++)
      {
        Y_Est->setValue(value, i, j, k);
      }
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::storeVoxelResponse(RealVolumeType::Pointer H_t,  AMatrixCol* VoxelLineResponse)
{
  Real_t ProfileThickness = 0.0;
  Real_t y = 0.0;
  Real_t t = 0.0;
  Real_t tmin;
  Real_t tmax;
  int16_t slice_index_min, slice_index_max;
  Real_t center_t,delta_t;
  int16_t index_delta_t;
  Real_t w3,w4;

  //Storing the response along t-direction for each voxel line
  notify("Storing the response along Y-direction for each voxel line", 0, Observable::UpdateProgressMessage);
  for (uint16_t i =0; i < m_Geometry->N_y; i++)
  {
    y = ((Real_t)i + 0.5) * m_TomoInputs->delta_xy + m_Geometry->y0;
    t = y;
    tmin = (t - m_TomoInputs->delta_xy / 2) > m_Sinogram->T0 ? t - m_TomoInputs->delta_xy / 2 : m_Sinogram->T0;
    tmax = (t + m_TomoInputs->delta_xy / 2) <= m_Sinogram->TMax ? t + m_TomoInputs->delta_xy / 2 : m_Sinogram->TMax;

    slice_index_min = static_cast<uint16_t>(floor((tmin - m_Sinogram->T0) / m_Sinogram->delta_t));
    slice_index_max = static_cast<uint16_t>(floor((tmax - m_Sinogram->T0) / m_Sinogram->delta_t));

    if(slice_index_min < 0)
    {
      slice_index_min = 0;
    }
    if(slice_index_max >= m_Sinogram->N_t)
    {
      slice_index_max = m_Sinogram->N_t - 1;
    }

    //printf("%d %d\n",slice_index_min,slice_index_max);

    for (int i_t = slice_index_min; i_t <= slice_index_max; i_t++)
    {
      center_t = ((Real_t)i_t + 0.5) * m_Sinogram->delta_t + m_Sinogram->T0;
      delta_t = fabs(center_t - t);
      index_delta_t = static_cast<uint16_t>(floor(delta_t / OffsetT));
      if(index_delta_t < DETECTOR_RESPONSE_BINS)
      {
        w3 = delta_t - (Real_t)(index_delta_t) * OffsetT;
        w4 = ((Real_t)index_delta_t + 1) * OffsetT - delta_t;
        uint16_t ttmp = index_delta_t + 1 < DETECTOR_RESPONSE_BINS ? index_delta_t + 1 : DETECTOR_RESPONSE_BINS - 1;
        ProfileThickness = (w4 / OffsetT) * H_t->getValue(0, 0, index_delta_t)
            + (w3 / OffsetT) * H_t->getValue(0, 0, ttmp);
    //  ProfileThickness = (w4 / OffsetT) * detectorResponse->d[0][uint16_t(floor(m_Sinogram->N_theta/2))][index_delta_t]
    //  + (w3 / OffsetT) * detectorResponse->d[0][uint16_t(floor(m_Sinogram->N_theta/2))][index_delta_t + 1 < DETECTOR_RESPONSE_BINS ? index_delta_t + 1 : DETECTOR_RESPONSE_BINS - 1];
    }
      else
      {
        ProfileThickness = 0;
      }

      if(ProfileThickness != 0) //Store the response of this slice
      {
#ifdef DEBUG
        printf("%d %lf\n", i_t, ProfileThickness);
#endif
        VoxelLineResponse[i].values[VoxelLineResponse[i].count] = ProfileThickness;
        VoxelLineResponse[i].index[VoxelLineResponse[i].count++] = i_t;
      }
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::calculateGeometricMeanConstraint(ScaleOffsetParams* NuisanceParams)
{

  Real_t low,high,dist;
  Real_t perturbation=1e-30;//perturbs the rooting range
  uint16_t rooting_attempt_counter;
  int32_t errorcode=-1;
  Real_t LagrangeMultiplier;
  Real_t LambdaRootingAccuracy=1e-10;//accuracy for rooting Lambda
  uint16_t MaxNumRootingAttempts = 1000;//for lambda this corresponds to a distance of 2^1000
  Real_t* root;
  Real_t a,b;
  Real_t temp_mu;

//TODO: Fix these initializations
  high = 0;
  low = 0;
  dist = 0;
  //Root the expression using the derived quadratic parameters. Need to choose min and max values
     printf("Rooting the equation to solve for the optimal Lagrange multiplier\n");

     if(high != std::numeric_limits<Real_t>::max())
     {
       high-=perturbation; //Since the high value is set to make all discriminants exactly >=0 there are some issues when it is very close due to round off issues. So we get sqrt(-6e-20) for example. So subtract an arbitrary value like 0.5
       //we need to find a window within which we need to root the expression . the upper bound is clear but lower bound we need to look for one
       //low=high;
       dist=-1;
     }
     else if (low != std::numeric_limits<Real_t>::min())
     {
       low +=perturbation;
       //high=low;
       dist=1;
     }

     rooting_attempt_counter=0;
     errorcode=-1;
     CE_ConstraintEquation ce(NumOfViews, QuadraticParameters, d1, d2, Qk_cost, bk_cost, ck_cost, LogGain);

     while(errorcode != 0 && low <= high && rooting_attempt_counter < MaxNumRootingAttempts) //0 implies the signof the function at low and high is the same
     {
       uint32_t iter_count = 0;
       LagrangeMultiplier=solve<CE_ConstraintEquation>(&ce, low, high, LambdaRootingAccuracy, &errorcode,iter_count);
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
         printf("%lf %lf %lf\n",QuadraticParameters->getValue(i_theta, 0),QuadraticParameters->getValue(i_theta, 1),QuadraticParameters->getValue(i_theta, 2));
       }
 #ifdef DEBUG_CONSTRAINT_OPT

       //for(i_theta =0; i_theta < Sinogram->N_theta;i_theta++)
       //{
       fwrite(&Qk_cost->d[0][0], sizeof(Real_t), m_Sinogram->N_theta*3, Fp8);
       //}
       //for(i_theta =0; i_theta < Sinogram->N_theta;i_theta++)
       //{
       fwrite(&bk_cost->d[0][0], sizeof(Real_t), m_Sinogram->N_theta*2, Fp8);
       //}
       fwrite(&ck_cost->d[0], sizeof(Real_t), m_Sinogram->N_theta, Fp8);
 #endif

       return;
     }

     CE_ConstraintEquation conEqn(NumOfViews, QuadraticParameters, d1, d2, Qk_cost, bk_cost, ck_cost, LogGain);

     //Based on the optimal lambda compute the optimal mu and I0 values
     for (int i_theta =0; i_theta < m_Sinogram->N_theta; i_theta++)
     {

       root = conEqn.CE_RootsOfQuadraticFunction(QuadraticParameters->getValue(i_theta, 0),QuadraticParameters->getValue(i_theta, 1),LagrangeMultiplier); //returns the 2 roots of the quadratic parameterized by a,b,c
       a=root[0];
       b=root[0];
       if(root[0] >= 0 && root[1] >= 0)
       {
         temp_mu = d1->d[i_theta] - root[0]*d2->d[i_theta]; //for a given lambda we can calculate I0(\lambda) and hence mu(lambda)
         a = (Qk_cost->getValue(i_theta, 0)*root[0]*root[0] + 2*Qk_cost->getValue(i_theta, 1)*root[0]*temp_mu + temp_mu*temp_mu*Qk_cost->getValue(i_theta, 2) - 2*(bk_cost->getValue(i_theta, 0)*root[0] + temp_mu*bk_cost->getValue(i_theta, 1)) + ck_cost->d[i_theta]);//evaluating the cost function

         temp_mu = d1->d[i_theta] - root[1]*d2->d[i_theta];//for a given lambda we can calculate I0(\lambda) and hence mu(lambda)
         b = (Qk_cost->getValue(i_theta, 0)*root[1]*root[1] + 2*Qk_cost->getValue(i_theta, 1)*root[1]*temp_mu + temp_mu*temp_mu*Qk_cost->getValue(i_theta, 2) - 2*(bk_cost->getValue(i_theta, 0)*root[1] + temp_mu*bk_cost->getValue(i_theta, 1)) + ck_cost->d[i_theta]);//evaluating the cost function
       }

       if(a == Minimum(a, b))
       NuisanceParams->I_0->d[i_theta] = root[0];
       else
       {
         NuisanceParams->I_0->d[i_theta] = root[1];
       }

       free(root); //freeing the memory holding the roots of the quadratic equation

       NuisanceParams->mu->d[i_theta] = d1->d[i_theta] - d2->d[i_theta]*NuisanceParams->I_0->d[i_theta];//some function of I_0[i_theta]
     }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::calculateArithmeticMean()
{

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int SOCEngine::jointEstimation(RealVolumeType::Pointer Weight,
                                ScaleOffsetParamsPtr NuisanceParams,
                                RealVolumeType::Pointer ErrorSino,
                                RealVolumeType::Pointer Y_Est,
                                CostData::Pointer cost)
{
  std::string indent("  ");

  if(m_Sinogram->BF_Flag == false)
  {
  Real_t AverageI_kUpdate=0;//absolute sum of the gain updates
  Real_t AverageMagI_k=0;//absolute sum of the initial gains

  Real_t AverageDelta_kUpdate=0; //absolute sum of the offsets
  Real_t AverageMagDelta_k=0;//abs sum of the initial offset

  Real_t sum = 0;
  int err = 0;
  uint64_t startm, stopm;

  //DATA_TYPE high = std::numeric_limits<DATA_TYPE>::max();
  //DATA_TYPE low = std::numeric_limits<DATA_TYPE>::min();
  //Joint Scale And Offset Estimation

  //forward project
  for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
  {
    for (uint16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++)
    {
      for (uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
      {
    //Y_Est->d[i_theta][i_r][i_t]=0;
        Real_t ttmp = m_Sinogram->counts->getValue(i_theta, i_r, i_t) - ErrorSino->getValue(i_theta, i_r, i_t)-NuisanceParams->mu->d[i_theta];
        Y_Est->setValue(ttmp, i_theta, i_r, i_t);
        Y_Est->divideByValue(NuisanceParams->I_0->d[i_theta], i_theta, i_r, i_t);
      }
    }
  }

  START_TIMER;
  for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
  {
    Real_t a = 0;
    Real_t b = 0;
    Real_t c = 0;
    Real_t d = 0;
//  DATA_TYPE e = 0;
    Real_t numerator_sum = 0;
    Real_t denominator_sum = 0;
//  DATA_TYPE temp = 0.0;

    //compute the parameters of the quadratic for each view
    for (uint16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++)
    {
      for (uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
      {
        size_t counts_idx = m_Sinogram->counts->calcIndex(i_theta, i_r, i_t);
        size_t weight_idx = Weight->calcIndex(i_theta, i_r, i_t);
        size_t yest_idx = Y_Est->calcIndex(i_theta, i_r, i_t);

        numerator_sum += (m_Sinogram->counts->d[counts_idx] * Weight->d[weight_idx]);
        denominator_sum += (Weight->d[weight_idx]);

        a += (Y_Est->d[yest_idx] * Weight->d[weight_idx]);
        b += (Y_Est->d[yest_idx] * Weight->d[weight_idx] * m_Sinogram->counts->d[counts_idx]);
        c += (m_Sinogram->counts->d[counts_idx] * m_Sinogram->counts->d[counts_idx] * Weight->d[weight_idx]);
        d += (Y_Est->d[yest_idx] * Y_Est->d[yest_idx] * Weight->d[weight_idx]);

      }
    }

    bk_cost->setValue(numerator_sum, i_theta, 1); //yt*\lambda*1
    bk_cost->setValue(b, i_theta, 0); //yt*\lambda*(Ax)
    ck_cost->d[i_theta] = c; //yt*\lambda*y
    Qk_cost->setValue(denominator_sum, i_theta, 2);
    Qk_cost->setValue(a, i_theta, 1);
    Qk_cost->setValue(d, i_theta, 0);

    d1->d[i_theta] = numerator_sum / denominator_sum;
    d2->d[i_theta] = a / denominator_sum;

    a = 0;
    b = 0;
    for (uint16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++)
    {
      for (uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
      {
        size_t counts_idx = m_Sinogram->counts->calcIndex(i_theta, i_r, i_t);
        size_t weight_idx = Weight->calcIndex(i_theta, i_r, i_t);
        size_t yest_idx = Y_Est->calcIndex(i_theta, i_r, i_t);

        a += ((Y_Est->d[yest_idx] - d2->d[i_theta]) * Weight->d[weight_idx] * Y_Est->d[yest_idx]);
        b -= ((m_Sinogram->counts->d[counts_idx] - d1->d[i_theta]) * Weight->d[weight_idx] * Y_Est->d[yest_idx]);
      }
    }
    QuadraticParameters->setValue(a, i_theta, 0);
    QuadraticParameters->setValue(b, i_theta, 1);

#if 0
    temp = (QuadraticParameters->getValue(i_theta, 1) * QuadraticParameters->getValue(i_theta, 1)) / (4 * QuadraticParameters->getValue(i_theta, 0));

    if(temp > 0 && temp < high)
    {
      high = temp;
    } //high holds the maximum value for the rooting operation. beyond this value discriminants become negative. Basically high = min{b^2/4*a}
    else if(temp < 0 && temp > low)
    {
      low = temp;
    }
#endif
  }
  STOP_TIMER;
  PRINT_TIME("Joint Estimation Loops Time");

  //compute cost
  /********************************************************************************************/
  sum = 0;
  for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
  {
    sum += (Qk_cost->getValue(i_theta, 0) * NuisanceParams->I_0->d[i_theta] * NuisanceParams->I_0->d[i_theta]
        + 2 * Qk_cost->getValue(i_theta, 1) * NuisanceParams->I_0->d[i_theta] * NuisanceParams->mu->d[i_theta]
        + NuisanceParams->mu->d[i_theta] * NuisanceParams->mu->d[i_theta] * Qk_cost->getValue(i_theta, 2)
        - 2 * (bk_cost->getValue(i_theta, 0) * NuisanceParams->I_0->d[i_theta] + NuisanceParams->mu->d[i_theta] * bk_cost->getValue(i_theta, 1)) + ck_cost->d[i_theta]); //evaluating the cost function
  }
  sum /= 2;
  printf("The value of the data match error prior to updating the I and mu =%lf\n", sum);

  /********************************************************************************************/

#ifdef GEOMETRIC_MEAN_CONSTRAINT
  calculateGeometricMeanConstraint();
#else
  Real_t sum1 = 0;
  Real_t sum2 = 0;
  for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
  {
    sum1 += (1.0 / (Qk_cost->getValue(i_theta, 0) - Qk_cost->getValue(i_theta, 1) * d2->d[i_theta]));
    sum2 += ((bk_cost->getValue(i_theta, 0) - Qk_cost->getValue(i_theta, 1) * d1->d[i_theta]) / (Qk_cost->getValue(i_theta, 0) - Qk_cost->getValue(i_theta, 1) * d2->d[i_theta]));
  }
  Real_t LagrangeMultiplier = (-m_Sinogram->N_theta * m_Sinogram->targetGain + sum2) / sum1;
  for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
  {

    AverageMagI_k += fabs(NuisanceParams->I_0->d[i_theta]); //store the sum of the vector of gains

    Real_t NewI_k = (-1 * LagrangeMultiplier - Qk_cost->getValue(i_theta, 1) * d1->d[i_theta] + bk_cost->getValue(i_theta, 0))
        / (Qk_cost->getValue(i_theta, 0) - Qk_cost->getValue(i_theta, 1) * d2->d[i_theta]);

    AverageI_kUpdate += fabs(NewI_k - NuisanceParams->I_0->d[i_theta]);

    NuisanceParams->I_0->d[i_theta] = NewI_k;
    //Postivity Constraint on the gains

    if(NuisanceParams->I_0->d[i_theta] < 0)
    {
      NuisanceParams->I_0->d[i_theta] *= 1;
    }
    AverageMagDelta_k += fabs(NuisanceParams->mu->d[i_theta]);

    Real_t NewDelta_k = d1->d[i_theta] - d2->d[i_theta] * NuisanceParams->I_0->d[i_theta]; //some function of I_0[i_theta]
    AverageDelta_kUpdate += fabs(NewDelta_k - NuisanceParams->mu->d[i_theta]);
    NuisanceParams->mu->d[i_theta] = NewDelta_k;
    //Postivity Constraing on the offsets

    if(NuisanceParams->mu->d[i_theta] < 0)
    {
      NuisanceParams->mu->d[i_theta] *= 1;
    }
  }
#endif //Type of constraining Geometric or arithmetic

  /********************************************************************************************/
  //checking to see if the cost went down
  sum = 0;
  for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
  {
    sum += (Qk_cost->getValue(i_theta, 0) * NuisanceParams->I_0->d[i_theta] * NuisanceParams->I_0->d[i_theta])
        + (2 * Qk_cost->getValue(i_theta, 1) * NuisanceParams->I_0->d[i_theta] * NuisanceParams->mu->d[i_theta])
        + (NuisanceParams->mu->d[i_theta] * NuisanceParams->mu->d[i_theta] * Qk_cost->getValue(i_theta, 2))
        - (2 * (bk_cost->getValue(i_theta, 0) * NuisanceParams->I_0->d[i_theta] + NuisanceParams->mu->d[i_theta] * bk_cost->getValue(i_theta, 1)) + ck_cost->d[i_theta]); //evaluating the cost function
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
        size_t counts_idx = m_Sinogram->counts->calcIndex(i_theta, i_r, i_t);
        size_t yest_idx = Y_Est->calcIndex(i_theta, i_r, i_t);
        size_t error_idx = ErrorSino->calcIndex(i_theta, i_r, i_t);

        ErrorSino->d[error_idx] = m_Sinogram->counts->d[counts_idx] -
            NuisanceParams->mu->d[i_theta] - (NuisanceParams->I_0->d[i_theta] * Y_Est->d[yest_idx]);
      }
    }
  }


#ifdef COST_CALCULATE
  err = calculateCost(cost, Weight, ErrorSino);
  if (err < 0)
  {
	std::cout<<"Cost went up after Gain+Offset update"<<std::endl;
    return err;
  }
#endif

  printf("Lagrange Multiplier = %lf\n", LagrangeMultiplier);

//#ifdef DEBUG
  std::cout << "Tilt\tGains\tOffsets\tVariance" << std::endl;
  for (uint16_t i_theta = 0; i_theta < getSinogram()->N_theta; i_theta++)
  {
    std::cout << i_theta << "\t" << NuisanceParams->I_0->d[i_theta] <<
        "\t" << NuisanceParams->mu->d[i_theta] <<std::endl;
  }
//#endif

  Real_t I_kRatio=AverageI_kUpdate/AverageMagI_k;
  Real_t Delta_kRatio = AverageDelta_kUpdate/AverageMagDelta_k;
  std::cout<<"Ratio of change in I_k "<<I_kRatio<<std::endl;
  std::cout<<"Ratio of change in Delta_k "<<Delta_kRatio<<std::endl;

  return 0;
	}
	else
	{

		for (uint16_t i_theta=0; i_theta < m_Sinogram->N_theta; i_theta++)
		{
			Real_t num_sum=0;
			Real_t den_sum=0;
			Real_t alpha =0;
			for(uint16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++) {
				for(uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
				{
					num_sum += (ErrorSino->getValue(i_theta, i_r, i_t) * Weight->getValue(i_theta, i_r, i_t) );
					den_sum += Weight->getValue(i_theta, i_r, i_t);
				}
			}
			alpha = num_sum/den_sum;

			for(uint16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++)
				for(uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
				{
					ErrorSino->deleteFromValue(alpha, i_theta, i_r, i_t);
				}

			NuisanceParams->mu->d[i_theta] += alpha;
			std::cout<<NuisanceParams->mu->d[i_theta]<<std::endl;
		}
#ifdef COST_CALCULATE
		/*********************Cost Calculation*************************************/
		Real_t cost_value = computeCost(ErrorSino, Weight);
		std::cout<<cost_value<<std::endl;
		int increase = cost->addCostValue(cost_value);
		if (increase ==1)
		{
			std::cout << "Cost just increased after offset update!" << std::endl;
			//break;
			return -1;
		}
		cost->writeCostValue(cost_value);
		/**************************************************************************/
#endif
		return 0;
	} //BFflag = true

}
// -----------------------------------------------------------------------------
// Calculate Error Sinogram
// Also compute weights of the diagonal covariance matrix
// -----------------------------------------------------------------------------

void SOCEngine::calculateMeasurementWeight(RealVolumeType::Pointer Weight,
                                           ScaleOffsetParamsPtr NuisanceParams,
                                           RealVolumeType::Pointer ErrorSino,
                                           RealVolumeType::Pointer Y_Est)
{
  Real_t checksum = 0;
  START_TIMER;
  for (int16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++) //slice index
  {
#ifdef NOISE_MODEL
    {
		NuisanceParams->alpha->d[i_theta] = m_Sinogram->InitialVariance->d[i_theta]; //Initialize the refinement parameters from any previous run
    }
#endif //Noise model
    checksum = 0;
    for (int16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++)
    {
      for (uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
      {

        size_t counts_idx = m_Sinogram->counts->calcIndex(i_theta, i_r, i_t);
        size_t weight_idx = Weight->calcIndex(i_theta, i_r, i_t);
        size_t yest_idx = Y_Est->calcIndex(i_theta, i_r, i_t);
        size_t error_idx = ErrorSino->calcIndex(i_theta, i_r, i_t);

        if(m_Sinogram->BF_Flag == false)
        {
          ErrorSino->d[error_idx] = m_Sinogram->counts->d[counts_idx] - Y_Est->d[yest_idx] - NuisanceParams->mu->d[i_theta];
        }
        else
        {
          size_t bfcounts_idx = m_BFSinogram->counts->calcIndex(i_theta, i_r, i_t);

          ErrorSino->d[error_idx] = m_Sinogram->counts->d[counts_idx] - m_BFSinogram->counts->d[bfcounts_idx] * Y_Est->d[yest_idx]
              - NuisanceParams->mu->d[i_theta];
        }


#ifndef IDENTITY_NOISE_MODEL
        if(m_Sinogram->counts->d[counts_idx] != 0)
        {
          Weight->d[weight_idx] = 1.0 / m_Sinogram->counts->d[counts_idx];
        }
        else
        {
          Weight->d[weight_idx] = 1.0; //Set the weight to some small number
      //TODO: Make this something resonable
        }
#else
      Weight->d[i_theta][i_r][i_t] = 1.0;
#endif //IDENTITY_NOISE_MODEL endif

#ifdef FORWARD_PROJECT_MODE
        temp=Y_Est->d[i_theta][i_r][i_t]/NuisanceParams->I_0->d[i_theta];
        fwrite(&temp,sizeof(Real_t),1,Fp6);
#endif
#ifdef DEBUG
        if(Weight->d[weight_idx] < 0)
        {
          std::cout << m_Sinogram->counts->d[counts_idx] << "    " << NuisanceParams->alpha->d[i_theta] << std::endl;
        }
#endif//Debug

#ifdef NOISE_MODEL
          {
        Weight->d[weight_idx] /= NuisanceParams->alpha->d[i_theta];
          }
#endif

        checksum += Weight->d[weight_idx];
      }
    }
#ifdef DEBUG
    printf("Check sum of Diagonal Covariance Matrix= %lf\n", checksum);
#endif
  }
  STOP_TIMER;
  std::cout << std::endl;
  std::string indent(" ");
  PRINT_TIME("Computing Weights");
}


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int SOCEngine::calculateCost(CostData::Pointer cost,
                             RealVolumeType::Pointer Weight,
                             RealVolumeType::Pointer ErrorSino)
{
  Real_t cost_value = computeCost(ErrorSino, Weight);
  std::cout << "cost_value: " << cost_value << std::endl;
  int increase = cost->addCostValue(cost_value);
  if(increase == 1)
  {
    return -1;
  }
  cost->writeCostValue(cost_value);
  return 0;
}

// -----------------------------------------------------------------------------
// Updating the Weights for Noise Model
// -----------------------------------------------------------------------------
void SOCEngine::updateWeights(RealVolumeType::Pointer Weight,
                              ScaleOffsetParamsPtr NuisanceParams,
                              RealVolumeType::Pointer ErrorSino)
{
  Real_t AverageVarUpdate = 0; //absolute sum of the gain updates
  Real_t AverageMagVar = 0; //absolute sum of the initial gains
  Real_t sum = 0;

  for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
  {
    sum = 0;
    //Factoring out the variance parameter from the Weight matrix
    for (uint16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++)
    {
      for (uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
      {
        size_t weight_idx = Weight->calcIndex(i_theta, i_r, i_t);
   //     size_t yest_idx = Y_Est->calcIndex(i_theta, i_r, i_t);
    //    size_t error_idx = ErrorSino->calcIndex(i_theta, i_r, i_t);
#ifndef IDENTITY_NOISE_MATRIX
        size_t counts_idx = m_Sinogram->counts->calcIndex(i_theta, i_r, i_t);
        if(m_Sinogram->counts->d[counts_idx] != 0)
        {
          Weight->d[weight_idx] = 1.0 / m_Sinogram->counts->d[counts_idx];
        }
        else
        {
          Weight->d[weight_idx] = 1.0;
        }
#else
        Weight->d[i_theta][i_r][i_t] = 1.0;
#endif//Identity noise Model
      }
    }


    for (uint16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++)
    {
      for (uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
      {
        size_t error_idx = ErrorSino->calcIndex(i_theta, i_r, i_t);
        size_t weight_idx = Weight->calcIndex(i_theta, i_r, i_t);
        sum += (ErrorSino->d[error_idx] * ErrorSino->d[error_idx] * Weight->d[weight_idx]); //Changed to only account for the counts
      }
    }
    sum /=(m_Sinogram->N_r*m_Sinogram->N_t);

    AverageMagVar += fabs(NuisanceParams->alpha->d[i_theta]);
    AverageVarUpdate += fabs(sum - NuisanceParams->alpha->d[i_theta]);
    NuisanceParams->alpha->d[i_theta] = sum;
    //Update the weight for ICD updates
    for (uint16_t i_r = 0; i_r < m_Sinogram->N_r; i_r++)
    {
      for (uint16_t i_t = 0; i_t < m_Sinogram->N_t; i_t++)
      {
#ifndef IDENTITY_NOISE_MATRIX
        size_t counts_idx = m_Sinogram->counts->calcIndex(i_theta, i_r, i_t);
        size_t weight_idx = Weight->calcIndex(i_theta, i_r, i_t);

        if(NuisanceParams->alpha->d[i_theta] != 0 && m_Sinogram->counts->d[counts_idx] != 0)
        {
          Weight->d[weight_idx] = 1.0 / (m_Sinogram->counts->d[counts_idx] * NuisanceParams->alpha->d[i_theta]);
        }
        else
        {
          Weight->d[weight_idx] = 1.0;
        }
#else
		Weight->d[i_theta][i_r][i_t] = 1.0/NuisanceParams->alpha->d[i_theta];
#endif //IDENTITY_NOISE_MODEL endif

      }
    }

  }

//#ifdef DEBUG
  std::cout << "Noise Model Weights:" << std::endl;
  std::cout << "Tilt\tWeight" << std::endl;
  for (uint16_t i_theta = 0; i_theta < m_Sinogram->N_theta; i_theta++)
  {
    std::cout << i_theta << "\t" << NuisanceParams->alpha->d[i_theta] << std::endl;
  }
//#endif
  Real_t VarRatio = AverageVarUpdate / AverageMagVar;
  std::cout << "Ratio of change in Variance " << VarRatio << std::endl;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::writeNuisanceParameters(ScaleOffsetParamsPtr NuisanceParams)
{
  NuisanceParamWriter::Pointer nuisanceBinWriter = NuisanceParamWriter::New();
  nuisanceBinWriter->setSinogram(m_Sinogram);
  nuisanceBinWriter->setTomoInputs(m_TomoInputs);
  nuisanceBinWriter->setObservers(getObservers());
  nuisanceBinWriter->setNuisanceParams(NuisanceParams.get());
#ifdef JOINT_ESTIMATION
  nuisanceBinWriter->setFileName(m_TomoInputs->gainsOutputFile);
  nuisanceBinWriter->setDataToWrite(NuisanceParamWriter::Nuisance_I_O);
  nuisanceBinWriter->execute();
  if (nuisanceBinWriter->getErrorCondition() < 0)
  {
    setErrorCondition(-1);
    notify(nuisanceBinWriter->getErrorMessage().c_str(), 100, Observable::UpdateProgressValueAndMessage);
  }

  nuisanceBinWriter->setFileName(m_TomoInputs->offsetsOutputFile);
  nuisanceBinWriter->setDataToWrite(NuisanceParamWriter::Nuisance_mu);
  nuisanceBinWriter->execute();
  if (nuisanceBinWriter->getErrorCondition() < 0)
  {
   setErrorCondition(-1);
   notify(nuisanceBinWriter->getErrorMessage().c_str(), 100, Observable::UpdateProgressValueAndMessage);
  }
#endif

#ifdef NOISE_MODEL
  nuisanceBinWriter->setFileName(m_TomoInputs->varianceOutputFile);
  nuisanceBinWriter->setDataToWrite(NuisanceParamWriter::Nuisance_alpha);
  nuisanceBinWriter->execute();
  if (nuisanceBinWriter->getErrorCondition() < 0)
  {
    setErrorCondition(-1);
    notify(nuisanceBinWriter->getErrorMessage().c_str(), 100, Observable::UpdateProgressValueAndMessage);
  }
#endif//Noise Model

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::writeSinogramFile(ScaleOffsetParamsPtr NuisanceParams, RealVolumeType::Pointer Final_Sinogram)
{
  // Write the Sinogram out to a file
  SinogramBinWriter::Pointer sinogramWriter = SinogramBinWriter::New();
  sinogramWriter->setSinogram(m_Sinogram);
  sinogramWriter->setTomoInputs(m_TomoInputs);
  sinogramWriter->setObservers(getObservers());
  sinogramWriter->setNuisanceParams(NuisanceParams);
  sinogramWriter->setData(Final_Sinogram);
  sinogramWriter->execute();
  if (sinogramWriter->getErrorCondition() < 0)
  {
    setErrorCondition(-1);
    notify(sinogramWriter->getErrorMessage().c_str(), 100, Observable::UpdateProgressValueAndMessage);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::writeReconstructionFile()
{
  // Write the Reconstruction out to a file
  RawGeometryWriter::Pointer writer = RawGeometryWriter::New();
  writer->setGeometry(m_Geometry);
  writer->setFilePath(m_TomoInputs->reconstructedOutputFile);
  writer->setObservers(getObservers());
  writer->execute();
  if (writer->getErrorCondition() < 0)
  {
    setErrorCondition(writer->getErrorCondition());
    notify("Error Writing the Raw Geometry", 100, Observable::UpdateProgressValueAndMessage);
  }

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::writeVtkFile()
{
  std::string vtkFile(m_TomoInputs->tempDir);
  vtkFile = vtkFile.append(MXADir::getSeparator()).append(ScaleOffsetCorrection::ReconstructedVtkFile);

  VTKStructuredPointsFileWriter vtkWriter;
  vtkWriter.setWriteBinaryFiles(true);
  DimsAndRes dimsAndRes;
  dimsAndRes.dim0 = m_Geometry->N_x;
  dimsAndRes.dim1 = m_Geometry->N_y;
  dimsAndRes.dim2 = m_Geometry->N_z;
  dimsAndRes.resx = 1.0f;
  dimsAndRes.resy = 1.0f;
  dimsAndRes.resz = 1.0f;

  std::vector<VtkScalarWriter*> scalarsToWrite;

  std::stringstream ss;
  ss.str("");
  ss << "Writing Vtk Reconstruction to '" << vtkFile << "'";
  notify(ss.str(), 0, Observable::UpdateProgressMessage);

  VtkScalarWriter* w0 = static_cast<VtkScalarWriter*>(new TomoOutputScalarWriter(m_Geometry.get()));
  w0->setWriteBinaryFiles(true);
  scalarsToWrite.push_back(w0);

  int error = vtkWriter.write<DimsAndRes>(vtkFile, &dimsAndRes, scalarsToWrite);
  if (error < 0)
  {
    std::cout << "Error writing vtk file '" << vtkFile << "'" << std::endl;
  }
  delete w0;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void SOCEngine::writeMRCFile()
{
  /* Write the output to the MRC File */

   std::string mrcFile (m_TomoInputs->tempDir);
   mrcFile = mrcFile.append(MXADir::getSeparator()).append(ScaleOffsetCorrection::ReconstructedMrcFile);

   std::stringstream ss;
   ss.str("");
   ss << "Writing MRC Reconstruction file to '" << mrcFile << "'";
   notify(ss.str(), 0, Observable::UpdateProgressMessage);

   MRCWriter::Pointer mrcWriter = MRCWriter::New();
   mrcWriter->setOutputFile(mrcFile);
   mrcWriter->setGeometry(m_Geometry);
   mrcWriter->write();
}
