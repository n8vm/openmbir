// C Includes
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

// C++ includes
#include <string>
#include <iostream>

// EIMTomo Includes
#include "EIMTomo/EIMTomo.h"
#include "EIMTomo/common/EIMTime.h"
#include "EIMTomo/common/allocate.h"

// MXA Includes
#include "MXA/Utilities/MXADir.h"
#include "ScaleOffsetCorrectionConstants.h"
#include "ScaleOffsetCorrectionParser.h"
#include "ScaleOffsetCorrectionEngine.h"

#define START startm = EIMTOMO_getMilliSeconds();
#define STOP stopm = EIMTOMO_getMilliSeconds();
#define PRINTTIME printf( "%6.3f seconds used by the processor.\n", ((double)stopm-startm)/1000.0);

/*double
 CE => Computation Engine
 CI => Computation Inputs
 N_ => Number of ..

 */
#define MAKE_OUTPUT_FILE(Fp, err, outdir, filename)\
    {\
    std::string filepath(outdir);\
    filepath =+ MXADir::Separator;\
    filepath.append(filename);\
    Fp = fopen(filepath.c_str(),"wb");\
    if (Fp == NULL) { std::cout << "Error Opening Output file " << filepath << std::endl; err = 1; }\
    }

int main(int argc, char** argv)
{
  int16_t error, i, j, k;
  FILE* Fp = NULL;
  TomoInputs ParsedInput;
  Sino Sinogram;
  Geom Geometry;
  DATA_TYPE* buffer = (DATA_TYPE*)get_spc(1, sizeof(DATA_TYPE));
  uint64_t startm;
  uint64_t stopm;

  START;

  error = -1;
  ScaleOffsetCorrectionParser soci;

  error = soci.parseArguments(argc, argv, &ParsedInput);
  if(error != -1)
  {
    printf("***************\nParameter File: %s\nSinogram File: %s\nInitial Reconstruction File: %s\nOutput Directory: %s\nOutput File: %s\n***************\n",
           ParsedInput.ParamFile.c_str(),
           ParsedInput.SinoFile.c_str(),
           ParsedInput.InitialRecon.c_str(),
           ParsedInput.outputDir.c_str(),
           ParsedInput.OutputFile.c_str());

    // Make sure the output directory is created if it does not exist
    if(MXADir::exists(ParsedInput.outputDir) == false)
    {
      std::cout << "Output Directory '" << ParsedInput.outputDir << "' does NOT exist. Attempting to create it." << std::endl;
      if(MXADir::mkdir(ParsedInput.outputDir, true) == false)
      {
        std::cout << "Error creating the output directory '" << ParsedInput.outputDir << "'\n   Exiting Now." << std::endl;
        return EXIT_FAILURE;
      }
      std::cout << "Output Directory Created." << std::endl;
    }

    //Read the paramters into the structures
    Fp = fopen(ParsedInput.ParamFile.c_str(), "r");
    if(errno)
    {
      std::cout << "Error Opening File: " << errno << std::endl;
      return EXIT_FAILURE;
    }

    soci.readParameterFile(Fp, &ParsedInput, &Sinogram, &Geometry);
    fclose(Fp);
    //Based on the inputs , calculate the "other" variables in the structure definition
    soci.initializeSinoParameters(&Sinogram, &ParsedInput);
    //CI_MaskSinogram(&OriginalSinogram,&MaskedSinogram);
    soci.initializeGeomParameters(&Sinogram, &Geometry, &ParsedInput);
  }

  SOCEngine soce(&Sinogram, &Geometry, &ParsedInput);
  error = soce.mapicdReconstruct();
  if(error == 0)
  {
    std::cout << "Error During Reconstruction" << std::endl;
    return EXIT_FAILURE;
  }
  Fp = fopen(ParsedInput.OutputFile.c_str(), "w");

  printf("Main\n");
  printf("Final Dimensions of Object Nz=%d Nx=%d Ny=%d\n", Geometry.N_z, Geometry.N_x, Geometry.N_y);

  for (i = 0; i < Geometry.N_y; i++)
  {
    for (j = 0; j < Geometry.N_x; j++)
    {
      for (k = 0; k < Geometry.N_z; k++)
      {
        buffer = &Geometry.Object[k][j][i];
        fwrite(buffer, sizeof(DATA_TYPE), 1, Fp);
      }
    }
    printf("%d\n", i);
  }

  fclose(Fp);
  STOP;
  PRINTTIME;
  // if(error < 0)
  // {
  // //TODO:clean up memory here
  // return EXIT_FAILURE;
  // }
  // //TODO:free memory
  //
  // return EXIT_SUCCESS;
  return 0;
}

