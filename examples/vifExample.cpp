#include "protoApp.h"
#include "protoVif.h"
#include "protoCap.h"
#include "protoPktIP.h"
#include "protoPktETH.h"
#include "protoPktARP.h"
#include "protoTimer.h"
#include "protoPipe.h"
#include "protoNet.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h> //for "isspace()"
#ifdef WIN32
#include <Iptypes.h>  // For MAX_ADAPTER_NAME_LENGTH
#endif //WIN32
// rxlimit smoothing factors -- weights for new data
#define RXLIMIT_SIZE_ALPHA 0.1   // for average packet length
#define RXLIMIT_RATE_ALPHA 0.1  // for average rate measurement

// rxlimit alpha modifiers
//     if sample datarate is > factor*rxlimit or < rxlimit/factor, 
//     alpha will be increased to more quickly adjust average rate
#define RXLIMIT_ALPHA_RATEFACTOR 2.0

// minimum time (sec) to update average datarate
//     if this is set too small, packets received very close together
//     may have an abnormally high instantaneous rate, throwing off the
//     average rate measurement despite the smoothing factor.  This
//     serves to further smooth out the instantaneous rate.  Another way
//     to look at this is that the drop probability will be updated at
//     most 1/RXLIMIT_MIN_UPDATE_TIME times per second.
# define RXLIMIT_MIN_UPDATE_TIME 0.01

/**
 * @class VifExampleApp
 *
 * @brief Example using protoVif
 *
 */
class VifExampleApp : public ProtoApp
{
    public:
      VifExampleApp();
      ~VifExampleApp();

      bool OnStartup(int argc, const char*const* argv);
      bool ProcessCommands(int argc, const char*const* argv);
      void OnShutdown();
      
      enum Mode
      {
          BRIDGE,
          CLONE,
          REPLACE
      };

    private:
      enum CmdType {CMD_INVALID, CMD_ARG, CMD_NOARG};
      static const char* const CMD_LIST[];
      static CmdType GetCmdType(const char* string);
      bool OnCommand(const char* cmd, const char* val);        
      void Usage();
            
      // Generate random number in range 0..max
      double UniformRand(double max = 1.0)
          {return (max * ((double)rand() / (double)RAND_MAX));}
      
      bool OnTxRateTimeout(ProtoTimer& theTimer);
      
      void OnControlMsg(ProtoSocket&       thePipe, 
                        ProtoSocket::Event theEvent);
      
      void PeekPkt(ProtoPktETH& ethPkt, bool inbound);

      void OnOutboundPkt(ProtoChannel& theChannel,
                         ProtoChannel::Notification notifyType);
      void OnInboundPkt(ProtoChannel& theChannel,
                        ProtoChannel::Notification notifyType);
      
      ProtoVif*      vif;
      ProtoCap*      cap;

      char           vif_name[ProtoVif::VIF_NAME_MAX]; 
      ProtoAddress   vif_addr;
      ProtoAddress   vif_hwaddr;
      unsigned int   vif_mask_len;
      char           if_name[ProtoVif::VIF_NAME_MAX];
#ifdef WIN32
      char           if_friendly_name[MAX_ADAPTER_NAME_LENGTH];
	  bool           dhcp_enabled;
#endif //WIN32
      Mode           vif_mode;
      double         txrate_limit;  // in kbps
      double         rxrate_limit;  // in kbps
      double         rxrate_limit_uppernominal;  // kbps; above this level alpha will be increased
      double         rxrate_limit_lowernominal;  // kbps; below this level alpha will be increased
      ProtoTimer     txrate_timer;
      ProtoTime      rxlimit_prev_pkt_time;
      bool           rxlimit_prev_pkt_time_initialized;
      double         rxlimit_avg_rate;
      double         rxlimit_drop_prob;
      double         rxlimit_avg_size;
      unsigned int   rxlimit_rxbytes;
      double         tx_loss;
      double         rx_loss;
      
      ProtoPipe      control_pipe;   // pipe I listen to
        
  /**
   * We cc the smf "mne blocking" support here as
   * a hack to use vifExample in MNE as a virtual "Mobile" 
   * interface
   */
      enum {MNE_BLOCK_MAX = 100};
      bool MneIsBlocking(const char* macAddr) const;
      char            mne_block_list[6*MNE_BLOCK_MAX];  
      unsigned int    mne_block_list_len;

}; // end class VifExampleApp

void VifExampleApp::Usage()
{
    fprintf(stderr, "Usage: vifExample vif <vif_name> [bridge|clone|replace] interface <if_name>\n"
                    "                  [addr <ifAddr>/<maskLength>][mac <macAddr>]\n"
                    "                  [rxrate <kbps>][txrate <kbps>][txloss <percent>]\n"
                    "                  [rxloss <percent>][instance <name>][help] \n\n"
                    "Note: Cloned interfaces use the real interface MAC address, while\n"
                    "      Bridged interfaces (default) use the virtual interface MAC addr\n"
                    "      (randomly assigned, unless \"mac\" command is given)\n");
}

const char* const VifExampleApp::CMD_LIST[] =
{
    "-help",        // print help info an exit
    "+vif",         // virtual interface name
    "+addr",        // <addr/maskLength> virtual interface address
    "+mac",         // MAC address (for bridge mode only)
    "+interface",   // name of real interface to which vif is "bridged"
    "-bridge",      // denotes a "bridged" interface (uses vif MAC)
    "-clone",       // denotes a "cloned" interface (uses real interface MAC)
    "-replace",     // vif overtakes interface configuration (addressing, etc), i.e., replaces the interface (original iface restored on exit)
    "+txrate",      // <kbps> set outbound data rate limit of vif (default unlimited)
    "+rxrate",      // <kbps> set inbound data rate limit of vif (default unlimited)
    "+txloss",      // <percent> packet drop probability applied to outbound pkts
    "+rxloss",      // <percent> packet drop probability applied to inbound pkts        
    "+instance",    // <instanceName> of "control pipe" to listen for commands
    "+debug",       // <debugLevel>
    NULL
};

/**
 * This macro creates our ProtoApp derived application instance 
 */
PROTO_INSTANTIATE_APP(VifExampleApp) 

VifExampleApp::VifExampleApp()
: vif(NULL), cap(NULL), vif_mask_len(0), 
#ifdef WIN32
    dhcp_enabled(false),
#endif //WIN32
	vif_mode(BRIDGE), txrate_limit(-1.0), rxrate_limit(-1.0), 
    rxrate_limit_uppernominal(-1.0), rxrate_limit_lowernominal(-1.0), 
    rxlimit_prev_pkt_time_initialized(false), rxlimit_avg_rate(0.0), 
    rxlimit_drop_prob(0.0), rxlimit_avg_size(0.0), rxlimit_rxbytes(0), 
    tx_loss(0.0), rx_loss(0.0),
    control_pipe(ProtoPipe::MESSAGE), mne_block_list_len(0)
{
    vif_name[0] = '\0'; 
    if_name[0] = '\0'; 
#ifdef WIN32
	if_friendly_name[0] = '\0';
#endif //WIN32
    txrate_timer.SetListener(this, &VifExampleApp::OnTxRateTimeout);
    
    control_pipe.SetNotifier(&GetSocketNotifier());
    control_pipe.SetListener(this, &VifExampleApp::OnControlMsg);
}

VifExampleApp::~VifExampleApp()
{
}

VifExampleApp::CmdType VifExampleApp::GetCmdType(const char* cmd)
{
    if (!cmd) return CMD_INVALID;
    unsigned int len = strlen(cmd);
    bool matched = false;
    CmdType type = CMD_INVALID;
    const char* const* nextCmd = CMD_LIST;
    while (*nextCmd)
    {
        if (!strncmp(cmd, *nextCmd+1, len))
        {
            if (matched)
            {
                // ambiguous command (command should match only once)
                return CMD_INVALID;
            }
            else
            {
                matched = true;   
                if ('+' == *nextCmd[0])
                    type = CMD_ARG;
                else
                    type = CMD_NOARG;
            }
        }
        nextCmd++;
    }
    return type; 
}  // end VifExampleApp::GetCmdType()


bool VifExampleApp::OnStartup(int argc, const char*const* argv)
{
    // Seed rand() with time of day usec
    // (comment out for repeatable results)
    struct timeval currentTime;
    ProtoSystemTime(currentTime);
    srand((unsigned int)currentTime.tv_usec);
    
    if (!ProcessCommands(argc, argv))
    {
        PLOG(PL_ERROR, "VifExampleApp::OnStartup() error processing command line options\n");
        Usage();
        return false;   
    }
    
    // Check for valid parameters.
    if (0 == strlen(vif_name))
    {
        PLOG(PL_ERROR, "VifExampleApp::OnStartup() error: missing required 'vif' command!\n");
        Usage();
        return false;
    }
    if (0 == strlen(if_name))
    {
        PLOG(PL_ERROR, "VifExampleApp::OnStartup() error: missing required 'interface' command!\n");
        Usage();
        return false;
    }
    
    // Create our vif instance and initialize ...
    if (!(vif = ProtoVif::Create()))
    {
        PLOG(PL_ERROR, "VifExampleApp::OnStartup() new ProtoVif error: %s\n", GetErrorString());
        return false;
    }  

    // Create our cap instance and initialize ...
    if (!(cap = ProtoCap::Create()))
    {
        PLOG(PL_ERROR, "VifExampleApp::OnStartup() new ProtoCap error: %s\n", GetErrorString());
        return false;
    }  

    vif->SetNotifier(static_cast<ProtoChannel::Notifier*>(&dispatcher));
    vif->SetListener(this,&VifExampleApp::OnOutboundPkt);

    cap->SetNotifier(static_cast<ProtoChannel::Notifier*>(&dispatcher));
    cap->SetListener(this,&VifExampleApp::OnInboundPkt);
	
    if (!vif->Open(vif_name, vif_addr, vif_mask_len))
    {
        PLOG(PL_ERROR,"VifExampleApp::OnStartup() ProtoVif::Open(\"%s\") error\n", vif_name);
        Usage();
        return false;
    }

    if (!cap->Open(if_name))
    {
       PLOG(PL_ERROR,"VifExampleApp::OnStartup() ProtoCap::Open(\"%s\") error\n", if_name);
       Usage();
       return false;
    }
    
    switch (vif_mode)
    {
        case BRIDGE:
            // use vif MAC addr or one explicitly provided on command-line
            if (vif_hwaddr.IsValid())
            {
                if (!vif->SetHardwareAddress(vif_hwaddr))
                {
                    PLOG(PL_ERROR, "VifExampleApp::OnStartup() ProtoVif::SetHardwareAddress() error\n");
                    Usage();
                    return false;
                }
				// Seems we  need to reopen vif after setting hardware address?
				// Note that SetHardwareAddress is not currently working on windows 7
				// so this open is redundant on windows 7.  TBD: Remove?
#ifdef WIN32
				/*
				if (!vif->Open(vif_name, vif_addr, vif_mask_len))
				{
					PLOG(PL_ERROR, "VifExampleApp::OnStartup() ProtoVif::Open(\"%s\") error\n", vif_name);
					Usage();
					return false;
				}
				*/
#endif
				// Snag the virtual interface hardware address
				ProtoAddress tmp_hw_addr;
				if (!ProtoNet::GetInterfaceAddress(vif_name, ProtoAddress::ETH, tmp_hw_addr))
					PLOG(PL_ERROR, "Win32Vif::Open(%s) error: unable to get ETH address!\n", vif_name);

				PLOG(PL_INFO,"Snagged>%s from vif>%s\n", tmp_hw_addr.GetHostString(), vif_name);


            }
            break;
        case CLONE:
            // Will transmit frames using "interface" MAC addr
            break;
        case REPLACE:
        {
            // 1) Set vif MAC addr to same as "interface"
            if (!ProtoNet::GetInterfaceAddress(if_name, ProtoAddress::ETH, vif_hwaddr))
            {
                PLOG(PL_ERROR, "VifExampleApp::OnStartup() error getting interface MAC address\n");
                Usage();
                return false;
            }
            if (!vif->SetHardwareAddress(vif_hwaddr))
            {
                PLOG(PL_ERROR, "VifExampleApp::OnStartup() ProtoVif::SetHardwareAddress() error\n");
                Usage();
                return false;
            }
			// TBD: Seems like we need to reopen after changing hardware address?
			// Note that SetHardwareAddress is not currently working on windows 7
			// so this open is redundant on windows 7.  
#ifdef WIN32
			/*
			if (!vif->Open(vif_name, vif_addr, vif_mask_len))
			{
				PLOG(PL_ERROR, "VifExampleApp::OnStartup() ProtoVif::Open(\"%s\") error\n", vif_name);
				Usage();
				return false;
			}
			*/
#endif
            ProtoAddressList addrList;
            if (!ProtoNet::GetInterfaceAddressList(if_name, ProtoAddress::IPv4, addrList))
                PLOG(PL_WARN, "VifExampleApp::OnStartup() warning: no interface IPv4 addresses?\n");
            if (!ProtoNet::GetInterfaceAddressList(if_name, ProtoAddress::IPv6, addrList))
                PLOG(PL_WARN, "VifExampleApp::OnStartup() warning: no interface IPv6 addresses?\n");
#ifdef WIN32
			// On WIN32 after we move the interface ip address to the vif
			// interface subsequent calls using the original ifAddr ip address 
	        // will get the vif adapter - use the friendly name to prevent this.
			ProtoAddress ifAddr;
			if (ifAddr.ConvertFromString(if_name))
			{
				if (!ProtoNet::GetInterfaceFriendlyName(ifAddr, if_friendly_name, MAX_ADAPTER_NAME_LENGTH))
				{
					PLOG(PL_ERROR, "ProtoNet::GetInterfaceAddressList(%s) error: no matching interface name found\n", if_name);
					return false;
				}
				PLOG(PL_INFO, "ProtoNet::GetInterfaceAddressList() friendly if_name>%s\n", if_friendly_name);
			}

			// Also squirrel away the dhcp enabled boolean so we can restore correctly
			dhcp_enabled = ProtoNet::GetInterfaceAddressDhcp(if_name,ifAddr);
#endif //WIN32

            // TBD - check for empty addrList?
            ProtoAddress addr;
            ProtoAddressList::Iterator iterator(addrList);
#ifndef WIN32
            while (iterator.GetNextAddress(addr))
            {
				if (addr.IsLinkLocal()) continue;
                unsigned int maskLen = ProtoNet::GetInterfaceAddressMask(if_name, addr);
				// TODO: continue?
				if (maskLen == 0)
					continue;

                // Remove address from "interface"
#ifdef WIN32
				// On WIN32 after we move the interface ip address to the vif
				// address subsequent calls using the ifAddr will get the vif adapter - 
				// use the friendly name to prevent this.
                if (!ProtoNet::RemoveInterfaceAddress(if_friendly_name, addr, maskLen))
#else 
				if (!ProtoNet::RemoveInterfaceAddress(if_name,addr,maskLen))
#endif //if/else WIN32
                {
                    PLOG(PL_ERROR, "VifExampleApp::OnStartup() error removing address %s from interface %s\n", addr.GetHostString(), if_name);
                    Usage();
                    return false;
                }
                // Assign address to vif
                if (!ProtoNet::AddInterfaceAddress(vif_name, addr, maskLen))
                {
                    PLOG(PL_ERROR, "VifExampleApp::OnStartup() error adding address %s to vif %s\n", addr.GetHostString(), vif_name);
                    Usage();
                    return false;
                }
#ifdef WIN32
				// Set vif_addr to the new addr
				vif_addr = addr;
#endif //WIN32
            }
#endif // !WIN32
            break;
        }
    }
    
    PLOG(PL_INFO, "vifExample: running on virtual interface: %s\n", vif_name);

    return true;
}  // end VifExampleApp::OnStartup()

void VifExampleApp::OnShutdown()
{
    if (NULL != vif)
    {
        if (REPLACE == vif_mode)
        {
            // Assign vif address(es) back to "interface"
            ProtoAddressList addrList;
            if (!ProtoNet::GetInterfaceAddressList(vif_name, ProtoAddress::IPv4, addrList))
                PLOG(PL_ERROR, "VifExampleApp::OnShutdown() error getting vif IPv4 addresses\n");
            if (!ProtoNet::GetInterfaceAddressList(vif_name, ProtoAddress::IPv6, addrList))
                PLOG(PL_ERROR, "VifExampleApp::OnShutdown() error getting vif IPv6 addresses\n");

            ProtoAddress addr;
            ProtoAddressList::Iterator iterator(addrList);
#ifndef WIN32
            while (iterator.GetNextAddress(addr))
            {
				if (addr.IsLinkLocal()) continue;
				unsigned int maskLen = 0;
#ifdef WIN32
				// TODO: add option to GetInterfaceAddressMask to accept vif_name for lookup
				maskLen = ProtoNet::GetInterfaceAddressMask(vif_addr.GetHostString(), addr);
#else
                maskLen = ProtoNet::GetInterfaceAddressMask(vif_name, addr);
#endif //WIN32
				// TBD: continue?
				if (maskLen == 0)
					continue;
                // Remove address from vif
                if (!ProtoNet::RemoveInterfaceAddress(vif_name, addr, maskLen))
                    PLOG(PL_ERROR, "VifExampleApp::OnShutdown() error removing address %s from vif %s\n", addr.GetHostString(), vif_name);
                // Assign address to "interface"
#ifdef WIN32
				// TODO: Make common function prototypes
                if (!ProtoNet::AddInterfaceAddress(if_friendly_name, addr, maskLen, dhcp_enabled))
                    PLOG(PL_ERROR, "VifExampleApp::OnShutdown() error adding address %s to interface %s\n", addr.GetHostString(), if_friendly_name);
#else
				if (!ProtoNet::AddInterfaceAddress(if_name, addr, maskLen))
					PLOG(PL_ERROR, "VifExampleApp::OnShutdown() error adding address %s to interface %s\n", addr.GetHostString(), if_name);
#endif //WIN32
            }  
#endif // !WIN32
        }     
            vif->Close();
            delete vif;
            vif = NULL;
    }
    
    if (NULL != cap)
    {
        cap->Close();
        delete cap;
        cap = NULL;
    }
    
    PLOG(PL_INFO, "vifExample: Done.\n"); 

}  // end VifExampleApp::OnShutdown()

bool VifExampleApp::ProcessCommands(int argc, const char*const* argv)
{
    // Dispatch command-line commands to our OnCommand() method
    int i = 1;
    while ( i < argc)
    {
        // Is it a vifExample command?
        switch (GetCmdType(argv[i]))
        {
            case CMD_INVALID:
            {
                PLOG(PL_ERROR, "VifExampleApp::ProcessCommands() Invalid command:%s\n", 
                        argv[i]);
                Usage();
                return false;
            }
            case CMD_NOARG:
                if (!OnCommand(argv[i], NULL))
                {
                    PLOG(PL_ERROR, "VifExampleApp::ProcessCommands() ProcessCommand(%s) error\n", 
                            argv[i]);
                    Usage();
                    return false;
                }
                i++;
                break;
            case CMD_ARG:
                if (!OnCommand(argv[i], argv[i+1]))
                {
                    PLOG(PL_ERROR, "VifExampleApp::ProcessCommands() ProcessCommand(%s, %s) error\n", 
                            argv[i], argv[i+1]);
                    Usage();
                    return false;
                }
                i += 2;
                break;
        }
    }
    return true;  
}  // end VifExampleApp::ProcessCommands()

bool VifExampleApp::OnCommand(const char* cmd, const char* val)
{
    // (TBD) move command processing into Mgen class ???
    CmdType type = GetCmdType(cmd);
    ASSERT(CMD_INVALID != type);
    size_t len = strlen(cmd);
    if ((CMD_ARG == type) && !val)
    {
        PLOG(PL_ERROR, "VifExampleApp::ProcessCommand(%s) missing argument\n", cmd);
        Usage();
        return false;
    }
    else if (!strncmp("help", cmd, len))
    {
        Usage();
        exit(0);
    }
    else if (!strncmp("vif", cmd, len))
    {
        strncpy(vif_name, val, ProtoVif::VIF_NAME_MAX);

    }
    else if (!strncmp("address", cmd, len))
    {
        int addrLen = strcspn(val,"/");    
        char addrString[32] = "\0";
        strncpy(addrString,val,addrLen);

        const char* maskLenPtr = strchr(val,'/');
        if (NULL != maskLenPtr)
        {
            maskLenPtr++;
        }
        else
        {
            PLOG(PL_ERROR, "VifExampleApp::OnCommand() Error: missing <maskLength>\n");
            return false;
        }
        if (!vif_addr.ResolveFromString(addrString))     
        {
            PLOG(PL_ERROR, "VifExampleApp::OnCommand() Error: invalid <vifAddr>\n");
            return false;   
        }

        if (1 != sscanf(maskLenPtr, "%u", &vif_mask_len))
        {
            PLOG(PL_ERROR, "VifExampleApp::OnCommand() Error: invalid <maskLength>\n");
            return false; 
        }

        if ((vif_mask_len < 1) || (vif_mask_len > 32))
        {
            PLOG(PL_ERROR, "VifExampleApp::OnCommand() Error: invalid <maskLength>\n");
            return false; 
        }
        PLOG(PL_DEBUG, "VifExampleApp::OnCommand(addr) value: %s\n",vif_addr.GetHostString());

    }
    else if (!strncmp("mac", cmd, len))
    {
        if (!vif_hwaddr.ResolveEthFromString(val))     
        {
            PLOG(PL_ERROR, "VifExampleApp::OnCommand() Error: invalid <macAddr>\n");
            return false;   
        }
        PLOG(PL_DEBUG, "VifExampleApp::OnCommand(mac) value: %s\n",vif_hwaddr.GetHostString());
    }
    else if (!strncmp("interface", cmd, len))
    {
        strncpy(if_name, val, ProtoVif::VIF_NAME_MAX);
    }
    else if (!strncmp("bridge", cmd, len))
    {
        vif_mode = BRIDGE;
    }
    else if (!strncmp("clone", cmd, len))
    {
        vif_mode = CLONE;
    }
    else if (!strncmp("replace", cmd, len))
    {
        vif_mode = REPLACE;
    }
    else if (!strncmp("txrate", cmd, len))
    {
        double rateLimit;
        if (1 != sscanf(val, "%lf", &rateLimit))
        {
            PLOG(PL_ERROR, "VifExampleApp::OnCommand(txrate) error: invalid value: %s\n", val);
            return false;
        }
        if (rateLimit < 0.0)
        {
            PLOG(PL_ERROR, "VifExampleApp::OnCommand(txrate) error: invalid value: %s\n", val);
            return false;
        }
        txrate_limit = 1000.0 * rateLimit;
    }
    else if (!strncmp("rxrate", cmd, len))
    {
        double rateLimit;
        if (1 != sscanf(val, "%lf", &rateLimit))
        {
            PLOG(PL_ERROR, "VifExampleApp::OnCommand(rxrate) error: invalid value: %s\n", val);
            return false;
        }
        if (rateLimit < 0.0)
        {
            PLOG(PL_ERROR, "VifExampleApp::OnCommand(txrate) error: invalid value: %s\n", val);
            return false;
        }
        rxrate_limit = 1000.0 * rateLimit;
        rxrate_limit_uppernominal = RXLIMIT_ALPHA_RATEFACTOR * rxrate_limit;
        rxrate_limit_lowernominal = rxrate_limit / RXLIMIT_ALPHA_RATEFACTOR;
    }
    else if (!strncmp("txloss", cmd, len))
    {
        double lossPercent;
        if (1 != sscanf(val, "%lf", &lossPercent))
        {
            PLOG(PL_ERROR, "VifExampleApp::OnCommand(txloss) error: invalid value: %s\n", val);
            return false;
        }
        tx_loss = lossPercent;
    }
    else if (!strncmp("rxloss", cmd, len))
    {
        double lossPercent;
        if (1 != sscanf(val, "%lf", &lossPercent))
        {
            PLOG(PL_ERROR, "VifExampleApp::OnCommand(rxloss) error: invalid value: %s\n", val);
            return false;
        }
        rx_loss = lossPercent;
    }
    else if (!strncmp("instance", cmd, len))
    {
        if (control_pipe.IsOpen()) control_pipe.Close();
        if (!control_pipe.Listen(val))
        {
            PLOG(PL_ERROR, "VifExampleApp::OnCommand(instance) error opening control pipe\n");
            return false;
        } 
    }   
    else if (!strncmp("debug", cmd, len))
    {
        SetDebugLevel(atoi(val));
    }
    else
    {
        PLOG(PL_ERROR, "vifExample error: invalid command\n");
        Usage();
        return false;
    }
    return true;
}  // end VifExampleApp::OnCommand()

void VifExampleApp::OnControlMsg(ProtoSocket& thePipe, ProtoSocket::Event theEvent)
{
    if (ProtoSocket::RECV == theEvent)
    {
        char buffer[8192];
        unsigned int len = 8191;
        if (thePipe.Recv(buffer, len))
        {
            buffer[len] = '\0';
            // Parse received message from controller and populate
            // our forwarding table
            if (0 != len)
                PLOG(PL_INFO,"VifExampleApp::OnControlMsg() recv'd %d byte message from controller: \"%s\"\n",
                    len, buffer);
            char* cmd = buffer;
            char* arg = NULL;
            for (unsigned int i = 0; i < len; i++)
            {
                if ('\0' == buffer[i])
                {
                    break;
                }
                else if (isspace(buffer[i]))
                {
                    buffer[i] = '\0';
                    arg = buffer+i+1;
                    break;
                }
            }
            unsigned int cmdLen = strlen(cmd);
            unsigned int argLen = len - (arg - cmd);
            // Check for a pipe only commands first
            if (!strncmp(cmd, "mneMacBlock", cmdLen) || !strncmp(cmd, "mneBlock", cmdLen))
            {
                // The "arg" points to the current set of MPR mneBlock MAC addresses
                // Overwrite our current mneBlock list
                if (argLen > (6*MNE_BLOCK_MAX))
                {
                    PLOG(PL_ERROR, "VifExampleApp::OnControlMsg(mneBlock) error: mac list too long!\n");
                    // (TBD) record this error indication permanently
                    argLen = (6*MNE_BLOCK_MAX);
                }
                memcpy(mne_block_list, arg, argLen);
                mne_block_list_len = argLen;
            }  
            else
            {
                // Maybe it's a regular command
                if (!OnCommand(cmd, arg))
                    PLOG(PL_ERROR, "VifExampleApp::OnControlMsg() invalid command: \"%s\"\n", cmd);
            }
        }
    }
}  // end VifExampleApp::OnControlMsg()

bool VifExampleApp::MneIsBlocking(const char* macAddr) const
{
    const size_t MAC_ADDR_LEN = 6;
    const char *ptr = mne_block_list;
    const char* endPtr = mne_block_list + mne_block_list_len;
    while (ptr < endPtr)
    {
        if (!memcmp(macAddr, ptr, MAC_ADDR_LEN))
            return true;   
        ptr += MAC_ADDR_LEN;
    }
    return false;
}  // end VifExampleApp::MneIsBlocking()

void VifExampleApp::PeekPkt(ProtoPktETH& ethPkt, bool inbound)
{
    switch (ethPkt.GetType())
    {
        case ProtoPktETH::IP:
        case ProtoPktETH::IPv6:
        {
            unsigned int payloadLength = ethPkt.GetPayloadLength();
            ProtoPktIP ipPkt;
            if (!ipPkt.InitFromBuffer(payloadLength, (UINT32*)ethPkt.AccessPayload(), payloadLength))
            {
                PLOG(PL_ERROR, "vifExample::PeekPkt() error: bad %sbound IP packet\n",
                        inbound ? "in" : "out");
                break;
            }
            ProtoAddress dstAddr; 
            ProtoAddress srcAddr;
            switch (ipPkt.GetVersion())
            {
                case 4:
                {
                    ProtoPktIPv4 ip4Pkt(ipPkt);
                    ip4Pkt.GetDstAddr(dstAddr);
                    ip4Pkt.GetSrcAddr(srcAddr);
                    break;
                } 
                case 6:
                {
                    ProtoPktIPv6 ip6Pkt(ipPkt);
                    ip6Pkt.GetDstAddr(dstAddr);
                    ip6Pkt.GetSrcAddr(srcAddr);
                    break;
                }
                default:
                {
                    PLOG(PL_ERROR,"VifExampleApp::PeekPkt() Error: Invalid IP pkt version.\n");
                    break;
                }
            }
            PLOG(PL_ALWAYS, "VifExampleApp::PeekPkt() %sbound  packet IP dst>%s ",
                    inbound ? "in" : "out", dstAddr.GetHostString());
            PLOG(PL_ALWAYS, "src>%s length>%d\n", srcAddr.GetHostString(), ipPkt.GetLength());
            break;
        }
        case ProtoPktETH::ARP:
        {
            ProtoPktARP arp;
            if (!arp.InitFromBuffer((UINT32*)ethPkt.AccessPayload(), ethPkt.GetPayloadLength()))
            {
                PLOG(PL_ERROR, "VifExampleApp::PeekPkt() received bad ARP packet?\n");
                break;
            } 
            PLOG(PL_ALWAYS,"VifExampleApp::PeekPkt() %sbound ARP ", 
                  inbound ? "in" : "out");
            switch(arp.GetOpcode())
            {
                case ProtoPktARP::ARP_REQ:
                    PLOG(PL_ALWAYS, "request ");
                    break;
                case ProtoPktARP::ARP_REP:
                    PLOG(PL_ALWAYS, "reply ");
                    break;
                default:
                    PLOG(PL_ALWAYS, "??? ");
                    break;
            }
            ProtoAddress addr;
            arp.GetSenderHardwareAddress(addr);
            PLOG(PL_ALWAYS, "from eth:%s ", addr.GetHostString());
            arp.GetSenderProtocolAddress(addr);
            PLOG(PL_ALWAYS, "ip:%s ", addr.GetHostString());
            arp.GetTargetProtocolAddress(addr);
            PLOG(PL_ALWAYS, "for ip:%s ", addr.GetHostString());
            if (ProtoPktARP::ARP_REP == arp.GetOpcode())
            {

                arp.GetTargetHardwareAddress(addr);
                PLOG(PL_ALWAYS, "eth:%s ", addr.GetHostString()); 
            }
            PLOG(PL_ALWAYS, "\n");
            break;
        }
        default:
            PLOG(PL_ERROR, "VifExampleApp::PeekPkt() unknown %s packet type\n", inbound ? "inbound" : "outbound");
            break;
    }
}  // end VifExampleApp::PeekPkt()

/**
 * @note We offset the buffer here by 2 bytes since 
 * Ethernet header is 14 bytes
 * (i.e. not a multiple of 4 (sizeof(UINT32))
 * This gives us a properly aligned buffer for 
 * 32-bit aligned IP packets
 */
void VifExampleApp::OnOutboundPkt(ProtoChannel&              theChannel,
                                  ProtoChannel::Notification notifyType)
{
    const int BUFFER_MAX = 4096;
    UINT32 alignedBuffer[BUFFER_MAX/sizeof(UINT32)];
    // offset by 2-bytes so IP content is 32-bit aligned
    UINT16* ethBuffer = ((UINT16*)alignedBuffer) + 1; 
    unsigned int numBytes = BUFFER_MAX - 2;
        
    while (vif->Read((char*)ethBuffer, numBytes))
    {
        if (numBytes == 0) break;  // no more packets to receive
        
        TRACE("read %u bytes from vif ...\n", numBytes);
        
        ProtoPktETH ethPkt((UINT32*)ethBuffer, BUFFER_MAX - 2);
        if (!ethPkt.InitFromBuffer(numBytes))
        {
            PLOG(PL_ERROR, "VifExampleApp::OnOutboundPkt() error: bad Ether frame\n");
            continue;
        }
        
        // Implement "tx_loss" random packet dropping, if applicable
        // (Note we still enforce the "rate_limit" as if packet was sent
        if (!((tx_loss > 0.0) && (UniformRand(100.0) < tx_loss)))
        {
            if (CLONE == vif_mode)
                cap->Forward((char*)ethBuffer, numBytes);   // send using interface MAC addr
            else
                cap->Send((char*)ethBuffer, numBytes);      // send using vif MAC addr (same as iface if REPLACE mode)
            if (GetDebugLevel() >= 2) PeekPkt(ethPkt, false);
        }
        else
        {
            PLOG(PL_DETAIL, "VifExampleApp::OnOutboundPkt() dropped outbound packet\n");
        }
        
        // If applicable, enforce rate limiting
        if (txrate_limit >= 0.0)
        {
            vif->StopInputNotification();
            double pktTime = 8*numBytes / txrate_limit;
            txrate_timer.SetInterval(pktTime);
            txrate_timer.SetRepeat(0);  
            ActivateTimer(txrate_timer);
            break; 
        }
        numBytes = BUFFER_MAX;
    }
} // end VifExampleApp::OnOutboundPkt

/**
 * (TBD) perhaps we could  "test" the vif to see if
 * it has a packet already and just go ahead and read
 * it and immediately reshedule our timer, thus keeping
 * things "a little more real" so to speak
 * One way to do this would be to use "vif->SetBlocking(false)"
 * _after_ "vif->StopInputNotification()" above and then we
 * can use "vif->Read()" to look for a packet ready to go
 * upon our rate timeout here ...
 */  
bool VifExampleApp::OnTxRateTimeout(ProtoTimer& /*theTimer*/)
{
    vif->StartInputNotification();
    return true;   
}  // end VifExampleApp::OnTxRateTimeout()

/**
 * @note We offset the buffer here by 2 bytes since 
 * Ethernet header is 14 bytes
 * (i.e. not a multiple of 4 (sizeof(UINT32))
 * This gives us a properly aligned buffer for 
 * 32-bit aligned IP packets
 */
void VifExampleApp::OnInboundPkt(ProtoChannel&              theChannel,
                                 ProtoChannel::Notification notifyType)
{
    ProtoTime currentTime;
    if (ProtoChannel::NOTIFY_INPUT != notifyType) return;
    while(1) 
    {
        ProtoCap::Direction direction;

        const int BUFFER_MAX = 4096;
        UINT32 alignedBuffer[BUFFER_MAX/sizeof(UINT32)];
        // offset by 2-bytes so IP content is 32-bit aligned
        UINT16* ethBuffer = ((UINT16*)alignedBuffer) + 1; 
        unsigned int numBytes = BUFFER_MAX - 2;
            
        if (!cap->Recv((char*)ethBuffer, numBytes, &direction))
        {
            PLOG(PL_ERROR, "VifExampleApp::OnInboundPkt() ProtoCap::Recv() error\n");
            break;
        }

        if (numBytes == 0) break;  // no more packets to receive

        if ((ProtoCap::OUTBOUND != direction)) 
        {
            // Map ProtoPktETH instance into buffer and init for processing
            ProtoPktETH ethPkt((UINT32*)ethBuffer, BUFFER_MAX - 2);
            if (!ethPkt.InitFromBuffer(numBytes))
            {
                PLOG(PL_ERROR, "VifExampleApp::OnInboundPkt() error: bad Ether frame\n");
                continue;
            }
            
            // In "MNE" environment, ignore packets from blocked MAC sources
            ProtoAddress srcMacAddr;
            ethPkt.GetSrcAddr(srcMacAddr);
            if ((0 != mne_block_list_len) &&
                (MneIsBlocking(srcMacAddr.GetRawHostAddress())))
                continue;  // ignore packets blocked by MNE
            
            // Implement inbound rate limit
            if (rxrate_limit == 0.0)
                continue;
            else if (rxrate_limit > 0.0)
            {
                rxlimit_rxbytes += numBytes;
                if (0 == rxlimit_avg_size)
                    rxlimit_avg_size = numBytes;
                else // compute exponentially weighted moving average packet size
                    rxlimit_avg_size = RXLIMIT_SIZE_ALPHA*numBytes + (1.0 - RXLIMIT_SIZE_ALPHA)*rxlimit_avg_size;
                if (rxlimit_drop_prob > 0.0)
                {
                    // weight drop probability based on packet size vs avg.
                    double p = rxlimit_drop_prob * (double)numBytes/rxlimit_avg_size;
                    if (UniformRand(1.0) < p)
                    {
                        PLOG(PL_DETAIL, "VifExampleApp::OnInboundPkt() dropped inbound packet due to rate limit\n");
                        continue;
                    }
                }
            }
            
            // Implement "rx_loss" random packet dropping, if applicable
            if ((rx_loss > 0.0) && (UniformRand(100.0) < rx_loss))
            {
                PLOG(PL_DETAIL, "VifExampleApp::OnInboundPkt() dropped inbound packet due to txloss\n");
                continue;
            }
            
            // Only process IP & ARP packets (skip others)
            switch (ethPkt.GetType())
            {
                case ProtoPktETH::IP:   
                case ProtoPktETH::IPv6:
                case ProtoPktETH::ARP:  
                    if (GetDebugLevel() >= 2) PeekPkt(ethPkt, true);
                    vif->Write((char*)ethPkt.GetBuffer(), ethPkt.GetLength());
                    break;
                default:
                    break;
            }
        }
    }  // end while(1)
    if (rxrate_limit > 0.0)
    {
        currentTime.GetCurrentTime();
        if (rxlimit_prev_pkt_time_initialized == false)  // first time through
        {
            rxlimit_prev_pkt_time = currentTime;
            rxlimit_prev_pkt_time_initialized = true;
        }
        else
        {
            double deltaTime = ProtoTime::Delta(currentTime, rxlimit_prev_pkt_time);
            if (deltaTime >= RXLIMIT_MIN_UPDATE_TIME)
            {
                double currentRateSample = 8.0*rxlimit_rxbytes / deltaTime;
                if (0.0 == rxlimit_avg_rate)
                    rxlimit_avg_rate = currentRateSample;
                else
                {
                    double alpha = RXLIMIT_RATE_ALPHA;
                    // outside of nominal rates, increase alpha for faster response
                    if (currentRateSample > rxrate_limit_uppernominal)
                        alpha *= currentRateSample/rxrate_limit;
                    else if (currentRateSample < rxrate_limit_lowernominal)
                        alpha *= rxrate_limit/currentRateSample;
                    if (alpha > 1.0) alpha = 1.0;
                    rxlimit_avg_rate = alpha*currentRateSample + (1.0 - alpha)*rxlimit_avg_rate;
                }

                if (rxlimit_avg_rate <= rxrate_limit)
                    rxlimit_drop_prob = 0.0;
                else 
                    rxlimit_drop_prob = 1.0 - (rxrate_limit / rxlimit_avg_rate);

                rxlimit_prev_pkt_time = currentTime;
                rxlimit_rxbytes = 0;
            }
        }
    }
} // end VifExampleApp::OnInboundPkt

