/** 
 * @file protoCap.cpp
 * @brief Protolib Generic base class for simple link layer packet capture (ala libpcap) and raw link layer packet transmission.
 */

/**
 * @class ProtoCap
 *
 * @brief Generic base class for simple link layer packet capture (ala libpcap) and raw link layer packet transmission.
 */

#include "protoCap.h"

// (TBD) How bad would it really be to inline these in the "ProtoCap" class definition?
//       (The we could get rid of this file)

/**
* @brief Enables input notification by default
*
*/
ProtoCap::ProtoCap()
 :   if_index(-1), user_data(NULL)
{
    // Enable input notification by default for ProtoCap
    StartInputNotification();
}


ProtoCap::~ProtoCap()
{
    if (IsOpen()) Close();
}
