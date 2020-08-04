/******************************************************************************
*
* Copyright (C) 2016 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/


/******************************************************************************
* Description
* ===========
* An example Microblaze application that fetches partial bitstreams from a 
* TFTP (Trivial File Transfer Protocol) server, stores them in memory, and then 
* programs the PRC to manage those partial bitstreams.
*
* RPs and RMs
* ===========
* The design is based on the standard PR tutorial design consisting of SHIFT 
* and COUNT RPs.  It is implemented on a KC705 board
*  
* The software application will change the loaded RMs every 5 seconds
*
* TFTP server
* ===========
* The application requires a TFTP server to be running on the computer at 
* specified by the "ServerIpAddr" variable.    
* 
* In this example application the software can predict the partial bitstream 
* names and knows how to process each RM, so no additional information needs to 
* be fetched from the TFTP server.  Later examples show how an application can 
* fetch this information as well.
*
*                     !!!! Important !!!! 
*
* Your firewall may block TFTP transactions so you may need to configure it to 
* allow them, or temporarily disable it.
*
*                     !!!! Important !!!! 
* Required Changes
* ================
* Set the following variables to match your environment:
*  MacEthernetAddress  : The board's MAC address
*  BoardIpAddr         : The board's IP address
*  Netmask             : The Netmask
*  GatewayIpAddr       : The Gateway IP address
*  ServerIpAddr        : The TFTP server's IP address
*
* This code calls XPrc_PrintVsmStatus() to read and display the status of
* the PRC's VSMs.  This requires a compiler flag in the BSP:
*  BSP Settings -> cpu -> extra_compiler_flags -> add "-DXPRC_DEBUG_CORE"
******************************************************************************/


#include <stdio.h>
#include "xparameters.h"
#include "pr_tftp.h"           // Include the PR TFTP library code
#include "xprc.h"              // Include the PRC driver code
#include "netif/xadapter.h"
#include "platform.h"
#include "platform_config.h"

// Missing declaration in lwIP
void lwip_init();

// =====================================================================
// Timeout handling
// =====================================================================
//
// This flag is incremented in platform.c::timer_callback.  When it
// reaches a certain value, a TFTP timeout is declared.
//
extern volatile int unsigned PR_TFTP_TimerCount;

// The timer callback is called by timer interrupt that's set up to
// trigger every 1 second.
//
#define PR_TFTP_TIMEOUT_THRESHOLD 50
#define PR_TFTP_TIMEOUT_RETRY      3

static struct netif LocalNetif;

void print_app_header(struct ip_addr *TftpServerIpAddr){
    xil_printf("======================================================================================\n\r");
    xil_printf("An example Microblaze application that fetches partial bitstreams from a TFTP server,\n\r");
    xil_printf("stores them in memory, and then programs the PRC to manage those partial bitstreams.\n\r");
    xil_printf("======================================================================================\n\r");
    xil_printf("  RPs and RMs\n\r");
    xil_printf("  -----------\n\r");
    xil_printf("  The design is based on the standard PR tutorial design consisting of SHIFT and COUNT\n\r");
    xil_printf("  RPs.  It is implemented on a KC705 board\n\r");
    xil_printf("   \n\r");
    xil_printf("  TFTP server\n\r");
    xil_printf("  -----------\n\r");
    xil_printf("  The application requires a TFTP server to be running on the computer at \n\r");
    xil_printf("  %d.%d.%d.%d.\n\r", ip4_addr1(TftpServerIpAddr), ip4_addr2(TftpServerIpAddr),
                                     ip4_addr3(TftpServerIpAddr), ip4_addr4(TftpServerIpAddr));
    xil_printf(" \n\r");
    xil_printf(" !!!! Important !!!! Your firewall may block TFTP transactions so you may need\n\r");
    xil_printf(" !!!! Important !!!! to configure it to allow them, or temporarily disable it.\n\r");
    xil_printf("======================================================================================\n\r");
    xil_printf("\n\r");
}

int main(){
  
  // --------------------------------------------------------------------
  // Variable Declarations
  // --------------------------------------------------------------------
  
  int Status;

  // A buffer to store each received partial bitstream.  In this example the
  // buffer is not freed as we want to keep each partial bitstream after we
  // have downloaded it.  Instead, it is just allocated for the next partial
  //
  char * pBuffer = NULL;
  
  // A struct holding some options to control the TFTP file download.
  // These are not essential to the transfer and can be ignored.
  //
  pr_tftp_options_s TransferOptions;
  
  // Error code from PR TFTP functions
  //
  pr_tftp_err_t Err;
  
  // A pointer to the PRC's configuration information.
  XPrc_Config *pPrcConfig;
  XPrc Prc;  // PRC Driver Instance Data


  // Various IP addresses
  //
  struct ip_addr BoardIpAddr, Netmask, GatewayIpAddr, ServerIpAddr;
  
  // The mac address of the board. this should be unique per board
  //
  unsigned char MacEthernetAddress[] = { 0x00, 0x0a, 0x35, 0x02, 0xAE, 0xE3 };
  
  // --------------------------------------------------------------------
  // Set up the application
  // --------------------------------------------------------------------
  
  // Enable the Caches and setup the interrupts
  //
  init_platform();
  
  // Setup the IP addresses to be used by the application
  //
  IP4_ADDR(&BoardIpAddr   , 149, 199, 131,  41);  // The board's IP address
  IP4_ADDR(&Netmask       , 255, 255, 255,   0);  // The Netmask
  IP4_ADDR(&GatewayIpAddr , 149, 199, 131, 254);  // The Gateway IP address
  IP4_ADDR(&ServerIpAddr  , 149, 199, 131, 172);  // The TFTP server's IP address
  
  // Initialize the IP stack
  //
  lwip_init();
  
  // Add network interface to the netif_list, and set it as default
  //
  if (!xemac_add(&LocalNetif, &BoardIpAddr, &Netmask, &GatewayIpAddr,
                 MacEthernetAddress, PLATFORM_EMAC_BASEADDR)) {
    xil_printf("Error adding N/W interface\n\r");
    return XST_FAILURE;
  }
  
  // Initialise the PRC Instance Driver.
  //
  pPrcConfig = XPrc_LookupConfig((u16)XPAR_PRC_0_DEVICE_ID);
  if (NULL == pPrcConfig) {
    return XST_FAILURE;
  }
  
  Status = XPrc_CfgInitialize(&Prc, pPrcConfig, pPrcConfig->BaseAddress);
  if (Status != XST_SUCCESS) {
    return XST_FAILURE;
  }

  // Enable the interrupts
  //
  platform_enable_interrupts();
  
  // Bring up the network interface
  //
  netif_set_default(&LocalNetif);
  netif_set_up(&LocalNetif);
  
  print_app_header(&ServerIpAddr);
  
  // --------------------------------------------------------------------
  // Print some PRC status
  // --------------------------------------------------------------------
  // We get most information in the active state, so make sure the VSMs
  // are active
  //
  
  xil_printf("======================================================================================\n\r");
  xil_printf("PRC Status on startup   \n\r");
  xil_printf("======================================================================================\n\r");
  xil_printf("  The current status of the PRC is:   \n\r");
  
  for (u16 VsmId = 0; VsmId < XPrc_GetNumberOfVsms(&Prc); VsmId++){
    
    // Don't bother checking the status because this command is
    // harmless if the VSM is already in the active state
    //
    XPrc_SendRestartWithNoStatusCommand(&Prc, VsmId);
    
    // Wait until it has left the shutdown state
    //
    while(XPrc_IsVsmInShutdown(&Prc, VsmId) == 1);
    
    xil_printf("  VSM %x status:\n\r", VsmId);
    XPrc_PrintVsmStatus(&Prc, VsmId, "    ");

    // Shut it down again and wait until that takes effect
    //
    XPrc_SendShutdownCommand(&Prc, VsmId);
    while(XPrc_IsVsmInShutdown(&Prc, VsmId) == 0);
  }
  xil_printf("======================================================================================\n\r\n\r");
  
  // --------------------------------------------------------------------
  // Fetch all of the partial bitstreams from a TFTP server and program
  // the PRC to handle them
  // --------------------------------------------------------------------

  for (u16 VsmId = 0; VsmId < XPrc_GetNumberOfVsms(&Prc); VsmId++){
    if(XPrc_IsVsmInShutdown(&Prc, VsmId) == 0) {
      xil_printf("ERROR: Programming VSM %0d but it is active \n\r", VsmId );
      return XST_FAILURE;
    }
    
    for (u16 RmId = 0; RmId < XPrc_GetNumRms(&Prc, VsmId); RmId++){
      u32 DataBufferSize = 0;
      u32 BsSize         = 0;
      char filename[256];
      
      // ------------------
      // Fetch the partial
      // ------------------
      //
      sprintf((char *)&filename, "example_1/rp%0d_rm%0d.bin", VsmId, RmId);
      xil_printf("Fetching the partial for VSM %0d RM %0d: %s\n\r", VsmId, RmId, filename);
      
      TransferOptions.ReallocateMemoryIfRequired = 1; 
      TransferOptions.IncrementAmount            = 1024*10;   // Allocate in 10K chunks for speed
      TransferOptions.DebugMemoryAllocation      = 0;
      TransferOptions.DebugTftp                  = 0;
      
      BsSize = 0;
      Err = PR_TFTP_FetchPartialToMem(&LocalNetif              , // The network interface to use
                                      ServerIpAddr             , // The TFTP server IP address
                                      (char *)&filename        , // The path & name of the file to fetch
                                      &pBuffer                 , // A pointer to a memory buffer to store the file
                                      &DataBufferSize          , // A pointer to a variable holding the size of the buffer.
                                      &BsSize                  , // A pointer to a variable to return the number of bytes stored
                                      &PR_TFTP_TimerCount      , // A pointer to a counter incremented in a timer callback somewhere
                                      PR_TFTP_TIMEOUT_THRESHOLD, // How long to wait on a TFTP packet before timing out
                                      PR_TFTP_TIMEOUT_RETRY    , // How many times to retry a failed packet
                                      &TransferOptions           //
                                      );
      if (Err != PR_TFTP_ERR_OK) {
        xil_printf("Failed to fetch Partial Bitstream %s. Aborting program. \n\r", filename);
        return XST_FAILURE;
      }
      
      // ----------------------------
      // Program the PRC for this RM
      // ----------------------------
      //
      XPrc_SetBsSize   (&Prc, VsmId, RmId, BsSize);
      XPrc_SetBsAddress(&Prc, VsmId, RmId, (u32)pBuffer);    
    }
  }
  
  // --------------------------------------------------------------------
  // Restart the VSMs
  // --------------------------------------------------------------------
  //
  xil_printf("======================================================================================\n\r");
  xil_printf("Restarting PRC's VSMs\n\r");
  xil_printf("======================================================================================\n\r");
  for (u16 VsmId = 0; VsmId < XPrc_GetNumberOfVsms(&Prc); VsmId++){
    XPrc_SendRestartWithNoStatusCommand(&Prc, VsmId);
    
    // Wait until it has left the shutdown state
    //
    while(XPrc_IsVsmInShutdown(&Prc, VsmId) == 1);
    
    xil_printf("  VSM %x status:\n\r", VsmId);
    XPrc_PrintVsmStatus(&Prc, VsmId, "    ");
  }
  xil_printf("======================================================================================\n\r\n\r");
  
  
  // --------------------------------------------------------------------
  // Send Software Triggers
  // --------------------------------------------------------------------
  //
  xil_printf("======================================================================================\n\r");
  xil_printf("Trigger the PRC every 5 seconds\n\r");
  xil_printf("======================================================================================\n\r");
  {
    
    u16 ActiveRms[XPrc_GetNumberOfVsms(&Prc)];
    PR_TFTP_TimerCount = 0;  // We aren't doing any more TFTP transfers so reuse this timer variable
    
    for (int VsmId = 0; VsmId < XPrc_GetNumberOfVsms(&Prc); VsmId++){
      // Set the default active RM for each VS
      //
      if (XPrc_GetHasPorRm (&Prc, VsmId)) {
        ActiveRms[VsmId] = XPrc_GetPorRm(&Prc, VsmId);
      } else {
        ActiveRms[VsmId] = 0;
      }
    }
    
    while(1){
      // Change the RMs every 5 seconds (approx)
      if(PR_TFTP_TimerCount > 5){
        PR_TFTP_TimerCount = 0;
        for (u16 VsmId = 0; VsmId < XPrc_GetNumberOfVsms(&Prc); VsmId++){
          ActiveRms[VsmId] = (ActiveRms[VsmId]+1) % XPrc_GetNumRms(&Prc, VsmId);
          xil_printf("------------------------------\n\r");
          xil_printf("Sending trigger %0d to VSM %0d\n\r", ActiveRms[VsmId], VsmId);
          xil_printf("------------------------------\n\r");
          XPrc_SendSwTrigger (&Prc, VsmId, ActiveRms[VsmId]);

          xil_printf("  VSM %x status:\n\r", VsmId);
          XPrc_PrintVsmStatus(&Prc, VsmId, "    ");
          xil_printf("  \n\r");

        }
      }
    }
  }
  
  cleanup_platform();
  
  // We stored each bitstream locally but didn't store the pointers to them, except 
  // in the PRC.  To deallocate that memory, we would have to read the bitstream addresses
  // from the PRC and free them.  
  
  return 0;
}
