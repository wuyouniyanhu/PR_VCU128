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
* TFTP (Trivial File Transfer Protocol) server as needed and passes them  
* directly to the AXI HWICAP without buffering them in memory.
* This demonstrates the "Fetch to Function" mode of the PR TFTP library.
*
* RPs and RMs
* ===========
* The design is based on the standard PR tutorial design consisting of SHIFT 
* and COUNT RPs.  It is implemented on a KC705 board
*  
* TFTP server
* ===========
* The application requires a TFTP server to be running on the computer  
* specified by the "ServerIpAddr" variable.    
* This needs to provide a comma seperate value file called "example_3/rm_info.csv" 
* which provides the following information for each partial bitstream: 
*          RP ID
*          RM ID
*          Reset Duration    (Not used in this example)
*          Reset Required    (Not used in this example)
*          Startup Required  (Not used in this example)
*          Shutdown Required (Not used in this example)
*          BS Size           (Not used in this example)
*          File Name
*    
*  For example
*  #RP_ID, RM_ID, Reset Duration, Reset Required, Startup Required, Shutdown Required, BS_SIZE, FILE_NAME
*       0,     0,              3,              3,                1,                 2,  375300, example_3/shift_right.bin 
*       0,     1,              3,              3,                1,                 2,  375300, example_3/shift_left.bin
*  
*  
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
******************************************************************************/

#include <stdio.h>
#include "xparameters.h"
#include "pr_tftp.h"           // Include the PR TFTP library code
#include "xhwicap.h"
#include "netif/xadapter.h"
#include "string.h"
#include "platform.h"
#include "platform_config.h"
#include "xil_printf.h"
#include "lwip/udp.h"
#include "xil_cache.h"

#define NUM_RP 2

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

// The instance of the AXI HWICAP
XHwIcap HWICAPInst; 

// The callback for the PR TFTP File Receive function.  This is called for ever partial bitstream 
// data packet that we receive over TFTP.  It passes the data to the AXI HWICAP directly.
//

void RecvDataCallback (void *Arg, char * Data, int unsigned NumberOfBytes){
  int Status;

  XHwIcap *InstancePtr = (XHwIcap*) Arg;

  // Data is a non word aligned array of chars in network byte order.  Non word aligned means
  // the data can be in memory like this:
  //  Address  
  //    000CD420     00DD FFFF
  //    000CD424     0022 0000
  //    000CD428     FFFF 8844
  //    000CD42C     FFFF FFFF
  //    000CD430     AA66 FFFF   <--         Part of Sync Word
  //    000CD430     0000 5599   <-- Another part of Sync Word
  //
  // Notice that the sync word (0xAA995566) is split over two 32 memory locations.
  // This means we can't access the data as 32 bit data by just reinterpreting the 
  // buffer using a pointer cast.  We need to copy it to a 32 bit aligned buffer.
  //
  // If TFTP options are not used (they aren't supported in the PR TFTP library without modification)
  // then the maximum size of the data payload will be 512 bytes (128 32 bit words).  Declare a static 
  // array of 128 32 bit words for speed.  Malloc and free could be used, but that would have a speed 
  // impact
  //
  static u32 FormattedPayload[128];
  memcpy(FormattedPayload, Data, NumberOfBytes);

  // The bitstreams used in this example are the bin files created by write_bitstream.
  // They are in a little endian format but become big endian when transmitted over the network
  // (network byte order is always big endian).  This loop converts them from network byte order
  // (big endian) back to host byte order (little endian in this example).
  //
  // If the bitstreams from the Partials directory were used instead, this code wouldn't be needed.
  // The bitstreams in the Partials directory are created using "write_cfgmem -interface SMAPx32"
  // which converts them to big endian for the ICAP.  When they are transmitted over TFTP they are
  // byte swapped back to little endian, so there's nothing we need to do here.  

  for (int i = 0; i < NumberOfBytes/4; i++) {
    FormattedPayload[i] = ntohl(FormattedPayload[i]);
  }


  Status = XHwIcap_DeviceWrite(InstancePtr,              // A pointer to the XHwIcap instance.
                               FormattedPayload,         // A pointer to the data to be written to the ICAP device.
                               (u32)(NumberOfBytes / 4)  // The number of words to write to the ICAP device.
                               );
  if (Status != XST_SUCCESS) {
    xil_printf("RecvDataCallback DeviceWrite failed\n\r");
  }

  if (NumberOfBytes != 512) {
    xil_printf("RecvDataCallback finished loading partial bitstream\n\r");
  }
} 


void print_app_header(struct ip_addr *TFTPServerIpAddr){
    xil_printf("======================================================================================\n\r");
    xil_printf("An example Microblaze application that fetches partial bitstreams from a TFTP server \n\r");
    xil_printf("on demand and stores them in a common buffer.  The CPU loads the bitstreams from this \n\r");
    xil_printf("buffer.  Only one bitstream is stored at any time.                                    \n\r");
    xil_printf("======================================================================================\n\r");
    xil_printf("  RPs and RMs\n\r");
    xil_printf("  -----------\n\r");
    xil_printf("  The design is based on the standard PR tutorial design consisting of SHIFT and COUNT\n\r");
    xil_printf("  RPs.  It is implemented on a KC705 board\n\r");
    xil_printf("    SHIFT RP:  \n\r");
    xil_printf("           Triggered by the EAST (SW3) and WEST buttons (SW6)\n\r");
    xil_printf("           Software loads the RM that isn't already loaded\n\r");
    xil_printf("    COUNT RP:  \n\r");
    xil_printf("           Triggered by the NORTH (SW2) and SOUTH buttons (SW4)\n\r");
    xil_printf("           Software loads the RM that isn't already loaded\n\r");
    xil_printf("  TFTP server\n\r");
    xil_printf("  -----------\n\r");
    xil_printf("  The application requires a TFTP server to be running on the computer at \n\r");
    xil_printf("  %d.%d.%d.%d.\n\r", ip4_addr1(TFTPServerIpAddr), ip4_addr2(TFTPServerIpAddr),
                                     ip4_addr3(TFTPServerIpAddr), ip4_addr4(TFTPServerIpAddr));
    xil_printf("  This needs to provide a file called \"example_3/rm_info.csv\" which provides the following  \n\r");
    xil_printf("  information for each partial bitstream: \n\r");
    xil_printf("         RP ID\n\r");
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
  
  // A pointer to the HWICAP's configuration information.
  //
  XHwIcap_Config *pHWICAPConfig = NULL;

  // Various IP addresses
  //
  struct ip_addr BoardIpAddr, Netmask, GatewayIpAddr, ServerIpAddr;
  
  // The mac address of the board. this should be unique per board
  //
  unsigned char MacEthernetAddress[] = { 0x00, 0x0a, 0x35, 0x02, 0xAE, 0xE3 };

  // --------------------------------------------------------------------
  // Set up the application
  // --------------------------------------------------------------------

  // Initialise the AXI HWICAP driver
  pHWICAPConfig = XHwIcap_LookupConfig(XPAR_AXI_HWICAP_0_DEVICE_ID);
  if (pHWICAPConfig == NULL) {
    return XST_FAILURE;
  }

  Status = XHwIcap_CfgInitialize(&HWICAPInst, pHWICAPConfig, pHWICAPConfig->BaseAddress);
  if (Status != XST_SUCCESS) {
    return XST_FAILURE;
  }

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

  // Enable the interrupts
  //
  platform_enable_interrupts();
  
  // Bring up the network interface
  //
  netif_set_default(&LocalNetif);
  netif_set_up(&LocalNetif);
  
  print_app_header(&ServerIpAddr);
  
  
  // --------------------------------------------------------------------
  // Fetch information about the partial bitstreamss from a TFTP server
  // --------------------------------------------------------------------
  //
  xil_printf("======================================================================================\n\r");
  xil_printf("Fetching RM Information from TFTP Server\n\r");
  xil_printf("======================================================================================\n\r");
  xil_printf("Requesting file from a TFTP server running on %d.%d.%d.%d.  If this times out then check that your firewall is disabled and allows TFTP traffic.\n\r",
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
                                 "example_3/rm_info.csv"   , // The path & name of the file to fetch.
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


  // Print the received file for debug
  //
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
                                                    NUM_RP, 
                                                   &pRPInfoArray) != PR_TFTP_ERR_OK) {
    xil_printf("Failed to parse RM Info file. Aborting program. \n\r");
    return XST_FAILURE;
  }

  // Free the buffer that holds the RM Information CSV file
  free(pRmInfoFile);

  // -----------------------------------------
  // Some debug printing
  // -----------------------------------------
  //
  PR_TFTP_PrintDataStructure(NUM_RP, pRPInfoArray);

 
  // -----------------------------------------
  // Start fetching and loading RMs
  // -----------------------------------------
  {
    u16 ActiveRms[NUM_RP];
    // The static bitstream starts with the Shift Right and Count Up RMs.  These are both ID 0 in 
    // the rm_info.csv file
    //
    ActiveRms[0] = 0;
    ActiveRms[1] = 0;
    
    while (1) {
      // Change the RMs every 5 seconds (approx)
      //
      if(PR_TFTP_TimerCount > 5){
        PR_TFTP_TimerCount = 0;
        
        for (u16 VsmId = 0; VsmId < NUM_RP; VsmId++){
          ActiveRms[VsmId] = (ActiveRms[VsmId]+1) % NUM_RP;
          
          pRPInfo = PR_TFTP_GetRPInfoByIndex(pRPInfoArray, NUM_RP, VsmId);
          
          if (pRPInfo == NULL) {
            xil_printf("ERROR Cannot retrieve information for VSM %0d\n\r", VsmId );
            return XST_FAILURE;
          }
          pRMInfo = PR_TFTP_GetRMInfoByID(pRPInfo, ActiveRms[VsmId]);
          
          if (pRMInfo == NULL) {
            xil_printf("ERROR Cannot retrieve information for VSM %0d RM index 0\n\r", VsmId );
            return XST_FAILURE;
          }
          
          // Turn that into a file name
          xil_printf("Fetching the partial for VSM %0d RM %0d (unknown size): %s\n\r", VsmId, ActiveRms[VsmId], pRMInfo->pFileName);
          
          // ------------------
          // Fetch the partial
          // ------------------
          //
          // Setup some transfer options
          //
          TransferOptions.DebugTftp             = 0;
                    
          Err = PR_TFTP_FetchPartialToFunction(&LocalNetif              , // The network interface to use
                                               ServerIpAddr             , // The TFTP server IP address
                                               pRMInfo->pFileName       , // The path & name of the file to fetch
                                               RecvDataCallback         , // The function to call when data is received
                                               (void *)&HWICAPInst      , // The custom argument to the callback function
                                               &PR_TFTP_TimerCount      , // A pointer to a counter incremented in a timer callback somewhere
                                               PR_TFTP_TIMEOUT_THRESHOLD, // How long to wait on a TFTP packet before timing out
                                               PR_TFTP_TIMEOUT_RETRY    , // How many times to retry a failed packet
                                               &TransferOptions           //
                                               );
          
          if (Err != PR_TFTP_ERR_OK) {
            xil_printf("Failed to fetch Partial Bitstream %s. Aborting program. \n\r", pRMInfo->pFileName);
            return XST_FAILURE;
          }
          
          xil_printf("Fetched Partial Bitstream %s.\n\r", pRMInfo->pFileName);
        }
      }
    }
  }


  // We never get to this point but if we did, here's how to clean up the memory
  // that was used
  PR_TFTP_FreeDataStructure(NUM_RP, &pRPInfoArray);

  cleanup_platform();
  xil_printf("Exiting\n\r");
  return XST_SUCCESS;
}

