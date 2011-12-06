//#include "ComputationEngine.h"
#include "ComputationInputs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
//#include "randlib.h"
clock_t startm, stopm;
#define START if ( (startm = clock()) == -1) {printf("Error calling clock");exit(1);}
#define STOP if ( (stopm = clock()) == -1) {printf("Error calling clock");exit(1);}
#define PRINTTIME printf( "%6.3f seconds used by the processor.\n", ((double)stopm-startm)/CLOCKS_PER_SEC);

/*
 CE => Computation Engine
 CI => Computation Inputs
 N_ => Number of ..
 
 */



int main(int argc,char** argv)
{
	int16_t error,i,j,k;
	FILE* Fp;
	TomoInputs ParsedInput;
	Sinogram Sinogram;
	Geometry Geometry;
	double *buffer=(double*)get_spc(1,sizeof(double));
	
	START;
	
	error=-1;
	error=parseArguments(argc,argv,&ParsedInput);
	if(error != -1)
	{
		printf("%s %s %s %s\n",ParsedInput.ParamFile,ParsedInput.SinoFile,ParsedInput.InitialRecon,ParsedInput.OutputFile);
		//Read the paramters into the structures
		Fp = fopen(ParsedInput.ParamFile,"r");
		readParameterFile(Fp,&ParsedInput,&Sinogram,&Geometry);
		fclose(Fp);
		//Based on the inputs , calculate the "other" variables in the structure definition
		initializeSinoParameters(&Sinogram,&ParsedInput);
		initializeGeomParameters(&Sinogram,&Geometry,&ParsedInput);
	}
	
	
	
	error=CE_MAPICDReconstruct(&Sinogram,&Geometry,&ParsedInput);
	Fp=fopen(ParsedInput.OutputFile,"w");
	
	printf("Main\n");
	printf("Final Dimensions of Object Nz=%d Nx=%d Ny=%d\n",Geometry.N_z,Geometry.N_x,Geometry.N_y);
	
	for(i = 0;i < Geometry.N_y; i++)
	{
		for(j = 0;j < Geometry.N_x; j++)
			for(k = 0;k < Geometry.N_z; k++)
			{
				buffer = &Geometry.Object[k][j][i];
				fwrite(buffer,sizeof(double),1,Fp);
			}
		printf("%d\n",i);
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

