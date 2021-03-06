/* ============================================================================
 * Copyright (c) 2011, Michael A. Jackson (BlueQuartz Software)
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
 * Neither the name of Michael A. Jackson nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
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
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#ifndef FILTERPIPELINE_H_
#define FILTERPIPELINE_H_


#include <string>
#include <vector>

#include "MXA/Common/MXASetGetMacros.h"

#include "MBIRLib/MBIRLib.h"

#include "MBIRLib/Common/Observer.h"
#include "MBIRLib/Common/AbstractFilter.h"

/*
 *
 */
class MBIRLib_EXPORT FilterPipeline : public Observer
{
  public:
    MXA_SHARED_POINTERS(FilterPipeline)
    MXA_TYPE_MACRO_SUPER(FilterPipeline, Observer)
    MXA_STATIC_NEW_MACRO(FilterPipeline)

    virtual ~FilterPipeline();

    typedef std::vector<AbstractFilter::Pointer>  FilterContainerType;

    MXA_INSTANCE_PROPERTY(int, ErrorCondition)

    /**
     * @brief Cancel the operation
     */
    virtual void setCancel(bool value);
    virtual bool getCancel();

    /**
     * @brief This method is called to start the pipeline for a plugin
     */
    virtual void run();

    /**
     * @brief A pure virtual function that gets called from the "run()" method. Subclasses
     * are expected to create a concrete implementation of this method.
     */
    virtual void execute();

    /**
     * @brief This method is called from the run() method just before exiting and
     * signals the end of the pipeline execution
     */
    virtual void pipelineFinished();

    // virtual void printFilterNames(std::ostream &out);


  protected:
    FilterPipeline();

  private:
    bool m_Cancel;

    FilterPipeline(const FilterPipeline&); // Copy Constructor Not Implemented
    void operator=(const FilterPipeline&); // Operator '=' Not Implemented
};

#endif /* FILTERPIPELINE_H_ */
