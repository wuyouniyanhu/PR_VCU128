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
* TFTP server
* ===========
* The application requires a TFTP server to be running on the computer at 
* specified by the "ServerIpAddr" variable.    
* This needs to provide a file called "example_2/rm_info.csv" which 
* provides the following information for each partial bitstream: 
*          VSM ID
*          RM ID
*          Reset Duration
*          Reset Required
*          Startup Required
*          Shutdown Required
*          BS Size
*          File Name
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
#include "string.h"
#include "platform.h"
#include "platform_config.h"
#include "xil_printf.h"
#include "lwip/udp.h"
#include "xil_cache.h"

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
    xil_printf("  This needs to provide a file called \"example_2/rm_info.csv\" which provides the following  \n\r");
    xil_printf("  information for each partial bitstream: \n\r");
    xil_printf("         VSM ID\n\r");
    xil_printf("         RM ID\n\r");
    xil_printf("         Reset Duration\n\r");
    xil_printf("         Reset Required\n\r");
    xil_printf("         Startup Required\n\r");
    xil_printf("         Shutdown Required\n\r");
    xil_printf("         BS Index\n\r");
    xil_printf("         BS Size\n\r");
    xil_printf("         File Name\n\r");
    xil_printf("   \n\r");
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
  // have downloaded it.  Instead, the pointer is just reused for the next 
  // partial
  //
  char * pBuffer = NULL;
  
  // A buffer to store the RM Information file in before we parse it.
  // This is freed after it is used.
  //
  char * pRmInfoFile = NULL;
  
  // An array of RP information structs
  //
  pr_tftp_rp_s *pRPInfoArray;
  
  // Useful pointers to RP and RM information structs.
  pr_tftp_rp_s *pRPInfo;
  pr_tftp_rm_s *pRMInfo;
  
  // The PR TFTP fetch functions will set this to say how big the buffer holding the file is.
  // The buffer may be larger than the file contents.
  //
  u32 RmInfoFileBufferSize;
  
  // The PR TFTP fetch functions will set this to say how big the downloaded file was
  //
  u32 RmInfoFileBufferUsed;
  
  // A struct holding some options to control the TFTP file download.
  // These are not essential to the transfer and can be ignored.
  //
  pr_tftp_options_s TransferOptions;
  
  // Error code from PR TFTP functions
  //
  pr_tftp_err_t Err;
  

  XPrc Prc;  // PRC Driver Instance Data

  // A pointer to the PRC's configuration information.
  XPrc_Config *pPrcConfig;
  
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
  //
  xil_printf("======================================================================================\n\r");
  xil_printf("Fetching RM Information from TFTP Server    \n\r");
  xil_printf("======================================================================================\n\r");
  xil_printf("Requesting file from a TFTP server running on %d.%d.%d.%d.  If this times out then check that your firewall is disabled, or allows TFTP traffic.\n\r",
             ip4_addr1(&ServerIpAddr), ip4_addr2(&ServerIpAddr), ip4_addr3(&ServerIpAddr), ip4_addr4(&ServerIpAddr));
  
  
  // Setup some transfer options
  //
  TransferOptions.ReallocateMemoryIfRequired = 1   ; // Let it sort out how much memory to use.
  TransferOptions.IncrementAmount            = 1024; // If memory is allocated, increase the buffer in steps of 1K to
                                                     // cut down on the number of reallocations required
  TransferOptions.DebugMemoryAllocation      = 0;
  TransferOptions.DebugTftp                  = 0;
  RmInfoFileBufferSize                       = 0;
  
  Err = PR_TFTP_FetchRmInfoToMem(&LocalNetif               , // The network interface to use.
                                 ServerIpAddr              , // The TFTP server IP address.
                                 "example_2/rm_info.csv"   , // The path & name of the file to fetch.
                                 &pRmInfoFile              , // A pointer to a memory buffer to store the file.
                                                             // This will be updated if more memory is allocated.
                                 &RmInfoFileBufferSize     , // A pointer to the size of the buffer.  This will be updated
                                                             // if more memory is allocated.
                                 &RmInfoFileBufferUsed     , // A pointer to a variable to return the number of bytes stored.
                                 &PR_TFTP_TimerCount       , // A pointer to a counter incremented in a timer callback somewhere.
                                 PR_TFTP_TIMEOUT_THRESHOLD , // How long to wait on a TFTP packet before timing out.
                                 PR_TFTP_TIMEOUT_RETRY     , // How many times to retry a failed packet.
                                 &TransferOptions            // Non-essential options.  Pass NULL for defaults
                                 );
  if (Err != PR_TFTP_ERR_OK) {
    xil_printf("Failed to fetch RM Info file. Aborting program. \n\r");
    return XST_FAILURE;
  }
  
  xil_printf("RM Info file has been received.  Size = %d bytes\n\r", RmInfoFileBufferUsed);
  xil_printf("%s\n\r", pRmInfoFile);
  xil_printf("======================================================================================\n\r\n\r");
  
  
  // --------------------------------------------------------------------
  // Process the CSV file containing the RM information
  // --------------------------------------------------------------------
  //
  xil_printf("======================================================================================\n\r");
  xil_printf("Processing the RM Information Received from the TFTP Server    \n\r");
  xil_printf("======================================================================================\n\r");
  
  if (PR_TFTP_InitialiseDataStructureFromRmInfoFile(pRmInfoFile, 
                                                    RmInfoFileBufferUsed, 
                                                    XPrc_GetNumberOfVsms(&Prc), 
                                                    &pRPInfoArray) != PR_TFTP_ERR_OK) {
    xil_printf("Failed to parse RM Info file. Aborting program. \n\r");
    return XST_FAILURE;
  }
  
  // Free the buffer that holds the RM Information CSV file
  free(pRmInfoFile);
  
  
  // -----------------------------------------
  // Some housekeeping on the data structure
  // -----------------------------------------
  //
  for (int VsmId = 0; VsmId < XPrc_GetNumberOfVsms(&Prc); VsmId++){
    
    // Set the default active RM for each VS
    //
    if (XPrc_GetHasPorRm (&Prc, VsmId)) {
      pRPInfoArray[VsmId].ActiveRM = XPrc_GetPorRm(&Prc, VsmId);
    } 
  }
  
  // -----------------------------------------
  // Some debug printing
  // -----------------------------------------
  //
  PR_TFTP_PrintDataStructure(XPrc_GetNumberOfVsms(&Prc), pRPInfoArray);

 
  // -----------------------------------------
  // Program the PRC 
  // -----------------------------------------
  for (u16 VsmId = 0; VsmId < XPrc_GetNumberOfVsms(&Prc); VsmId++){
    pRPInfo = PR_TFTP_GetRPInfoByIndex(pRPInfoArray, XPrc_GetNumberOfVsms(&Prc), VsmId);
    
    if (pRPInfo == NULL) {
      xil_printf("ERROR Cannot retrieve information for VSM %0d\n\r", VsmId );
      return XST_FAILURE;
    }
    
    if(XPrc_IsVsmInShutdown(&Prc, VsmId) == 0) {
      xil_printf("ERROR: Programming VSM %0d but it is active \n\r", VsmId );
      return XST_FAILURE;
    }
    
    for (u16 RmId = 0; RmId < pRPInfo->NumberOfRMs; RmId++){
      pRMInfo = PR_TFTP_GetRMInfoByID(pRPInfo, RmId);
      
      if (pRMInfo == NULL) {
        xil_printf("ERROR Cannot retrieve information for VSM %0d RM index 0\n\r", VsmId );
        return XST_FAILURE;
      }
      
      // ------------------
      // Fetch the partial
      // ------------------
      //
      
      //   ---- \/\/\/\/\/\/\/ ----
      //  Use this code if you want to preallocate the buffer based on the size from the RmInfo file
      // 
      // 
      //  xil_printf("Fetching the partial for VSM %0d RM %0d (%0d bytes): %s\n\r", VsmId, RmId, pRMInfo->BsSize, pRMInfo->pFileName);
      // 
      //  pBuffer = malloc (pRMInfo->BsSize);
      //  if (pBuffer == NULL) {
      //    xil_printf("ERROR: Error creating data buffer for VSM %d RM %d partial. Out of Memory\n\r", VsmId, RmId);
      //    return XST_FAILURE;
      //  }
      // 
      // // Setup some transfer options
      // //
      // TransferOptions.ReallocateMemoryIfRequired = 0; // We know how big the partials are and we have already
      //                                                 // allocated the buffer we need so don't let the file fetch
      //                                                 // do this - something has gone wrong if it needs more memory.
      // TransferOptions.DebugMemoryAllocation = 0;
      // TransferOptions.DebugTftp             = 0;
      // 
      // 
      // Err = PR_TFTP_FetchPartialToMem(&LocalNetif              , // The network interface to use
      //                                 ServerIpAddr             , // The TFTP server IP address
      //                                 pRMInfo->pFileName       , // The path & name of the file to fetch
      //                                 &pBuffer                 , // A pointer to a memory buffer to store the file
      //                                 &pRMInfo->BsSize         , // A pointer to a variable holding the size of the buffer.  As
      //                                                            // this call preallocates the correct buffer, this won't change
      //                                 NULL                     , // A pointer to a variable to return the number of bytes stored
      //                                 &PR_TFTP_TimerCount      , // A pointer to a counter incremented in a timer callback somewhere
      //                                 PR_TFTP_TIMEOUT_THRESHOLD, // How long to wait on a TFTP packet before timing out
      //                                 PR_TFTP_TIMEOUT_RETRY    , // How many times to retry a failed packet
      //                                 &TransferOptions           //
      //                                 );
      // 
      //   ---- /\/\/\/\/\/\/\ ----
      
      //  ---- \/\/\/\/\/\/\/ ----
      // Use this code if you want to let the TFTP library allocate the memory for you.  This means you don't
      // have to have the correct values in the RmInfo CSV file which might make it easier to set up and maintain.
      // The cost is that the code will be slower as memory will have to be reallocated.
      {
        u32 DataBufferSize = 0;
        
        xil_printf("Fetching the partial for VSM %0d RM %0d (unknown size): %s\n\r", VsmId, RmId, pRMInfo->pFileName);
        
        TransferOptions.ReallocateMemoryIfRequired = 1; 
        TransferOptions.IncrementAmount            = 1024*10;   // Allocate in 10K chunks for speed
        TransferOptions.DebugMemoryAllocation      = 0;
        TransferOptions.DebugTftp                  = 0;
        
        pRMInfo->BsSize = 0;
        Err = PR_TFTP_FetchPartialToMem(&LocalNetif              , // The network interface to use
                                        ServerIpAddr             , // The TFTP server IP address
                                        pRMInfo->pFileName       , // The path & name of the file to fetch
                                        &pBuffer                 , // A pointer to a memory buffer to store the file
                                        &DataBufferSize          , // A pointer to a variable holding the size of the buffer.
                                        &pRMInfo->BsSize         , // A pointer to a variable to return the number of bytes stored
                                        &PR_TFTP_TimerCount      , // A pointer to a counter incremented in a timer callback somewhere
                                        PR_TFTP_TIMEOUT_THRESHOLD, // How long to wait on a TFTP packet before timing out
                                        PR_TFTP_TIMEOUT_RETRY    , // How many times to retry a failed packet
                                        &TransferOptions           //
                                        );
        xil_printf("Fetched the partial for VSM %0d RM %0d.  Size = %0d bytes\n\r", VsmId, RmId, pRMInfo->BsSize);
        xil_printf("%0d bytes of memory wasted\n\r", DataBufferSize - pRMInfo->BsSize);
      }
      
      if (Err != PR_TFTP_ERR_OK) {
        xil_printf("Failed to fetch Partial Bitstream %s. Aborting program. \n\r", pRMInfo->pFileName);
        return XST_FAILURE;
      }
      

      // ----------------------------
      // Program the PRC for this RM
      // ----------------------------
      //
      XPrc_SetRmControl(&Prc, VsmId, RmId, pRMInfo->ShutdownRequired, pRMInfo->StartupRequired, pRMInfo->ResetRequired, pRMInfo->ResetDuration);
      XPrc_SetRmBsIndex(&Prc, VsmId, RmId, pRMInfo->BsIndex);
      XPrc_SetBsSize   (&Prc, VsmId, pRMInfo->BsIndex, pRMInfo->BsSize);
      XPrc_SetBsAddress(&Prc, VsmId, pRMInfo->BsIndex, (u32)pBuffer);
      
      //// Readback for debug
      //// ------------------
      //{
      //  int unsigned ResetDuration = 0;
      //  int unsigned ResetRequired = 0;
      //  int unsigned StartupRequired = 0;
      //  int unsigned ShutdownRequired = 0;
      //  int unsigned BsIndex = 0;
      //  
      //  XPrc_GetRmControl(&Prc, VsmId, RmId, (u8*)&ShutdownRequired, (u8*)&StartupRequired, (u8*)&ResetRequired, (u8*)&ResetDuration);
      //  BsIndex = XPrc_GetRmBsIndex(&Prc, VsmId, RmId);
      //  
      //
      //  xil_printf ("  Programmed VSM %d RM %d ResetDuration = %0d\n\r",
      //              VsmId, RmId, ResetDuration);
      //
      //
      //  xil_printf ("  Programmed VSM %d RM %d RM_CONTROL REG = ShutdownRequired = %0d, StartupRequired = %0d, ResetRequired = %0d, ResetDuration = %0d\n\r",
      //              VsmId, RmId, ShutdownRequired, StartupRequired, ResetRequired, ResetDuration);
      //  xil_printf ("  Programmed VSM %d RM %d BS_INDEX   REG = %0d\n\r", VsmId, RmId, (int unsigned)XPrc_GetRmBsIndex (&Prc, VsmId, RmId));
      //  xil_printf ("  Programmed VSM %d RM %d BS_SIZE    REG = %0d\n\r", VsmId, RmId, (int unsigned)XPrc_GetBsSize    (&Prc, VsmId, (u16) BsIndex));
      //  xil_printf ("  Programmed VSM %d RM %d BS_ADDRESS REG = %x\n\r" , VsmId, RmId, (int unsigned)XPrc_GetBsAddress (&Prc, VsmId, (u16) BsIndex));
      //  xil_printf ("\n\r");
      //}
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
    PR_TFTP_TimerCount = 0;  // We aren't doing any more TFTP transfers so reuse this timer variable
    
    while(1){
      // Change the RMs every 5 seconds (approx)
      if(PR_TFTP_TimerCount > 5){
        PR_TFTP_TimerCount = 0;
        for (u16 i = 0; i < XPrc_GetNumberOfVsms(&Prc); i++){
          pRPInfo = PR_TFTP_GetRPInfoByIndex(pRPInfoArray, XPrc_GetNumberOfVsms(&Prc), i);
          pRPInfo->ActiveRM = (pRPInfo->ActiveRM+1) % pRPInfo->NumberOfRMs;
          XPrc_SendSwTrigger (&Prc, pRPInfo->Id, pRPInfo->ActiveRM);
        }
      }
    }
  }
  
  cleanup_platform();
  
  // We never get to this point but if we did, here's how to clean up the memory
  // that was used
  PR_TFTP_FreeDataStructure(XPrc_GetNumberOfVsms(&Prc), &pRPInfoArray);
  
  // We stored each bitstream locally but didn't store the pointers to them, except 
  // in the PRC.  To deallocate that memory, we would have to read the bitstream addresses
  // from the PRC and free them.  
  
  return 0;
}
