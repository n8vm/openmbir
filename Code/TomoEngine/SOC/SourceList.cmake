#--////////////////////////////////////////////////////////////////////////////
#-- Copyright (c) 2011, Michael A. Jackson. BlueQuartz Software
#-- All rights reserved.
#-- BSD License: http://www.opensource.org/licenses/bsd-license.html
#-- This code was partly written under US Air Force Contract FA8650-07-D-5800
#--////////////////////////////////////////////////////////////////////////////
set (TomoEngine_SOC_SRCS
    ${TomoEngine_SOURCE_DIR}/SOC/ForwardProject.cpp
    ${TomoEngine_SOURCE_DIR}/SOC/SOCEngine.cpp
    ${TomoEngine_SOURCE_DIR}/SOC/SOCEngine_UpdateVoxels.cpp
    ${TomoEngine_SOURCE_DIR}/SOC/SOCEngine_Extra.cpp
    ${TomoEngine_SOURCE_DIR}/SOC/SOCInputs.cpp
    ${TomoEngine_SOURCE_DIR}/SOC/MultiResolutionSOC.cpp
    ${TomoEngine_SOURCE_DIR}/SOC/QGGMRF_Functions.cpp
)

set (TomoEngine_SOC_HDRS
    ${TomoEngine_SOURCE_DIR}/SOC/ForwardProject.h
    ${TomoEngine_SOURCE_DIR}/SOC/SOCConstants.h
    ${TomoEngine_SOURCE_DIR}/SOC/SOCEngine.h
    ${TomoEngine_SOURCE_DIR}/SOC/SOCInputs.h
    ${TomoEngine_SOURCE_DIR}/SOC/SOCStructures.h
    ${TomoEngine_SOURCE_DIR}/SOC/MultiResolutionSOC.h
    ${TomoEngine_SOURCE_DIR}/SOC/QGGMRF_Functions.h
)
set_source_files_properties( ${TomoEngine_SOURCE_DIR}/SOC/SOCEngine_UpdateVoxels.cpp
                             ${TomoEngine_SOURCE_DIR}/SOC/SOCEngine_Extra.cpp
                             PROPERTIES HEADER_FILE_ONLY TRUE)
cmp_IDE_SOURCE_PROPERTIES( "TomoEngine/SOC" "${TomoEngine_SOC_HDRS}" "${TomoEngine_SOC_SRCS}" "${CMP_INSTALL_FILES}")

