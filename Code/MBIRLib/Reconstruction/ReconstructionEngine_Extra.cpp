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

#include "ReconstructionEngine.h"
#include "MBIRLib/Reconstruction/ReconstructionConstants.h"
#include "MBIRLib/GenericFilters/MRCSinogramInitializer.h"
#include "MBIRLib/Common/EIMMath.h"
#include "MBIRLib/Common/EIMTime.h"
#include "MBIRLib/HAADF/UpdateYSlice.h"


#define START_TIMER uint64_t startm = EIMTOMO_getMilliSeconds();
#define STOP_TIMER uint64_t stopm = EIMTOMO_getMilliSeconds();
#define PRINT_TIME(msg)\
std::cout << indent << msg << ": " << ((double)stopm-startm)/1000.0 << " seconds" << std::endl;


//Function to print a list of voxel indices 
void printList(struct List InList)
{
	std::cout<<"Printing List :"<<std::endl;
	for(int32_t i=0; i < InList.NumElts; i++)
	std::cout<<"("<<InList.Array[i].zidx<<","<<InList.Array[i].xidx<<")"<<std::endl;
	
}

void maxList(struct List InList)
{
	int32_t max_z = -1;
	int32_t max_x = -1;
	for(int32_t i = 0; i < InList.NumElts; i++)
	{
		if(InList.Array[i].zidx > max_z)
			max_z = InList.Array[i].zidx;
			
		if(InList.Array[i].xidx > max_x)
			max_x = InList.Array[i].xidx;
	}	
	std::cout<<"Number of elements ="<<InList.NumElts<<std::endl;
	std::cout<<"("<<max_z<<","<<max_x<<")"<<std::endl;	
}


void minList(struct List InList)
{
	int32_t min_z = pow(2, 31);
	int32_t min_x = min_z;
	for(int32_t i = 0; i < InList.NumElts; i++)
	{
		if(InList.Array[i].zidx < min_z)
			min_z = InList.Array[i].zidx;
		
		if(InList.Array[i].xidx < min_x)
			min_x = InList.Array[i].xidx;
	}	
	std::cout<<"Number of elements ="<<InList.NumElts<<std::endl;
	std::cout<<"("<<min_z<<","<<min_x<<")"<<std::endl;	
}

// Read the Input data from the supplied data file
// We are scoping here so the various readers are automatically cleaned up before
// the code goes any farther
int ReconstructionEngine::readInputData()
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
  dataReader->setAdvParams(m_AdvParams);
  dataReader->setObservers(getObservers());
  dataReader->setVerbose(getVerbose());
  dataReader->setVeryVerbose(getVeryVerbose());
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
// Initialize the Geometry data from a rough reconstruction
// -----------------------------------------------------------------------------
int ReconstructionEngine::initializeRoughReconstructionData()
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
  else if (extension.compare("mrc") == 0)
  {
    notify("We are not dealing with mrc volume files.", 0, Observable::UpdateErrorMessage);
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
  geomInitializer->setAdvParams(m_AdvParams);
  geomInitializer->setObservers(getObservers());
  geomInitializer->setVerbose(getVerbose());
  geomInitializer->setVeryVerbose(getVeryVerbose());
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
void ReconstructionEngine::computeOriginalXDims(uint16_t &cropStart, uint16_t &cropEnd)
{

  Real_t x;
  for (uint16_t j = 0; j < m_Geometry->N_x; j++)
  {
    x = m_Geometry->x0 + ((Real_t)j + 0.5) * m_TomoInputs->delta_xz;
    if(x >= -(m_Sinogram->N_r * m_Sinogram->delta_r) / 2 && x <= (m_Sinogram->N_r * m_Sinogram->delta_r) / 2)
    {
      cropStart = j;
      break;
    }

  }

  for (int16_t j = m_Geometry->N_x - 1; j >= 0; j--)
  {
    x = m_Geometry->x0 + ((Real_t)j + 0.5) * m_TomoInputs->delta_xz;
    if(x >= -(m_Sinogram->N_r * m_Sinogram->delta_r) / 2 && x <= (m_Sinogram->N_r * m_Sinogram->delta_r) / 2)
    {
      cropEnd = (uint16_t)j;
      break;
    }
  }
	cropEnd+=1;	
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void ReconstructionEngine::initializeHt(RealVolumeType::Pointer H_t, Real_t OffsetT)
{
  Real_t ProfileCenterT;
  for (uint16_t k = 0; k < m_Sinogram->N_theta; k++)
  {
    for (unsigned int i = 0; i <m_AdvParams->DETECTOR_RESPONSE_BINS; i++)
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
void ReconstructionEngine::initializeVolume(RealVolumeType::Pointer Y_Est, double value)
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
void ReconstructionEngine::storeVoxelResponse(RealVolumeType::Pointer H_t,
                                              std::vector<HAADFAMatrixCol::Pointer> &VoxelLineResponse,
                                              HAADFDetectorParameters::Pointer haadfParameters)
{
  Real_t ProfileThickness = 0.0;
  Real_t y = 0.0;
  Real_t t = 0.0;
  Real_t tmin;
  Real_t tmax;
  int16_t slice_index_min, slice_index_max;
  Real_t center_t, delta_t;
  int16_t index_delta_t;
  Real_t w3, w4;

  const Real_t offsetT = haadfParameters->getOffsetT();
  //Storing the response along t-direction for each voxel line
  notify("Storing the response along Y-direction for each voxel line", 0, Observable::UpdateProgressMessage);
  if(getVeryVerbose())
  {
    std::cout << "Voxel Response by Y Slice" << std::endl;
    std::cout << "Y\tProfile Thickness" << std::endl;
  }
  for (uint16_t i = 0; i < m_Geometry->N_y; i++)
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
      index_delta_t = static_cast<uint16_t>(floor(delta_t / offsetT));
      if(index_delta_t < m_AdvParams->DETECTOR_RESPONSE_BINS)
      {
        w3 = delta_t - (Real_t)(index_delta_t) * offsetT;
        w4 = ((Real_t)index_delta_t + 1) * offsetT - delta_t;
        uint16_t ttmp = index_delta_t + 1 < m_AdvParams->DETECTOR_RESPONSE_BINS ? index_delta_t + 1 : m_AdvParams->DETECTOR_RESPONSE_BINS - 1;
		  ProfileThickness = (w4 / offsetT) * H_t->getValue(0, 0, index_delta_t) + (w3 / offsetT) * H_t->getValue(0, 0, ttmp);
      }
      else
      {
        ProfileThickness = 0;
      }

      if(ProfileThickness != 0) //Store the response of this slice
      {
        if(getVeryVerbose())
        {
          std::cout << i_t << "\t" << ProfileThickness << std::endl;
        }
        HAADFAMatrixCol::Pointer vlr = VoxelLineResponse[i];
        int32_t count = vlr->count;
        vlr->values[count] = ProfileThickness;
        vlr->index[count] = i_t;
//        size_t dim0 = vlr->valuesPtr->getDims()[0];
        vlr->setCount(count + 1);
      }
    }
  }
}



// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void ReconstructionEngine::calculateArithmeticMean()
{

}


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void ReconstructionEngine::writeReconstructionFile(const std::string &filepath)
{
  // Write the Reconstruction out to a file
  RawGeometryWriter::Pointer writer = RawGeometryWriter::New();
  writer->setGeometry(m_Geometry);
  writer->setFilePath(filepath);
  writer->setAdvParams(m_AdvParams);
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
void ReconstructionEngine::writeVtkFile(const std::string &vtkFile, uint16_t cropStart, uint16_t cropEnd)
{
  std::stringstream ss;
  ss << "Writing VTK file to '" << vtkFile << "'";
  notify(ss.str(), 0, Observable::UpdateProgressMessage);

  VTKStructuredPointsFileWriter vtkWriter;
  vtkWriter.setWriteBinaryFiles(true);


  DimsAndRes dimsAndRes;
  dimsAndRes.xStart = cropStart;
  dimsAndRes.xEnd = cropEnd;
  dimsAndRes.yStart = 0;
  dimsAndRes.yEnd = m_Geometry->N_y;
  dimsAndRes.zStart = 0;
  dimsAndRes.zEnd = m_Geometry->N_z;
  dimsAndRes.resx = 1.0f;
  dimsAndRes.resy = 1.0f;
  dimsAndRes.resz = 1.0f;

  std::vector<VtkScalarWriter*> scalarsToWrite;

  VtkScalarWriter* w0 = static_cast<VtkScalarWriter*>(new TomoOutputScalarWriter(m_Geometry.get()));
  w0->setXDims(cropStart, cropEnd);
  w0->setYDims(0, m_Geometry->N_y);
  w0->setZDims(0, m_Geometry->N_z);
  w0->setWriteBinaryFiles(true);
  scalarsToWrite.push_back(w0);

  int error = vtkWriter.write<DimsAndRes>(vtkFile, &dimsAndRes, scalarsToWrite);
  if(error < 0)
  {
    ss.str("");
    ss << "Error writing vtk file\n    '" << vtkFile << "'" << std::endl;
    setErrorCondition(-12);
    notify(ss.str(), 0, Observable::UpdateErrorMessage);
  }
  delete w0;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void ReconstructionEngine::writeMRCFile(const std::string &mrcFile, uint16_t cropStart, uint16_t cropEnd)
{
  /* Write the output to the MRC File */
  std::stringstream ss;
  ss.str("");
  ss << "Writing MRC file to '" << mrcFile << "'";
  notify(ss.str(), 0, Observable::UpdateProgressMessage);

  MRCWriter::Pointer mrcWriter = MRCWriter::New();
  mrcWriter->setOutputFile(mrcFile);
  mrcWriter->setGeometry(m_Geometry);
  mrcWriter->setAdvParams(m_AdvParams);
  mrcWriter->setXDims(cropStart, cropEnd);
  mrcWriter->setYDims(0, m_Geometry->N_y);
  mrcWriter->setZDims(0, m_Geometry->N_z);
  mrcWriter->setObservers(getObservers());
  mrcWriter->execute();
  if(mrcWriter->getErrorCondition() < 0)
  {
    ss.str("");
    ss << "Error writing MRC file\n    '" << mrcFile << "'" << std::endl;
    setErrorCondition(mrcWriter->getErrorCondition());
    notify(ss.str(), 0, Observable::UpdateErrorMessage);
  }

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void ReconstructionEngine::writeAvizoFile(const std::string &file, uint16_t cropStart, uint16_t cropEnd)
{
  //  std::cout << "Writing Avizo file " << file << std::endl;

  /* Write the output to the Avizo File */
  std::stringstream ss;
  ss.str("");
  ss << "Writing Avizo file to '" << file << "'";
  notify(ss.str(), 0, Observable::UpdateProgressMessage);

  AvizoUniformCoordinateWriter::Pointer writer = AvizoUniformCoordinateWriter::New();
  writer->setOutputFile(file);
  writer->setGeometry(m_Geometry);
  writer->setAdvParams(m_AdvParams);
  writer->setTomoInputs(m_TomoInputs);
  writer->setXDims(cropStart, cropEnd);
  writer->setYDims(0, m_Geometry->N_y);
  writer->setZDims(0, m_Geometry->N_z);
  writer->setObservers(getObservers());
  writer->setWriteBinaryFile(true);
  writer->execute();
  if(writer->getErrorCondition() < 0)
  {
    ss.str("");
    ss << "Error writing Avizo file\n    '" << file << "'" << std::endl;
    setErrorCondition(writer->getErrorCondition());
    notify(ss.str(), 0, Observable::UpdateErrorMessage);
  }

}

// -----------------------------------------------------------------------------
// Updates the voxels specified in the list for homogenous update. For non homgenous
// update computes the list. Calls the parallel update of voxels routine
// -----------------------------------------------------------------------------
uint8_t ReconstructionEngine::updateVoxels(int16_t OuterIter,
										   int16_t Iter,
										   std::vector<HAADFAMatrixCol::Pointer> &TempCol,
										   RealVolumeType::Pointer ErrorSino,
										   std::vector<HAADFAMatrixCol::Pointer> &VoxelLineResponse,
										   CostData::Pointer cost,
										   QGGMRF::QGGMRF_Values* qggmrf_values,
										   RealImageType::Pointer magUpdateMap,
										   RealImageType::Pointer filtMagUpdateMap,
										   UInt8Image_t::Pointer magUpdateMask,
										   UInt8Image_t::Pointer m_VisitCount,
										   Real_t PrevMagSum,
										   uint32_t EffIterCount)

{

#if ROI
	//variables used to stop the process
	Real_t AverageUpdate = 0;
	Real_t AverageMagnitudeOfRecon = 0;
#endif
	
    unsigned int updateType = VoxelUpdateType::RegularRandomOrderUpdate;
    struct List NHList;//non homogenous list of voxels to update
	
	
#ifdef NHICD
    if(0 == EffIterCount % 2)
    {
        updateType = VoxelUpdateType::HomogeniousUpdate;
    }
    else
    {
        updateType = VoxelUpdateType::NonHomogeniousUpdate;
    }
#endif//NHICD end if

#if defined (OpenMBIR_USE_PARALLEL_ALGORITHMS)
    tbb::task_scheduler_init init;
    int m_NumThreads = init.default_num_threads();
#else
    int m_NumThreads = 1;
#endif //Parallel algorithms

    std::stringstream ss;
    uint8_t exit_status = 1; //Indicates normal exit ; else indicates to stop inner iterations
    uint16_t subIterations = 1;
    std::string indent("    ");
    uint8_t err = 0;


    if(updateType == VoxelUpdateType::RegularRandomOrderUpdate)
    {
        ss << indent << "Regular Random Order update of Voxels" << std::endl;						
    }
    else if(updateType == VoxelUpdateType::HomogeniousUpdate)
    {
        ss << indent << "Homogenous update of voxels" << std::endl;		
    }
    else if(updateType == VoxelUpdateType::NonHomogeniousUpdate)
    {
        ss << indent << "Non Homogenous update of voxels" << std::endl;
        subIterations = SUB_ITER;		
    }
    else
    {
        ss << indent << "Unknown Voxel Update Type. Returning Now" << std::endl;
        notify(ss.str(), 0, Observable::UpdateErrorMessage);
        return exit_status;
    }

    if(getVerbose())
    {
        std::cout << ss.str() << std::endl;
    }

    Real_t NH_Threshold = 0.0;
    int totalLoops = m_TomoInputs->NumOuterIter * m_TomoInputs->NumIter;

#ifdef DEBUG
	std::cout<<"Max of list in ReconEngineExtra = "<<std::endl;
	maxList(m_VoxelIdxList);
#endif //DEBUG
	
    for (uint16_t NH_Iter = 0; NH_Iter < subIterations; ++NH_Iter) //This can be varied 
		//so each iterations of the voxels has multiple passes over the voxels 
		//but generally its set to 1
    {
				
        ss.str(" ");
        ss << "Outer Iteration: " << OuterIter << " of " << m_TomoInputs->NumOuterIter;
        ss << "   Inner Iteration: " << Iter << " of " << m_TomoInputs->NumIter;
        ss << "   SubLoop: " << NH_Iter << " of " << subIterations;
        float currentLoop = static_cast<float>(OuterIter * m_TomoInputs->NumIter + Iter);
        notify(ss.str(), currentLoop / totalLoops * 100.0f, Observable::UpdateProgressValueAndMessage);
        if(updateType == VoxelUpdateType::NonHomogeniousUpdate)
        {
#ifdef DEBUG
			Real_t TempSum = 0;
			for (int32_t j = 0; j < m_Geometry->N_z; j++)
				for (int32_t k = 0; k < m_Geometry->N_x; k++)
				{
					TempSum += magUpdateMap->getValue(j,k);
				}
			std::cout<<"**********************************************"<<std::endl;
			std::cout<<"Average mag Prior to VSC"<<TempSum/(m_Geometry->N_z*m_Geometry->N_x)<<std::endl;
			std::cout<<"**********************************************"<<std::endl;
#endif //debug	
			
            //Compute VSC and create a map of pixels that are above the threshold value
            ComputeVSC(magUpdateMap, filtMagUpdateMap);
			
#ifdef DEBUG
			TempSum = 0;
			for (int32_t j = 0; j < m_Geometry->N_z; j++)
				for (int32_t k = 0; k < m_Geometry->N_x; k++)
				{
					TempSum += magUpdateMap->getValue(j,k);
				}
			std::cout<<"**********************************************"<<std::endl;
			std::cout<<"Average mag Prior to NH Thresh"<<TempSum/(m_Geometry->N_z*m_Geometry->N_x)<<std::endl;
			std::cout<<"**********************************************"<<std::endl;
#endif //debug
			
            START_TIMER;
            NH_Threshold = SetNonHomThreshold(filtMagUpdateMap);
            STOP_TIMER;
            PRINT_TIME("  SetNonHomThreshold");
            std::cout << indent << "NHICD Threshold: " << NH_Threshold << std::endl;
            //Use  filtMagUpdateMap  to find MagnitudeUpdateMask
            //std::cout << "Completed Calculation of filtered magnitude" << std::endl;
            //Calculate the threshold for the top ? % of voxel updates
			
			//Generate a new List 
			NHList = GenNonHomList(NH_Threshold, filtMagUpdateMap);
			m_VoxelIdxList = NHList;			

        }
		


        START_TIMER;
#if defined (OpenMBIR_USE_PARALLEL_ALGORITHMS)
				
        std::vector<int> yCount(m_NumThreads, 0);
        int t = 0;
		
//		if( m_Geometry->N_y >= m_NumThreads) //If number of slices is greater than number of threads divide the updates
//		{
        for (int y = 0; y < m_Geometry->N_y; ++y)
        {
            yCount[t]++;
            ++t;
            if(t == m_NumThreads)
            {
                t = 0;
            }
        }
//		}
//		else { //Use a single core implementation
//			yCount[t]=m_Geometry->N_y;
//		}
		
		//Pass a randomized version of the list (hom or NH) to each thread
		//struct List NewList[t];				
		/*for(t = 0; t < m_NumThreads; ++t)
		{
			NewList[t]=GenRandList(m_VoxelIdxList);		
			
			std::cout<<"Max of list "<<t<<std::endl;
			maxList(NewList[t]);
			
		}*/		

		//Checking which voxels are going to be visited 
        for(int16_t tmpiter = 0; tmpiter < m_VoxelIdxList.NumElts; tmpiter++)
		{
			m_VisitCount->setValue(1,m_VoxelIdxList.Array[tmpiter].zidx,m_VoxelIdxList.Array[tmpiter].xidx);
			magUpdateMap->setValue(0,m_VoxelIdxList.Array[tmpiter].zidx,m_VoxelIdxList.Array[tmpiter].xidx);
		}
				
        uint16_t yStart = 0;
        uint16_t yStop = 0;

        tbb::task_list taskList;
        Real_t* averageUpdate = (Real_t*)(malloc(sizeof(Real_t) * m_NumThreads));
        ::memset(averageUpdate, 0, sizeof(Real_t) * m_NumThreads);
        Real_t* averageMagnitudeOfRecon = (Real_t*)(malloc(sizeof(Real_t) * m_NumThreads));
        ::memset(averageMagnitudeOfRecon, 0, sizeof(Real_t) * m_NumThreads);
		
		
		//Initialize individual magnitude maps for the separate threads
		dims[0] = m_Geometry->N_z; //height
		dims[1] = m_Geometry->N_x; //width			
		std::vector <RealImageType::Pointer> magUpdateMaps;
	/*	for (int t = 0; t < m_NumThreads; ++t)
        {
            yStart = yStop;
            yStop = yStart + yCount[t];
            if(yStart == yStop)
            {
                continue;
            } 
			RealImageType::Pointer _magUpdateMap = RealImageType::New(dims, "Mag Update Map");
			_magUpdateMap->initializeWithZeros();
			magUpdateMaps.push_back(_magUpdateMap);
		}
		
		yStart=0;
		yStop=0;*/
		
		std::cout<<" Starting multicore allocation .."<<std::endl;
		
        for (int t = 0; t < m_NumThreads; ++t)
        {
			
            yStart = yStop;
            yStop = yStart + yCount[t];
			
#ifdef DEBUG
			std::cout<<"Thread :"<<t<<"("<<yStart<<","<<yStop<<")"<<std::endl;
#endif //debug
			
            if(yStart == yStop)
            {
                continue;
            } // Processor has NO tasks to run because we have less Y's than cores
			
			RealImageType::Pointer _magUpdateMap = RealImageType::New(dims, "Mag Update Map");
			_magUpdateMap->initializeWithZeros();
			//::memset(_magUpdateMap->getPointer(),0,sizeof(Real_t)*m_Geometry->N_z*m_Geometry->N_x);
			magUpdateMaps.push_back(_magUpdateMap);
			
			struct List NewList = m_VoxelIdxList;
			NewList = GenRandList(NewList);
			
            // std::cout << "Thread: " << t << " yStart: " << yStart << "  yEnd: " << yStop << std::endl;
            UpdateYSlice& a =
            *new (tbb::task::allocate_root()) UpdateYSlice(yStart, yStop, m_Geometry, OuterIter, Iter,
                                                           m_Sinogram, TempCol, ErrorSino,
                                                           VoxelLineResponse, m_ForwardModel.get(), 
														   magUpdateMask,
														   _magUpdateMap, 
														   magUpdateMask, updateType,
                                                           NH_Threshold,
                                                           averageUpdate + t,
                                                           averageMagnitudeOfRecon + t,
                                                           m_AdvParams->ZERO_SKIPPING,
                                                           qggmrf_values, NewList);
            taskList.push_back(a);
        }

        tbb::task::spawn_root_and_wait(taskList);
        
		std::cout<<" Voxel update complete .."<<std::endl;
		
		// Now sum up magnitude update map values
        for (int t = 0; t < m_NumThreads; ++t)
        {
			//if(NewList[t].Array != NULL)
			//free(NewList[t].Array); //Free the space allocated to each randomized list
#ifdef DEBUG
			if(getVeryVerbose()) //TODO: Change variable name averageUpdate to totalUpdate
			{
				std::cout << "Total Update for thread "<<t<<":"<< averageUpdate[t]<< std::endl;
				std::cout << "Total Magnitude for thread "<<t<<":"<< averageMagnitudeOfRecon[t]<< std::endl;
			}
#endif //Debug
            AverageUpdate += averageUpdate[t];
            AverageMagnitudeOfRecon += averageMagnitudeOfRecon[t];
			
        }
        free(averageUpdate);
        free(averageMagnitudeOfRecon);
		
		
		//From individual threads update the magnitude map
		std::cout<<" Magnitude Map Update.."<<std::endl;
		RealImageType::Pointer TempPointer;
		for(int16_t tmpiter = 0; tmpiter < m_VoxelIdxList.NumElts; tmpiter++)
		{
			Real_t TempSum =0;
			for (uint16_t t = 0; t < magUpdateMaps.size(); ++t)
			{				
				TempPointer = magUpdateMaps[t];
				TempSum += TempPointer->getValue(m_VoxelIdxList.Array[tmpiter].zidx,m_VoxelIdxList.Array[tmpiter].xidx);			    
			}
			//std::cout<<"thread ="<<t<<std::endl;
			//Set the overall magnitude update map
			magUpdateMap->setValue(TempSum,m_VoxelIdxList.Array[tmpiter].zidx,m_VoxelIdxList.Array[tmpiter].xidx);
		}
		
#else //TODO modify the single thread code to handle the list based iterations
        uint16_t yStop = m_Geometry->N_y;
        uint16_t yStart = 0;
      /*  UpdateYSlice yVoxelUpdate(yStart, yStop,
                                  m_Geometry,
                                  OuterIter, Iter, m_Sinogram,
                                  m_BFSinogram, TempCol,
                                  ErrorSino, Weight, VoxelLineResponse,
                                  NuisanceParams, Mask,
                                  magUpdateMap, MagUpdateMask,
                                  &m_QGGMRF_Values,
                                  updateType,
                                  NH_Threshold,
                                  &AverageUpdate,
                                  &AverageMagnitudeOfRecon,
                                  m_AdvParams->ZERO_SKIPPING);

        yVoxelUpdate.execute();*/
		Real_t* averageUpdate = (Real_t*)(malloc(sizeof(Real_t)));
        ::memset(averageUpdate, 0, sizeof(Real_t));
        Real_t* averageMagnitudeOfRecon = (Real_t*)(malloc(sizeof(Real_t) ));
        ::memset(averageMagnitudeOfRecon, 0, sizeof(Real_t) );
		/*UpdateYSlice yVoxelUpdate(yStart, yStop, m_Geometry, OuterIter, Iter,
					 m_Sinogram, TempCol, ErrorSino,
					 VoxelLineResponse, m_ForwardModel.get(), mask,
					 magUpdateMap, magUpdateMask, updateType,
					 NH_Threshold,
					 averageUpdate,
					 averageMagnitudeOfRecon,
					 m_AdvParams->ZERO_SKIPPING,
					 qggmrf_values);*/
		
		UpdateYSlice(yStart, yStop, m_Geometry, OuterIter, Iter,
					 m_Sinogram, TempCol, ErrorSino,
					 VoxelLineResponse, m_ForwardModel.get(), mask,
					 magUpdateMap, 
					 magUpdateMask, updateType,
					 NH_Threshold,
					 averageUpdate,
					 averageMagnitudeOfRecon ,
					 m_AdvParams->ZERO_SKIPPING,
					 qggmrf_values, m_VoxelIdxList);
		
		yVoxelUpdate.execute();	
		
		AverageUpdate += averageUpdate[0];
		AverageMagnitudeOfRecon += averageMagnitudeOfRecon[0];
		free(averageUpdate);
		free(averageMagnitudeOfRecon);
	
		
#endif //Parallel algorithms
        /* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */STOP_TIMER;
        ss.str("");
        ss << "Inner Iter: " << Iter << " Voxel Update";
        PRINT_TIME(ss.str());

        if(getVerbose())
        {
            std::cout << "Total Update " << AverageUpdate << std::endl;
            std::cout << "Total Mag " << AverageMagnitudeOfRecon << std::endl;
        }
		
		

		if(getCancel() == true)
        {
            setErrorCondition(err);
            return exit_status;
        }

    }
	
//Stopping criteria code 
	exit_status=stopCriteria(magUpdateMap,magUpdateMask,PrevMagSum, EffIterCount);
	
#ifdef WRITE_INTERMEDIATE_RESULTS
	
	if(Iter == NumOfWrites*WriteCount)
	{
		WriteCount++;
		sprintf(buffer,"%d",Iter);
		sprintf(Filename,"ReconstructedObjectAfterIter");
		strcat(Filename,buffer);
		strcat(Filename,".bin");
		Fp3 = fopen(Filename, "w");
		TempPointer = geometry->Object;
		NumOfBytesWritten=fwrite(&(geometry->Object->d[0][0][0]), sizeof(Real_t),geometry->N_x*geometry->N_y*geometry->N_z, Fp3);
		printf("%d\n",NumOfBytesWritten);
		
		fclose(Fp3);
	}
#endif //write Intermediate results
	
#ifdef NHICD
	if(updateType == VoxelUpdateType::NonHomogeniousUpdate && NHList.Array != NULL)
	free(NHList.Array);
	
#endif //NHICD
	
	std::cout<<"exiting voxel update routine"<<std::endl;
	
    return exit_status;

}



// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void ReconstructionEngine::initializeROIMask(UInt8Image_t::Pointer Mask)
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
void ReconstructionEngine::ComputeVSC(RealImageType::Pointer magUpdateMap, RealImageType::Pointer filtMagUpdateMap)
{
    Real_t filter_op = 0;
	
	
    // int err = 0;
#ifdef DEBUG
    FILE *Fp = NULL;
    MAKE_OUTPUT_FILE(Fp, m_TomoInputs->tempDir, ScaleOffsetCorrection::MagnitudeMapFile);
    if(errno < 0)
    {

    }
    fwrite(magUpdateMap->getPointer(0, 0), m_Geometry->N_x * m_Geometry->N_z, sizeof(Real_t), Fp);
    fclose(Fp);
#endif //Debug
	
	::memcpy(filtMagUpdateMap->getPointer(0,0),magUpdateMap->getPointer(0,0),m_Geometry->N_x * m_Geometry->N_z*sizeof(Real_t));
	
    for (int16_t i = 0; i < m_Geometry->N_z; i++)
    {
        for (int16_t j = 0; j < m_Geometry->N_x; j++)
        {
            filter_op = 0;
            for (int16_t p = -2; p <= 2; p++)
            {
                for (int16_t q = -2; q <= 2; q++)
                {
                    if(i + p >= 0 && i + p < m_Geometry->N_z && j + q >= 0 && j + q < m_Geometry->N_x)
                    {
                		filter_op += m_HammingWindow[p + 2][q + 2] * magUpdateMap->getValue(i + p, j + q);
                    }
                }
            }
            filtMagUpdateMap->setValue(filter_op, i, j);
        }
    }

	
//	::memcpy(magUpdateMap->getPointer(0,0),filtMagUpdateMap->getPointer(0,0),m_Geometry->N_x * m_Geometry->N_z*sizeof(Real_t));
/*    for (int16_t i = 0; i < m_Geometry->N_z; i++)
    {
        for (int16_t j = 0; j < m_Geometry->N_x; j++)
        {
            magUpdateMap->setValue(filtMagUpdateMap->getValue(i, j), i, j);
        }
    }*/
	
#ifdef DEBUG
    MAKE_OUTPUT_FILE(Fp, m_TomoInputs->tempDir, ScaleOffsetCorrection::FilteredMagMapFile);
    if(errno < 0)
    {

    }
    fwrite(filtMagUpdateMap->getPointer(0, 0), m_Geometry->N_x * m_Geometry->N_z, sizeof(Real_t), Fp);
    fclose(Fp);
#endif //Debug
}

//NHICD routines

#ifdef NHICD

// -----------------------------------------------------------------------------
// Sort the entries of filtMagUpdateMap and set the threshold to be ? percentile
// -----------------------------------------------------------------------------
Real_t ReconstructionEngine::SetNonHomThreshold(RealImageType::Pointer magUpdateMap)
{
    size_t dims[1] =
    { m_Geometry->N_z * m_Geometry->N_x};
    RealArrayType::Pointer TempMagMap = RealArrayType::New(dims, "TempMagMap");

    uint32_t ArrLength = m_Geometry->N_z * m_Geometry->N_x;
    Real_t threshold;

	//Copy into a linear list for easier partial sorting
	Real_t* src = magUpdateMap->getPointer();
	Real_t* dest = TempMagMap->getPointer();
	::memcpy(dest,src, ArrLength*sizeof(Real_t));
	
#ifdef DEBUG	
	Real_t TempSum=0;
	for(uint32_t i=0;i < ArrLength;i++)
		TempSum+=TempMagMap->d[i];
	
	std::cout<<"Temp mag map average= "<<TempSum/ArrLength<<std::endl;
#endif //DEBUG
	
	/* for (uint32_t i = 0; i < m_Geometry->N_z; i++)
        for (uint32_t j = 0; j < m_Geometry->N_x; j++)
        {
            //TempMagMap->d[i*geometry->N_x+j]=i*geometry->N_x+j;
            TempMagMap->d[i * (uint32_t)m_Geometry->N_x + j] = magUpdateMap->getValue(i, j);
        } */   
	
    uint32_t percentile_index = ArrLength / NUM_NON_HOMOGENOUS_ITER;
	std::cout<<"Percentile to select= "<<percentile_index<<std::endl;
	
    //Partial selection sort

  /*  Real_t max;
    uint32_t max_index;
    for (uint32_t i = 0; i <= percentile_index; i++)
    {
        max = TempMagMap->d[i];
        max_index = i;
        for (uint32_t j = i + 1; j < ArrLength; j++)
        {
            if(TempMagMap->d[j] > max)
            {
                max = TempMagMap->d[j];
                max_index = j;
            }
        }
        Real_t temp = TempMagMap->d[i];
        TempMagMap->d[i] = TempMagMap->d[max_index];
        TempMagMap->d[max_index] = temp;
    }
	threshold = TempMagMap->d[percentile_index];*/
	
	threshold = RandomizedSelect(TempMagMap,0, ArrLength-1,ArrLength-percentile_index);

    return threshold;
}

//Function to generate a list of voxels based on the NH threshold
struct List ReconstructionEngine::GenNonHomList(Real_t NHThresh, RealImageType::Pointer magUpdateMap)
{	
	
	int32_t MaxNumElts = m_Geometry->N_z*m_Geometry->N_x; 
	struct List TempList;
	TempList.NumElts=0;
	TempList.Array=(struct ImgIdx*)(malloc(MaxNumElts*sizeof(struct ImgIdx)));	
	for (int16_t j = 0; j < m_Geometry->N_z; j++)
    {
        for (int16_t k = 0; k < m_Geometry->N_x; k++)
        {
            if(magUpdateMap->getValue(j,k) > NHThresh)
			{
				
				TempList.Array[TempList.NumElts].xidx = k;
				TempList.Array[TempList.NumElts].zidx = j;
				TempList.NumElts++;
			}
            
        }
    }
	
	struct List NHList = GenRandList(TempList);
	free(TempList.Array);
	
	std::cout<<"Number of elements in the NH list ="<<NHList.NumElts<<std::endl;
	
	return NHList;
	
}

#endif //NHICD function defines

//Function to generate a list of voxels in sequential order 
//for a given geometry object
void ReconstructionEngine::GenRegularList(struct List* InpList)
{
	int32_t iter = 0;
	for (int16_t j = 0; j < m_Geometry->N_z; j++)
        for (int16_t k = 0; k < m_Geometry->N_x; k++)
		{
		InpList->Array[iter].zidx = j;
		InpList->Array[iter].xidx = k;
		iter++;
		}
	
}

//Function to generate a randomized list from a given input list
//TODO: This function can cause memory leaks!! 

struct List ReconstructionEngine::GenRandList(struct List InpList)
{
	struct List OpList; 
	
	const uint32_t rangeMin = 0;
	const uint32_t rangeMax = std::numeric_limits<uint32_t>::max();
	typedef boost::uniform_int<uint32_t> NumberDistribution;
	typedef boost::mt19937 RandomNumberGenerator;
	typedef boost::variate_generator<RandomNumberGenerator&,NumberDistribution> Generator;
	
	NumberDistribution distribution(rangeMin, rangeMax);
	RandomNumberGenerator generator;
	Generator numberGenerator(generator, distribution);	
	boost::uint32_t arg = static_cast<boost::uint32_t>(EIMTOMO_getMilliSeconds());
	generator.seed(arg); // seed with the current time
	
	int32_t ArraySize = InpList.NumElts;
	
	OpList.NumElts = ArraySize;
	OpList.Array = (struct ImgIdx*)(malloc(ArraySize*sizeof(struct ImgIdx)));
	
	size_t dims[3] =
	{ ArraySize, 0, 0};
	Int32ArrayType::Pointer Counter = Int32ArrayType::New(dims, "Counter");
	
	for (int32_t j_new = 0; j_new < ArraySize; j_new++)
	{
		Counter->d[j_new] = j_new;
	}
	
	for(int32_t test_iter = 0;test_iter < InpList.NumElts;test_iter++)
	{
		int32_t Index = numberGenerator() % ArraySize;
		OpList.Array[test_iter] = InpList.Array[Counter->d[Index]];
		Counter->d[Index] = Counter->d[ArraySize - 1];
		ArraySize--;
	}
	
#ifdef DEBUG
	std::cout<<"Input"<<std::endl;
	maxList(InpList);
	
	std::cout<<"Output"<<std::endl;
	maxList(OpList);
	
#endif //Debug
	
	return OpList;
	
}


//Radomized select based on code from : http://stackoverflow.com/questions/5847273/order-statistic-implmentation


Real_t ReconstructionEngine::RandomizedSelect(RealArrayType::Pointer A,uint32_t p, uint32_t r,uint32_t i)
{
	
	//std::cout<<"***********Select****************"<<std::endl;
	//std::cout<<p<<"  "<<r<<"  "<<i<<std::endl;
	if (p == r)
    {
		return A->d[p];
    }
	uint32_t q = RandomizedPartition(A, p, r);
	uint32_t k = q - p + 1;
	if (i == k)
    {
		return A->d[q];
    }
	else if  (i < k)
    {
		return RandomizedSelect(A, p, q-1, i) ;
    }
	else return RandomizedSelect(A, q+1, r, i - k);
}
uint32_t ReconstructionEngine::Partition(RealArrayType::Pointer A,uint32_t p,uint32_t r)
{
	Real_t x=A->d[r],temp;
	uint32_t i=p-1,j;
	for(j=p;j<r;j++)
    {
		if(A->d[j]<=x)
        {
			i++;
			temp=A->d[i];
			A->d[i]=A->d[j];
			A->d[j]=temp;
        }
    }
	temp=A->d[i+1];
	A->d[i+1]=A->d[r];
	A->d[r]=temp;
	return i+1;
	
}
uint32_t ReconstructionEngine::RandomizedPartition(RealArrayType::Pointer A,uint32_t p,uint32_t r)
{
	
	const uint32_t rangeMin = 0;
	const uint32_t rangeMax = std::numeric_limits<uint32_t>::max();
	typedef boost::uniform_int<uint32_t> NumberDistribution;
	typedef boost::mt19937 RandomNumberGenerator;
	typedef boost::variate_generator<RandomNumberGenerator&,NumberDistribution> Generator;
	
	NumberDistribution distribution(rangeMin, rangeMax);
	RandomNumberGenerator generator;
	Generator numberGenerator(generator, distribution);
	boost::uint32_t arg = static_cast<boost::uint32_t>(EIMTOMO_getMilliSeconds());
	generator.seed(arg); // seed with the current time
	
	Real_t temp;	
	uint32_t j = p + numberGenerator()%(r-p+1);//rand()%(r-p+1);
	temp = A->d[r];
	A->d[r] = A->d[j];
	A->d[j] = temp;	
	return Partition(A, p, r);
}

Real_t ReconstructionEngine::roiVolumeSum(UInt8Image_t::Pointer Mask)
{
		
	Real_t sum = 0;
	uint32_t counter = 0;
	for(uint16_t j = 0; j < m_Geometry->N_z; j++)
		for(int16_t k = 0; k < m_Geometry->N_x; k++)
			if(Mask->getValue(j,k) == 1)
			{
				counter++;
				for (uint16_t i = 0; i < m_Geometry->N_y; i++)
					sum+=m_Geometry->Object->getValue(j,k,i);
			}
	std::cout<<"Number of elements in the roiVolumeSum = "<<counter<<std::endl;
	return sum;
}

uint8_t ReconstructionEngine::stopCriteria(RealImageType::Pointer magUpdateMap,UInt8Image_t::Pointer magUpdateMask, Real_t PrevMagSum, uint32_t EffIterCount)
{

	uint8_t exit_status = 1;
#ifdef DEBUG
	Real_t TempSum = 0;
	for (int32_t j = 0; j < m_Geometry->N_z; j++)
		for (int32_t k = 0; k < m_Geometry->N_x; k++)
		{
			TempSum += magUpdateMap->getValue(j,k);
		}
	std::cout<<"**********************************************"<<std::endl;
	std::cout<<"Average mag after voxel update"<<TempSum/(m_Geometry->N_z*m_Geometry->N_x)<<std::endl;
	std::cout<<"**********************************************"<<std::endl;
#endif //debug
	
#ifdef NHICD	
	//In non homogenous mode check stopping criteria only after a full equit = equivalet iteration
	if(EffIterCount%NUM_NON_HOMOGENOUS_ITER == 0 && EffIterCount > 0 && PrevMagSum > 0)
#else
	if(EffIterCount > 0 && PrevMagSum > 0)
#endif //NHICD
	{
		
		Real_t TempSum = 0;
		uint32_t counter = 0;
		for (int32_t j = 0; j < m_Geometry->N_z; j++)
			for (int32_t k = 0; k < m_Geometry->N_x; k++)
				if(magUpdateMask->getValue(j,k) == 1)
				{
					TempSum += magUpdateMap->getValue(j,k);
					counter++;
				}
		
		std::cout<<"Number of elements in the ROI after an equit= "<<counter<<std::endl;
		
		if(getVerbose())
		{
			std::cout<<"************************************************"<<std::endl;
			std::cout << EffIterCount + 1 << " " << (fabs(TempSum)/ fabs(PrevMagSum))<< std::endl;
			std::cout<<"************************************************"<<std::endl;
		}
		if( (fabs(TempSum)/ fabs(PrevMagSum)) < m_TomoInputs->StopThreshold && EffIterCount > 0)
		{
			std::cout << "This is the terminating point " << EffIterCount << std::endl;
			m_TomoInputs->StopThreshold *= m_AdvParams->THRESHOLD_REDUCTION_FACTOR; //Reducing the thresold for subsequent iterations
			std::cout << "New threshold" << m_TomoInputs->StopThreshold << std::endl;
			exit_status = 0;
		}
		
	}
/* #else
	if(AverageMagnitudeOfRecon > 0)
	{
		if(getVerbose())
		{
			std::cout << Iter + 1 << " " << AverageUpdate / AverageMagnitudeOfRecon << std::endl;
		}
		//Use the stopping criteria if we are performing a full update of all voxels
		if((AverageUpdate / AverageMagnitudeOfRecon) < m_TomoInputs->StopThreshold && updateType != VoxelUpdateType::NonHomogeniousUpdate)
		{
			std::cout << "This is the terminating point " << Iter << std::endl;
			m_TomoInputs->StopThreshold *= m_AdvParams->THRESHOLD_REDUCTION_FACTOR; //Reducing the thresold for subsequent iterations
			std::cout << "New threshold" << m_TomoInputs->StopThreshold << std::endl;
			exit_status = 0;
		}
	} */
//#endif //NHICD
	
	return exit_status;
}

