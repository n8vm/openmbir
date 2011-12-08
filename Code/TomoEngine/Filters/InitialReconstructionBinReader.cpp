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

#include "InitialReconstructionBinReader.h"

#include "TomoEngine/SOC/SOCConstants.h"
#include "TomoEngine/Common/EIMMath.h"


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
InitialReconstructionBinReader::InitialReconstructionBinReader()
{
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
InitialReconstructionBinReader::~InitialReconstructionBinReader()
{
}



// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void InitialReconstructionBinReader::execute()
{
  SuperClass::execute();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void InitialReconstructionBinReader::initializeData()
{
  TomoInputs* input = getInputs();
  Geometry* geometry = getGeometry();

  //Read the Initial Reconstruction data into a 3-D matrix
  FILE* Fp=fopen(input->InitialReconFile.c_str(),"r");
  std::cout<<"Reading Geom"<<std::endl;
  if(NULL != Fp)
  {
    for (uint16_t i = 0; i < geometry->N_y; i++)
    {
      for (uint16_t j = 0; j < geometry->N_x; j++)
      {
        for (uint16_t k = 0; k < geometry->N_z; k++)
        {
          DATA_TYPE buffer;
          fread((unsigned char*)(&buffer), sizeof(DATA_TYPE), 1, Fp);
          geometry->Object->d[k][j][i] = buffer;
        }
      }
    }
    fclose(Fp);
    notify("Done Reading Initial Reconstruction", 0, UpdateProgressMessage);
  }
}
