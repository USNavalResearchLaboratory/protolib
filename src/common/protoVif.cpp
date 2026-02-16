/**
* @file protoVif.cpp
* 
* @brief Extends ProtoChannel to provide virtual interface access.
*/
#include "protoVif.h"

ProtoVif::ProtoVif()
 : user_data(NULL)
{
    vif_name[0] = '\0';
    vif_name[VIF_NAME_MAX] = '\0';  // to guarantee null termination
}

ProtoVif::~ProtoVif()
{
    if (IsOpen()) Close();
}
