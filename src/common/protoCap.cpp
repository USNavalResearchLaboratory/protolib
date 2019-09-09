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
#include "protoDebug.h"

// (TBD) How bad would it really be to inline these in the "ProtoCap" class definition?
//       (The we could get rid of this file)

/**
* @brief Enables input notification by default
*
*/
ProtoCap::ProtoCap()
 :   if_index(0), user_data(NULL)
{
    // Enable input notification by default for ProtoCap
    StartInputNotification();
}


ProtoCap::~ProtoCap()
{
    if (IsOpen()) Close();
}


/**
 * @brief Changes the source mac addr to our own and writes packet to the pcap device
 *
 * 802.3 frames are not supported
 *
 * @param buffer
 * @param buflen
 *
 * @return success or failure indicator 
 */
bool ProtoCap::Forward(char* buffer, unsigned int& numBytes)
{
    // Change the src MAC addr to our own
    // (TBD) allow caller to specify dst MAC addr ???
    memcpy(buffer+6, if_addr.GetRawHostAddress(), 6);
    return Send(buffer, numBytes);
}  // end ProtoCap::Forward()



/**
 * @brief Changes the source mac addr to specified srcMacAddr and writes packet pcap device
 *
 * 802.3 frames are not supported
 *
 * @param buffer
 * @param buflen
 *
 * @return success or failure indicator 
 */        
bool ProtoCap::ForwardFrom(char* buffer, unsigned int& numBytes, const ProtoAddress& srcMacAddr)
{
    // Change the src MAC addr to our own
    // (TBD) allow caller to specify dst MAC addr ???
    memcpy(buffer+6, srcMacAddr.GetRawHostAddress(), 6);
    return Send(buffer, numBytes);
}  // end ProtoCap::ForwardFrom()
