#include "userdevice.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "FPGAModule.hh"
#include "RegisterMap.hh"
#include "RegisterMapCommon.hh"
#include "UDPRBCP.hh"
#include "errno.h"
#include "network.hh"
#include "rbcp.hh"
#include "MhTdcFuncs.hh"
#include "AdcFuncs.hh"
#include "DaqFuncs.hh"

bool    stand_alone = false;
DaqMode g_daq_mode  = DM_NORMAL;
std::string nick_name;
#define DEBUG 0

namespace
{
  using namespace LBUS;
  static const int n_header      = 3;
  static const int max_n_word    = n_header + 2 + 16*64 + 16*64;
  static const int max_data_size = sizeof(unsigned int)*max_n_word;

  char ip[100];
  unsigned int min_time_window;
  unsigned int max_time_window;

  int  sock=0;

  namespace TRG{
    static const int32_t kNumHead      {3};
    static const int32_t kNumBodyMax   {8192+65536}; // to read ADC data (32 ch * 2048 max windowsize)
    static const int32_t kNumData      {kNumHead + kNumBodyMax};
    static const int32_t kNumByte      {4};
  };

}



//______________________________________________________________________________
int
get_maxdatasize( void )
{
  return max_data_size;
}

//______________________________________________________________________________
void
open_device( NodeProp& nodeprop )
{
  nick_name = nodeprop.getNickName();
  static const std::string& func_name(nick_name+" [::"+__func__+"()]");

  // update DAQ mode
  g_daq_mode = nodeprop.getDaqMode();

  // load parameters
  const int argc = nodeprop.getArgc();
  for( int i=0; i<argc; ++i ){
    std::string arg = nodeprop.getArgv(i);
    std::istringstream iss;
    if( arg.substr(0,11) == "--sitcp-ip=" ){
      iss.str( arg.substr(11) );
      iss >> ip;
    }

    if( arg.substr(0,11) == "--min-twin=" ){
      iss.str( arg.substr(11) );
      iss >> min_time_window;
    }

    if( arg.substr(0,11) == "--max-twin=" ){
      iss.str( arg.substr(11) );
      iss >> max_time_window;
    }

  }

  //Connection check -----------------------------------------------
  while(0 > (sock = HUL::DAQ::ConnectSocket(ip) )){
    std::ostringstream oss;
    oss << func_name << " Connection fail : " << ip;
    send_error_message( oss.str() );
    std::cerr << oss.str() << std::endl;
  }

  close(sock);

  RBCP::UDPRBCP udp_rbcp(ip, RBCP::gUdpPort, RBCP::DebugMode::kNoDisp); //kInteractive->kNoDisp FOura
  HUL:: FPGAModule fpga_module(udp_rbcp);

  // Release AdcRo reset
  if(1 == fpga_module.ReadModule(ADC::kAddrAdcRoReset)){
    fpga_module.WriteModule(ADC::kAddrAdcRoReset, 0);
  }
  std::cout << "AdcRo rest = " <<     fpga_module.ReadModule(ADC::kAddrAdcRoReset) << std::endl;


  // Set trigger path //
  uint32_t reg_trg = TRM::kRegL1Ext;
  fpga_module.WriteModule(TRM::kAddrSelectTrigger, reg_trg);
  std::cout << std::hex << "SelectTrigger = " <<   fpga_module.ReadModule(TRM::kAddrSelectTrigger) << std::endl;

  // Set NIM-IN //
  fpga_module.WriteModule(IOM::kAddrExtL1, IOM::kReg_i_Nimin1);
  std::cout << "kAddrExtL1 = " <<   fpga_module.ReadModule(IOM::kAddrExtL1) << std::endl;
  
  // Set TDC window //
  HUL::DAQ::SetTdcWindow(max_time_window, min_time_window, fpga_module);
  std::cout << "kAddrPtrOfs = " << TDC::kAddrPtrOfs << std::endl;
  std::cout << "kAddrWindowMax = " << TDC::kAddrWindowMax << std::endl;
  std::cout << "kAddrWindowMin = " << TDC::kAddrWindowMin << std::endl;
  
  // Enable TDC block //
  uint32_t en_block = TDC::kEnLeading | TDC::kEnTrailing;
  fpga_module.WriteModule(TDC::kAddrEnableBlock, en_block);
  std::cout << "kAddrEnableBlock = " <<  fpga_module.ReadModule(TDC::kAddrEnableBlock) << std::endl;
  
  // Set ADC window (same as TDC) //
  HUL::DAQ::SetAdcWindow(max_time_window, min_time_window, fpga_module);
  std::cout << "kAddrPtrOfs = " << TDC::kAddrPtrOfs << std::endl;
  std::cout << "kAddrWindowMax = " << TDC::kAddrWindowMax << std::endl;
  std::cout << "kAddrWindowMin = " << TDC::kAddrWindowMin << std::endl;
  
  // Resest event counter //
  fpga_module.WriteModule(DCT::kAddrResetEvb, 0);
  std::cout << "kAddrResetEvb = " <<  fpga_module.ReadModule(DCT::kAddrResetEvb) << std::endl;

  ::sleep(1);

  {
    std::cout << func_name << " hoge1 : Last line of  open_device " << std::endl;
  }

  //  fpga_module.WriteModule(DCT::kAddrDaqGate, 1);  
  return;
}

//______________________________________________________________________________
void
init_device( NodeProp& nodeprop )
{
  static const std::string& func_name(nick_name+" [::"+__func__+"()]");

  // for operation check
  {
    std::cout << func_name << " hoge2 : First line of init_device " << ip << std::endl;
  }


  // update DAQ mode
  g_daq_mode = nodeprop.getDaqMode();
  std::cout << func_name << " hoge3 : hoge line of init_device " << ip << std::endl;
  //  event_num = 0;

  switch(g_daq_mode){
  case DM_NORMAL:
    {
      while(0 > (sock = HUL::DAQ::ConnectSocket(ip) )){
	std::ostringstream oss;
	oss << func_name << " Connection fail : " << ip;
	send_error_message( oss.str() );
	std::cerr << oss.str() << std::endl;
      }

      {
	std::ostringstream oss;
	oss << func_name << " Connection done : " << ip;
	send_normal_message( oss.str() );
      }

      // // Start DAQ
      RBCP::UDPRBCP udp_rbcp(ip, RBCP::gUdpPort, RBCP::DebugMode::kNoDisp);
      HUL::FPGAModule fpga_module(udp_rbcp);

      fpga_module.WriteModule(DCT::kAddrDaqGate, 1);
         
      std::cout << "kAddrDaqGate = " <<    fpga_module.ReadModule(DCT::kAddrDaqGate) << std::endl;
      std::cout << "DM_NORMAL, DAQ Gate ON in init_device" << std::endl;
      return;
    }
  case DM_DUMMY:
    {
      return;
    }
  default:
    return;
  }
}

//______________________________________________________________________________
void
finalize_device( NodeProp& nodeprop )
{

  RBCP::UDPRBCP udp_rbcp(ip, RBCP::gUdpPort, RBCP::DebugMode::kNoDisp);
  HUL::FPGAModule fModule(udp_rbcp);
  fModule.WriteModule(DCT::kAddrDaqGate, 0);

  ::sleep(1);
  unsigned int data[max_n_word];
  while(-1 != HUL::DAQ::DoEventCycle(sock, data));

  shutdown(sock, SHUT_RDWR);
  close(sock);

  return;
}

//______________________________________________________________________________
void
close_device( NodeProp& nodeprop )
{
  // const std::string& nick_name(nodeprop.getNickName());
  // const std::string& func_name(nick_name+" [::"+__func__+"()]");
  return;
}

//______________________________________________________________________________
int
wait_device( NodeProp& nodeprop )
/*
  return -1: TIMEOUT or FAST CLEAR -> continue
  return  0: TRIGGED -> go read_device
*/
{
  std::cout << "userdevice.cc : wait device start" << std::endl;

  // const std::string& nick_name(nodeprop.getNickName());
  // const std::string& func_name(nick_name+" [::"+__func__+"()]");
  switch(g_daq_mode){
  case DM_NORMAL:
    {
      return 0;
    }
  case DM_DUMMY:
    {
      usleep(200000);
      return 0;
    }
  default:
    return 0;
  }

}

//______________________________________________________________________________
int
read_device( NodeProp& nodeprop, unsigned int* data, int& len )
/*
  return -1: Do Not Send data to EV
  return  0: Send data to EV
*/
{
  // const std::string& nick_name(nodeprop.getNickName());
  // const std::string& func_name(nick_name+" [::"+__func__+"()]");

  std::cout << "userdevice.cc read_device start" << std::endl;

  switch(g_daq_mode){
  case DM_NORMAL:
    {
      std::cout << "userdevice.cc First line for read_device, DM_NORMAL" <<std::endl;
      int ret_event_cycle = HUL::DAQ::DoEventCycle(sock, data);
      len = ret_event_cycle == -1 ? -1 : ret_event_cycle/sizeof(unsigned int);
      // if( len > 0 ){
      // 	for(int i = 0; i<n_word; ++i){
      // 	  printf("%x ", data[i]);
      // 	  if(i%8==0) printf("\n");
      // 	}
      // }
      return len;
    }
  case DM_DUMMY:
    {
      len = 0;
      return 0;
    }
  default:
    len = 0;
    return 0;
  }

}
