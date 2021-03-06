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
#ifndef GAINSOFFSETSREADER_H_
#define GAINSOFFSETSREADER_H_

#include "MXA/Common/MXASetGetMacros.h"

#include "MBIRLib/MBIRLib.h"
#include "MBIRLib/GenericFilters/TomoFilter.h"


/**
 * @brief The GainsOffsetsReader class
 */
class MBIRLib_EXPORT GainsOffsetsReader : public TomoFilter
{
  public:
    MXA_SHARED_POINTERS(GainsOffsetsReader)
    MXA_STATIC_NEW_MACRO(GainsOffsetsReader)
    MXA_STATIC_NEW_SUPERCLASS(TomoFilter, GainsOffsetsReader)
    MXA_TYPE_MACRO_SUPER(GainsOffsetsReader, TomoFilter)

    virtual ~GainsOffsetsReader();

    // This is an input parameter and is defaulted to 0.0
    MXA_INSTANCE_PROPERTY(Real_t, TargetGain)


    // These are the output arrays that are read from the files
    MXA_INSTANCE_PROPERTY(RealArrayType::Pointer, InitialGain)
    MXA_INSTANCE_PROPERTY(RealArrayType::Pointer, InitialOffset)
    MXA_INSTANCE_PROPERTY(RealArrayType::Pointer, InitialVariance)

    virtual void execute();

  protected:
    GainsOffsetsReader();

  private:
    GainsOffsetsReader(const GainsOffsetsReader&); // Copy Constructor Not Implemented
    void operator=(const GainsOffsetsReader&); // Operator '=' Not Implemented
};


#endif /* GAINSOFFSETSREADER_H_ */
