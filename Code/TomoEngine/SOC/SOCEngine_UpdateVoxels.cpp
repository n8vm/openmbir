


/*****************************************************************************
 //Finds the min and max of the neighborhood . This is required prior to calling
 solve()
 *****************************************************************************/
#define find_min_max(low, high, V)\
{\
  low=NEIGHBORHOOD[INDEX_3(0,0,0)];\
  high=NEIGHBORHOOD[INDEX_3(0,0,0)];\
  for(uint8_t lcv_i = 0; lcv_i < 3;++lcv_i){\
    for(uint8_t lcv_j = 0; lcv_j < 3; ++lcv_j){\
      for(uint8_t lcv_k = 0; lcv_k < 3; ++lcv_k){\
        if(NEIGHBORHOOD[INDEX_3(lcv_i,lcv_j,lcv_k)] < low) {low = NEIGHBORHOOD[INDEX_3(lcv_i,lcv_j,lcv_k)];}\
        if(NEIGHBORHOOD[INDEX_3(lcv_i,lcv_j,lcv_k)] > high) {high=NEIGHBORHOOD[INDEX_3(lcv_i,lcv_j,lcv_k)];}\
      }\
    }\
  }\
  if(THETA2 !=0){\
    low = (low > (V - (THETA1/THETA2)) ? (V - (THETA1/THETA2)): low);\
    high = (high < (V - (THETA1/THETA2)) ? (V - (THETA1/THETA2)): high);\
  }\
}


//Updates a line of voxels
//Required Global vars / Private members
//ErrorSino
//Weights
//m_Geometry->Object - This is alrady available
//TempCol
//VoxelLine
//NuisanceParams


/* - These get initialized to Zero each time so they are essentially local variables */
//NEIGHBORHOOD
//BOUNDARYFLAG


class UpdateYSlice
#if defined (TomoEngine_USE_PARALLEL_ALGORITHMS)
: public tbb::task
#endif
{
  public:
    UpdateYSlice(uint16_t yStart, uint16_t yEnd,
                 GeometryPtr geometry, int16_t outerIter, int16_t innerIter,
                  SinogramPtr  sinogram, SinogramPtr  bfSinogram,
                  AMatrixCol** tempCol,
                 RealVolumeType::Pointer errorSino,
                 RealVolumeType::Pointer weight,
                 AMatrixCol* voxelLineResponse,
                 ScaleOffsetParams* nuisanceParams,
                 UInt8Image_t::Pointer mask,
                 RealImage_t::Pointer magUpdateMap,//Hold the magnitude of the reconstuction along each voxel line
                 UInt8Image_t::Pointer magUpdateMask,
                 QGGMRF::QGGMRF_Values* qggmrfValues,
                 SOCEngine::VoxelUpdateType updateType,
                 Real_t nh_Threshold,
                 Real_t* averageUpdate,
                 Real_t* averageMagnitudeOfRecon) :
      m_YStart(yStart),
      m_YEnd(yEnd),
      m_Geometry(geometry),
      m_OuterIter(outerIter),
      m_InnerIter(innerIter),
      m_Sinogram(sinogram),
      m_BFSinogram(bfSinogram),
      m_TempCol(tempCol),
      m_ErrorSino(errorSino),
      m_Weight(weight),
      m_VoxelLineResponse(voxelLineResponse),
      m_NuisanceParams(nuisanceParams),
      m_Mask(mask),
      m_MagUpdateMap(magUpdateMap),
      m_MagUpdateMask(magUpdateMask),
      m_QggmrfValues(qggmrfValues),
      m_UpdateType(updateType),
      m_NH_Threshold(nh_Threshold),
      m_ZeroCount(0),
      m_CurrentVoxelValue(0.0),
      m_AverageUpdate(averageUpdate),
      m_AverageMagnitudeOfRecon(averageMagnitudeOfRecon)
    {
      initVariables();
    }

    ~UpdateYSlice()
    {
    }

    /**
     *
     * @return
     */
    int getZeroCount() { return m_ZeroCount; }

  /**
   *
   */
#if defined (TomoEngine_USE_PARALLEL_ALGORITHMS)
    tbb::task*
#else
    void
#endif
    execute()
    {

      RNGVars RandomNumber;
#ifdef RANDOM_ORDER_UPDATES
      int32_t ArraySize = m_Geometry->N_x * m_Geometry->N_z;
      size_t dims[3] =
      { ArraySize, 0, 0};
      Int32ArrayType::Pointer Counter = Int32ArrayType::New(dims, "Counter");

      dims[0] = m_Geometry->N_z;
      dims[1] = m_Geometry->N_x;
      dims[2] = 0;
      UInt8Image_t::Pointer m_VisitCount = UInt8Image_t::New(dims, "VisitCount");

      for (int32_t j_new = 0; j_new < ArraySize; j_new++)
      {
        Counter->d[j_new] = j_new;
      }
      uint32_t NumVoxelsToUpdate = 0;
      for (int32_t j = 0; j < m_Geometry->N_z; j++)
      {
        for (int32_t k = 0; k < m_Geometry->N_x; k++)
        {
          if(m_UpdateType == SOCEngine::NonHomogeniousUpdate)
          {
            if(m_MagUpdateMap->getValue(j, k) > m_NH_Threshold)
            {
              m_MagUpdateMask->setValue(1, j, k);
              m_MagUpdateMap->setValue(0, j, k);
              NumVoxelsToUpdate++;
            }
            else
            {
              m_MagUpdateMask->setValue(0, j, k);
            }
          }
          else if(m_UpdateType == SOCEngine::HomogeniousUpdate)
          {
            m_MagUpdateMap->setValue(0, j, k);
            NumVoxelsToUpdate++;
          }
          else if(m_UpdateType == SOCEngine::RegularRandomOrderUpdate)
          {
            m_MagUpdateMap->setValue(0, j, k);
            NumVoxelsToUpdate++;
          }
          m_VisitCount->setValue(0, j, k);
        }
      }
      //   std::cout << "    " << "Number of voxel lines to update: " << NumVoxelsToUpdate << std::endl;

#endif
      for (int32_t j = 0; j < m_Geometry->N_z; j++) //Row index
      {
        for (int32_t k = 0; k < m_Geometry->N_x; k++) //Column index
        {

#ifdef RANDOM_ORDER_UPDATES

          uint32_t Index = (genrand_int31(&RandomNumber)) % ArraySize;
          int32_t k_new = Counter->d[Index] % m_Geometry->N_x;
          int32_t j_new = Counter->d[Index] / m_Geometry->N_x;
          Counter->d[Index] = Counter->d[ArraySize - 1];
          m_VisitCount->setValue(1, j_new, k_new);
          ArraySize--;
          Index = j_new * m_Geometry->N_x + k_new; //This index pulls out the apprppriate index corresponding to
          //the voxel line (j_new,k_new)
#endif //Random Order updates
          int shouldInitNeighborhood = 0;

          if(m_UpdateType == SOCEngine::NonHomogeniousUpdate && m_MagUpdateMask->getValue(j_new, k_new) == 1 && m_TempCol[Index]->count > 0)
          {
            ++shouldInitNeighborhood;
          }
          if(m_UpdateType == SOCEngine::HomogeniousUpdate && m_TempCol[Index]->count > 0)
          {
            ++shouldInitNeighborhood;
          }
          if(m_UpdateType == SOCEngine::RegularRandomOrderUpdate && m_TempCol[Index]->count > 0)
          {
            ++shouldInitNeighborhood;
          }

          if(shouldInitNeighborhood > 0)
          //After this should ideally call UpdateVoxelLine(j_new,k_new) ie put everything in this "if" inside a method called UpdateVoxelLine
          {
            //   std::cout << "UpdateYSlice- YStart: " << m_YStart << "  YEnd: " << m_YEnd << std::endl;
            Real_t UpdatedVoxelValue = 0.0;
            int32_t errorcode = -1;
            size_t Index = j_new * m_Geometry->N_x + k_new;
            Real_t low = 0.0, high = 0.0;

            for (int32_t i = m_YStart; i < m_YEnd; i++) //slice index
            {

              //Neighborhood of (i,j,k) should be initialized to zeros each time
              for (int32_t p = 0; p <= 2; p++)
              {
                for (int32_t q = 0; q <= 2; q++)
                {
                  for (int32_t r = 0; r <= 2; r++)
                  {
                    NEIGHBORHOOD[INDEX_3(p, q, r)] = 0.0;
                    BOUNDARYFLAG[INDEX_3(p, q, r)] = 0;
                  }
                }
              }
              //For a given (i,j,k) store its 26 point neighborhood
              for (int32_t p = -1; p <= 1; p++)
              {
                for (int32_t q = -1; q <= 1; q++)
                {
                  for (int32_t r = -1; r <= 1; r++)
                  {
                    if(i + p >= 0 && i + p < m_Geometry->N_y)
                    {
                      if(j_new + q >= 0 && j_new + q < m_Geometry->N_z)
                      {
                        if(k_new + r >= 0 && k_new + r < m_Geometry->N_x)
                        {
                          NEIGHBORHOOD[INDEX_3(p + 1, q + 1, r + 1)] = m_Geometry->Object->getValue(q + j_new, r + k_new, p + i);
                          BOUNDARYFLAG[INDEX_3(p + 1, q + 1, r + 1)] = 1;
                        }
                        else
                        {
                          BOUNDARYFLAG[INDEX_3(p + 1, q + 1, r + 1)] = 0;
                        }
                      }
                    }
                  }
                }
              }
              NEIGHBORHOOD[INDEX_3(1, 1, 1)] = 0.0;
              //Compute theta1 and theta2
              m_CurrentVoxelValue = m_Geometry->Object->getValue(j_new, k_new, i); //Store the present value of the voxel
              THETA1 = 0.0;
              THETA2 = 0.0;
#ifdef ZERO_SKIPPING
              //Zero Skipping Algorithm
              bool ZSFlag = true;
              if(m_CurrentVoxelValue == 0.0 && (m_InnerIter > 0 || m_OuterIter > 0))
              {
                for (uint8_t p = 0; p <= 2; p++)
                {
                  for (uint8_t q = 0; q <= 2; q++)
                  {
                    for (uint8_t r = 0; r <= 2; r++)
                    if(NEIGHBORHOOD[INDEX_3(p,q,r)] > 0.0)
                    {
                      ZSFlag = false;
                      break;
                    }
                  }
                }
              }
              else
              {
                ZSFlag = false; //First time dont care for zero skipping
              }
#else
              bool ZSFlag = false; //do ICD on all voxels
#endif //Zero skipping
              if(ZSFlag == false)
              {

                for (uint32_t q = 0; q < m_TempCol[Index]->count; q++)
                {
                  uint16_t i_theta = floor(static_cast<float>(m_TempCol[Index]->index[q] / (m_Sinogram->N_r)));
                  uint16_t i_r = (m_TempCol[Index]->index[q] % (m_Sinogram->N_r));
                  Real_t kConst0 = m_NuisanceParams->I_0->d[i_theta] * (m_TempCol[Index]->values[q]);
                  uint16_t VoxelLineAccessCounter = 0;
                  uint32_t vlrCount = m_VoxelLineResponse[i].index[0] + m_VoxelLineResponse[i].count;
                  for (uint32_t i_t = m_VoxelLineResponse[i].index[0]; i_t < vlrCount; i_t++)
                  {
                    size_t error_idx = m_ErrorSino->calcIndex(i_theta, i_r, i_t);
                    Real_t ProjectionEntry = kConst0 * m_VoxelLineResponse[i].values[VoxelLineAccessCounter];
                    if(m_Sinogram->BF_Flag == false)
                    {
                      THETA2 += (ProjectionEntry * ProjectionEntry * m_Weight->d[error_idx]);
                      THETA1 += (m_ErrorSino->d[error_idx] * ProjectionEntry * m_Weight->d[error_idx]);
                    }
                    else
                    {
                      ProjectionEntry *= m_BFSinogram->counts->d[error_idx];
                      THETA2 += (ProjectionEntry * ProjectionEntry * m_Weight->d[error_idx]);
                      THETA1 += (m_ErrorSino->d[error_idx] * ProjectionEntry * m_Weight->d[error_idx]);
                    }
                    VoxelLineAccessCounter++;
                  }
                }

                THETA1 *= -1;
                find_min_max(low, high, m_CurrentVoxelValue);

                //Solve the 1-D optimization problem
                //printf("V before updating %lf",V);
#ifndef SURROGATE_FUNCTION
                //TODO : What if theta1 = 0 ? Then this will give error

                DerivOfCostFunc docf(BOUNDARYFLAG, NEIGHBORHOOD, FILTER, V, THETA1, THETA2, SIGMA_X_P, MRF_P);
                UpdatedVoxelValue = (Real_t)solve < DerivOfCostFunc > (&docf, (double)low, (double)high, (double)accuracy, &errorcode, binarysearch_count);

                //std::cout<<low<<","<<high<<","<<UpdatedVoxelValue<<std::endl;
#else
                errorcode = 0;
#ifdef EIMTOMO_USE_QGGMRF
                UpdatedVoxelValue = QGGMRF::FunctionalSubstitution(low, high, m_CurrentVoxelValue,
                    BOUNDARYFLAG, FILTER, NEIGHBORHOOD,
                    THETA1, THETA2, m_QggmrfValues);
#else
                SurrogateUpdate = surrogateFunctionBasedMin();
                UpdatedVoxelValue = SurrogateUpdate;
#endif //QGGMRF
#endif//Surrogate function
                if(errorcode == 0)
                {

#ifdef POSITIVITY_CONSTRAINT
                  if(UpdatedVoxelValue < 0.0)
                  { //Enforcing positivity constraints
                    UpdatedVoxelValue = 0.0;
                  }
#endif
                }
                else
                {
                  if(THETA1 == 0 && low == 0 && high == 0)
                  {
                    UpdatedVoxelValue = 0;
                  }

                }

                //TODO Print appropriate error messages for other values of error code
                m_Geometry->Object->setValue(UpdatedVoxelValue, j_new, k_new, i);
                Real_t intermediate = m_MagUpdateMap->getValue(j_new, k_new) + fabs(UpdatedVoxelValue - m_CurrentVoxelValue);
                m_MagUpdateMap->setValue(intermediate, j_new, k_new);

#ifdef ROI
                //if(Mask->d[j_new][k_new] == 1)
                if(m_Mask->getValue(j_new, k_new) == 1)
                {
                  *m_AverageUpdate += fabs(UpdatedVoxelValue - m_CurrentVoxelValue);
                  *m_AverageMagnitudeOfRecon += fabs(m_CurrentVoxelValue); //computing the percentage update =(Change in mag/Initial magnitude)
                }
#endif
                Real_t kConst2 = 0.0;
                //Update the ErrorSinogram
                for (uint32_t q = 0; q < m_TempCol[Index]->count; q++)
                {
                  uint16_t i_theta = floor(static_cast<float>(m_TempCol[Index]->index[q] / (m_Sinogram->N_r)));
                  uint16_t i_r = (m_TempCol[Index]->index[q] % (m_Sinogram->N_r));
                  uint16_t VoxelLineAccessCounter = 0;
                  for (uint32_t i_t = m_VoxelLineResponse[i].index[0]; i_t < m_VoxelLineResponse[i].index[0] + m_VoxelLineResponse[i].count; i_t++)
                  {
                    size_t error_idx = m_ErrorSino->calcIndex(i_theta, i_r, i_t);
                    kConst2 = (m_NuisanceParams->I_0->d[i_theta] * (m_TempCol[Index]->values[q] * m_VoxelLineResponse[i].values[VoxelLineAccessCounter] * (UpdatedVoxelValue - m_CurrentVoxelValue)));
                    if(m_Sinogram->BF_Flag == false)
                    {
                      m_ErrorSino->d[error_idx] -= kConst2;
                    }
                    else
                    {

                      m_ErrorSino->d[error_idx] -= (m_BFSinogram->counts->d[error_idx] * kConst2);
                    }
                    VoxelLineAccessCounter++;
                  }
                }
              }
              else
              {
                m_ZeroCount++;
              }

            }
          }
          else
          {
            continue;
          }

        }
      }

#ifdef RANDOM_ORDER_UPDATES
      for (int j = 0; j < m_Geometry->N_z; j++)
      { //Row index
        for (int k = 0; k < m_Geometry->N_x; k++)
        { //Column index
          if(m_VisitCount->getValue(j, k) == 0)
          {
            printf("Pixel (%d %d) not visited\n", j, k);
          }
        }
      }
#endif

#if defined (TomoEngine_USE_PARALLEL_ALGORITHMS)
      return NULL;
#endif
    }


  private:
    uint16_t m_YStart;
    uint16_t m_YEnd;
    GeometryPtr m_Geometry;
    int16_t m_OuterIter;
    int16_t m_InnerIter;
    SinogramPtr  m_Sinogram;
    SinogramPtr  m_BFSinogram;
    AMatrixCol** m_TempCol;
    RealVolumeType::Pointer m_ErrorSino;
    RealVolumeType::Pointer m_Weight;
    AMatrixCol*  m_VoxelLineResponse;
    ScaleOffsetParams* m_NuisanceParams;
    UInt8Image_t::Pointer m_Mask;
    RealImage_t::Pointer m_MagUpdateMap;//Hold the magnitude of the reconstuction along each voxel line
    UInt8Image_t::Pointer m_MagUpdateMask;
    QGGMRF::QGGMRF_Values* m_QggmrfValues;
    SOCEngine::VoxelUpdateType m_UpdateType;

    Real_t m_NH_Threshold;

    int m_ZeroCount;
    Real_t m_CurrentVoxelValue;
#ifdef ROI
    //variables used to stop the process
    Real_t* m_AverageUpdate;
    Real_t* m_AverageMagnitudeOfRecon;
#endif


    //if 1 then this is NOT outside the support region; If 0 then that pixel should not be considered
    uint8_t BOUNDARYFLAG[27];
    //Markov Random Field Prior parameters - Globals DATA_TYPE
    Real_t FILTER[27];
    Real_t HAMMING_WINDOW[5][5];
    Real_t THETA1;
    Real_t THETA2;
    Real_t NEIGHBORHOOD[27];

    // -----------------------------------------------------------------------------
    //
    // -----------------------------------------------------------------------------
    void initVariables()
    {
     FILTER[INDEX_3(0,0,0)] = 0.0302; FILTER[INDEX_3(0,0,1)] = 0.0370; FILTER[INDEX_3(0,0,2)] = 0.0302;
     FILTER[INDEX_3(0,1,0)] = 0.0370; FILTER[INDEX_3(0,1,1)] = 0.0523; FILTER[INDEX_3(0,1,2)] = 0.0370;
     FILTER[INDEX_3(0,2,0)] = 0.0302; FILTER[INDEX_3(0,2,1)] = 0.0370; FILTER[INDEX_3(0,2,2)] = 0.0302;

     FILTER[INDEX_3(1,0,0)] = 0.0370; FILTER[INDEX_3(1,0,1)] = 0.0523; FILTER[INDEX_3(1,0,2)] = 0.0370;
     FILTER[INDEX_3(1,1,0)] = 0.0523; FILTER[INDEX_3(1,1,1)] = 0.0000; FILTER[INDEX_3(1,1,2)] = 0.0523;
     FILTER[INDEX_3(1,2,0)] = 0.0370; FILTER[INDEX_3(1,2,1)] = 0.0523; FILTER[INDEX_3(1,2,2)] = 0.0370;

     FILTER[INDEX_3(2,0,0)] = 0.0302; FILTER[INDEX_3(2,0,1)] = 0.0370; FILTER[INDEX_3(2,0,2)] = 0.0302;
     FILTER[INDEX_3(2,1,0)] = 0.0370; FILTER[INDEX_3(2,1,1)] = 0.0523; FILTER[INDEX_3(2,1,2)] = 0.0370;
     FILTER[INDEX_3(2,2,0)] = 0.0302; FILTER[INDEX_3(2,2,1)] = 0.0370; FILTER[INDEX_3(2,2,2)] = 0.0302;

     //Hamming Window here
     HAMMING_WINDOW[0][0]= 0.0013; HAMMING_WINDOW[0][1]=0.0086; HAMMING_WINDOW[0][2]=0.0159; HAMMING_WINDOW[0][3]=0.0086;HAMMING_WINDOW[0][4]=0.0013;
     HAMMING_WINDOW[1][0]= 0.0086; HAMMING_WINDOW[1][1]=0.0581;HAMMING_WINDOW[1][2]=0.1076;HAMMING_WINDOW[1][3]=0.0581;HAMMING_WINDOW[1][4]=0.0086;
     HAMMING_WINDOW[2][0]= 0.0159;HAMMING_WINDOW[2][1]=0.1076;HAMMING_WINDOW[2][2]=0.1993;HAMMING_WINDOW[2][3]=0.1076;HAMMING_WINDOW[2][4]=0.0159;
     HAMMING_WINDOW[3][0]= 0.0013;HAMMING_WINDOW[3][1]=0.0086;HAMMING_WINDOW[3][2]=0.0159;HAMMING_WINDOW[3][3]=0.0086;HAMMING_WINDOW[3][4]=0.0013;
     HAMMING_WINDOW[4][0]= 0.0086;HAMMING_WINDOW[4][1]=0.0581;HAMMING_WINDOW[4][2]=0.1076;HAMMING_WINDOW[4][3]=0.0581;HAMMING_WINDOW[4][4]=0.0086;
    }
};


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
uint8_t SOCEngine::updateVoxels(int16_t OuterIter, int16_t Iter,
                             VoxelUpdateType updateType,
                             UInt8Image_t::Pointer VisitCount,
                             RNGVars* RandomNumber,
                             AMatrixCol** TempCol,
                             RealVolumeType::Pointer ErrorSino,
                             RealVolumeType::Pointer Weight,
                             AMatrixCol* VoxelLineResponse,
                             ScaleOffsetParams* NuisanceParams,
                             UInt8Image_t::Pointer Mask,
                             CostData::Pointer cost)
{
  uint8_t exit_status = 1; //Indicates normal exit ; else indicates to stop inner iterations
  uint16_t subIterations = 1;
  std::string indent("    ");
  uint8_t err = 0;
  uint32_t zero_count = 0;
//  Real_t UpdatedVoxelValue;
//  Real_t accuracy=1e-8;
//  uint16_t binarysearch_count
//  Real_t accuracy = 1e-9; //This is the rooting accuracy for x
//  uint32_t binarysearch_count = 10; //Accuracy is 1/(2^10)
//  int32_t errorcode = -1;
//  int16_t Idx;

//  //FIXME: Where are these Initialized? Or what values should they be initialized to?
//  Real_t low = 0.0, high = 0.0;

  if(updateType == RegularRandomOrderUpdate)
  {
    std::cout << indent << "Regular Random Order update of Voxels" << std::endl;
  }
  else if(updateType == HomogeniousUpdate)
  {
    std::cout << indent << "Homogenous update of voxels" << std::endl;
  }
  else if(updateType == NonHomogeniousUpdate)
  {
    std::cout << indent << "Non Homogenous update of voxels" << std::endl;
    subIterations = NUM_NON_HOMOGENOUS_ITER;
  }
  else
  {
    std::cout << indent << "Unknown Voxel Update Type. Returning Now" << std::endl;
    return exit_status;
  }

  Real_t NH_Threshold = 0.0;
  std::stringstream ss;
  int totalLoops = m_TomoInputs->NumOuterIter * m_TomoInputs->NumIter;

  for (uint16_t NH_Iter = 0; NH_Iter < subIterations; ++NH_Iter)
  {
    ss.str("");
    ss << "Outer Iteration: " << OuterIter << " of " << m_TomoInputs->NumOuterIter;
    ss << "   Inner Iteration: " << Iter << " of " << m_TomoInputs->NumIter;
    ss << "   SubLoop: " << NH_Iter << " of " << subIterations;
    float currentLoop = static_cast<float>(OuterIter * m_TomoInputs->NumIter + Iter);
    notify(ss.str(), currentLoop / totalLoops * 100.0f, Observable::UpdateProgressValueAndMessage);
    if(updateType == NonHomogeniousUpdate)
    {
      //Compute VSC and create a map of pixels that are above the threshold value
      ComputeVSC();
      START_TIMER;
      NH_Threshold = SetNonHomThreshold();
      STOP_TIMER;PRINT_TIME("  SetNonHomThreshold");
      std::cout << indent << "NHICD Threshold: " << NH_Threshold << std::endl;
      //Use  FiltMagUpdateMap  to find MagnitudeUpdateMask
      //std::cout << "Completed Calculation of filtered magnitude" << std::endl;
      //Calculate the threshold for the top ? % of voxel updates
    }

    //printf("Iter %d\n",Iter);
#ifdef ROI
    //variables used to stop the process
    Real_t AverageUpdate = 0;
    Real_t AverageMagnitudeOfRecon = 0;
#endif

    START_TIMER;
#if defined (TomoEngine_USE_PARALLEL_ALGORITHMS)
    std::vector<int> yCount(m_NumThreads, 0);
    int t = 0;
    for(int y = 0; y < m_Geometry->N_y; ++y)
    {
      yCount[t]++;
      ++t;
      if (t==m_NumThreads) { t = 0; }
    }

    uint16_t yStart = 0;
    uint16_t yStop = 0;

    tbb::task_list taskList;
    Real_t* averageUpdate = (Real_t*)(malloc(sizeof(Real_t) * m_NumThreads));
    ::memset(averageUpdate, 0, sizeof(Real_t) * m_NumThreads);
    Real_t* averageMagnitudeOfRecon = (Real_t*)(malloc(sizeof(Real_t) * m_NumThreads));
    ::memset(averageMagnitudeOfRecon, 0, sizeof(Real_t) * m_NumThreads);
    for (int t = 0; t < m_NumThreads; ++t)
    {
      yStart = yStop;
      yStop = yStart + yCount[t];
      if (yStart == yStop) { continue; } // Processor has NO tasks to run because we have less Y's than cores



      std::cout << "Thread: " << t << " yStart: " << yStart << "  yEnd: " << yStop << std::endl;
      UpdateYSlice& a =
          *new (tbb::task::allocate_root()) UpdateYSlice(yStart, yStop,
                                                         m_Geometry,
                                                         OuterIter, Iter, m_Sinogram,
                                                         m_BFSinogram, TempCol,
                                                         ErrorSino, Weight, VoxelLineResponse,
                                                         NuisanceParams, Mask,
                                                         MagUpdateMap, MagUpdateMask,
                                                         &m_QGGMRF_Values,
                                                         updateType,
                                                         NH_Threshold,
                                                         averageUpdate + t,
                                                         averageMagnitudeOfRecon + t);
      taskList.push_back(a);
    }

    tbb::task::spawn_root_and_wait(taskList);
    // Now sum up some values
    for (int t = 0; t < m_NumThreads; ++t)
    {
      AverageUpdate += averageUpdate[t];
      AverageMagnitudeOfRecon += averageMagnitudeOfRecon[t];
    }
    free(averageUpdate);
    free(averageMagnitudeOfRecon);

#else
          uint16_t yStop = m_Geometry->N_y;
          uint16_t yStart = 0;
          UpdateYSlice yVoxelUpdate(yStart, yStop,
                                    m_Geometry,
                                    OuterIter, Iter, m_Sinogram,
                                    m_BFSinogram, TempCol,
                                    ErrorSino, Weight, VoxelLineResponse,
                                    NuisanceParams, Mask,
                                    MagUpdateMap, MagUpdateMask,
                                    &m_QGGMRF_Values,
                                    updateType,
                                    NH_Threshold,
                                    &AverageUpdate,
                                    &AverageMagnitudeOfRecon);

          yVoxelUpdate.execute();
#endif
          /* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

    STOP_TIMER;
    PRINT_TIME("%%%%%=====> Voxel Update");



#ifdef COST_CALCULATE

    /*********************Cost Calculation*************************************/
    Real_t cost_value = computeCost(ErrorSino, Weight);
    std::cout << cost_value << std::endl;
    int increase = cost->addCostValue(cost_value);
    if(increase == 1)
    {
      std::cout << "Cost just increased after ICD!" << std::endl;
      break;
    }
    cost->writeCostValue(cost_value);
    /**************************************************************************/
#else
    printf("%d\n",Iter);
#endif //Cost calculation endif

#ifdef ROI

	  std::cout<<"Average Update "<<AverageUpdate<<std::endl;
	  std::cout<<"Average Mag "<<AverageMagnitudeOfRecon<<std::endl;
    if(AverageMagnitudeOfRecon > 0)
    {
      printf("%d,%lf\n", Iter + 1, AverageUpdate / AverageMagnitudeOfRecon);

      //Use the stopping criteria if we are performing a full update of all voxels
      if((AverageUpdate / AverageMagnitudeOfRecon) < m_TomoInputs->StopThreshold && updateType != NonHomogeniousUpdate)
      {
        printf("This is the terminating point %d\n", Iter);
        m_TomoInputs->StopThreshold *= THRESHOLD_REDUCTION_FACTOR; //Reducing the thresold for subsequent iterations
        std::cout << "New threshold" << m_TomoInputs->StopThreshold << std::endl;
        exit_status = 0;
        break;
      }
    }
#endif//ROI end

#ifdef WRITE_INTERMEDIATE_RESULTS

    if(Iter == NumOfWrites*WriteCount)
    {
      WriteCount++;
      sprintf(buffer,"%d",Iter);
      sprintf(Filename,"ReconstructedObjectAfterIter");
      strcat(Filename,buffer);
      strcat(Filename,".bin");
      Fp3 = fopen(Filename, "w");
      TempPointer = m_Geometry->Object;
      NumOfBytesWritten=fwrite(&(m_Geometry->Object->d[0][0][0]), sizeof(Real_t),m_Geometry->N_x*m_Geometry->N_y*m_Geometry->N_z, Fp3);
      printf("%d\n",NumOfBytesWritten);

      fclose(Fp3);
    }
#endif

    if(getCancel() == true)
    {
      setErrorCondition(err);
      return exit_status;
    }

  }

  std::cout << "Number of zeroed out entries=" << zero_count << std::endl;
  return exit_status;

}