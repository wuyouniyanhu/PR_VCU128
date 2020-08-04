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

#ifndef PR_TFTP_H_
#define PR_TFTP_H_

#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "string.h"

// ==============================================================================================================================
// Macros and defines
// ==============================================================================================================================

// Define the maximum size of filename we can handle for the partial
//   Limited by maximum path length in windows
//   Limited by the maximum size of an RRQ packet with is (512 - 2 (OpCode) - 1 - 5 (for octet mode) - 1 - Option Size) bytes
//     = (503 - Option Size) bytes
#define PR_MAX_FILENAME 256

// This is called when a TFTP data packet is received.
typedef void (*pr_tftp_recv_data_fn)(void *Arg, char *Data, int unsigned NumberOfBytes);

// Error values
typedef s8 pr_tftp_err_t;

#define PR_TFTP_ERR_OK                   0 /* No Error - everything is ok                                             */ 
#define PR_TFTP_ERR_UNEXPECTED_PACKET   -1 /* UDP packet received but no request has been made                        */            
#define PR_TFTP_ERR_BAD_OPCODE          -2 /* Unknown or unexpected opcode in received TFTP packet                    */            
#define PR_TFTP_ERR_NOBUF_NOCALLBACK    -3 /* No data buffer has been allocated and no receive data callback supplied */            
#define PR_TFTP_ERR_BUF_TOO_SMALL       -4 /* The data buffer is too small for the partial                            */            
#define PR_TFTP_ERR_CANT_ALLOC_MEM      -5 /* Unable to allocate memory                                               */            
#define PR_TFTP_ERR_PACKET_FAILED       -6 /* Unable to send packet to server port                                    */            
#define PR_TFTP_ERR_TIMEOUT             -7 /* TFTP Timeout                                                            */            
#define PR_TFTP_ERR_CANT_SETUP_UDP      -8 /* Can't setup UDP connection                                              */            
#define PR_TFTP_ERR_TFTP_ERR            -9 /* A TFTP error occurred - see the TFTP specific error flag                */            
#define PR_TFTP_ERR_BAD_FILE           -10 /* A file could not be parsed correctly                                    */
#define PR_TFTP_ERR_BAD_PARAM          -11 /* A bad parameter value was passed to a function                          */


// ==============================================================================================================================
// Data Structures
// ==============================================================================================================================

// The options struct is populated by the user to control some aspects of the file fetch.
// It does not have to be used and if it isn't, default behaviour will be applied.
// If it is supplied, all options have to be set properly
//
typedef struct pr_tftp_options_s{

  // ---------------------------------
  // Memory Management
  // ---------------------------------
  // Control what happens if the data buffer isn't big enough for the received data.
  //  0: Issue an error and abort
  //  1: Reallocate the buffer
  char unsigned ReallocateMemoryIfRequired :1;

  // If the buffer does have to be reallocated, add this much memory.  If the the 
  // increment amount is too small then it will be ignored and the minimum required 
  // amount will be used.  
  u32 IncrementAmount;

  // ---------------------------------
  // Debug
  // ---------------------------------
  //
  char unsigned DebugMemoryAllocation :1;
  char unsigned DebugTftp             :1;

} pr_tftp_options_s;


// NOTE: This structure manages a file receive operation only.
//       It is not designed to write files to a TFTP server.
struct pr_tftp_information_s{
  
  // ---------------------------------
  // Memory information
  // ---------------------------------
  // Pointer to a pointer to the buffer to hold the received data.
  // The double pointer is required because the memory might get reallocated, 
  // in which case I need to be able to update the pointer that the calling 
  // function sees

  char **ppBuffer;
  
  //  The buffer size in bytes
  u32 BufferSize;
  
  // How many bytes are currently in use
  u32 NumberOfBytesWritten;
  
  // ---------------------------------==
  // Information about the file to fetch
  // ---------------------------------==
  
  // The name of the file to fetch
  char Filename[PR_MAX_FILENAME+1];
  
  // ---------------------------------
  // TFTP information
  // ---------------------------------
  char Mode [9];
  
  struct netif   *LocalNetif;
  struct ip_addr  ServerIPAddr;
  struct udp_pcb *Pcb;
  
  // ---------------------------------
  // Transfer status
  // ---------------------------------
  u16 LastBlockId;      // The ID of the last block received
  u16 LastBlockLength;  // The length of the last block received

  // Set to 1 when I have received at least one DATA block.  I can't rely on BlockID being 
  // non-zero because a large transfer will cause that to wrap to zero
  char unsigned LastBlockInfoValid :1;

  // Set to 1 when a request is made.  Used by the receive callback to check for an initial UDP packet that 
  // wasn't actually requested.  It's probably not necessary but it will catch at least one error
  //
  char unsigned RequestedFile : 1;

  // This is a handshake flag.  It's incremented by the receive callback when a block has been receieved, 
  // and it's cleared by pr_tftp_send_packet_and_wait so that it can detect when the next block has been 
  // rceeived
  // 
  char  BlocksToProcess;  

  // The transfer has completed.  USe "error" to check if it passed or failed.  
  char unsigned Complete : 1;
  
  // Asserted if an error occurred in the transfer
  char unsigned Error : 1;
  
  // The error code sent by the TFTP server
  u16 TftpErrorCode;
  
  // The PR TFTP Error code.  This reports why the file fetch failed
  pr_tftp_err_t ErrorCode;

  // ---------------------------------
  // Timeout Handling
  // ---------------------------------
  
  // A pointer to a volatile variable that is updated by a timer interrupt somewhere in the system.
  // 
  volatile int unsigned *pTimeoutTickCount;

  // How many timer ticks can we see before declaring a timeout?
  // The time between timer interrupts depends on how the system is configured, so this value has to take this into account.
  // For example, a 100 MHz clock with a timer load value of 16,500,000 has a timer period of 0.165 seconds.  
  // A 30 second TFTP timeout would need a value of 182
  //
  int unsigned TimeoutThreshold;

  // How many times should we retry a packet that has timed out?
  //
  int unsigned TimeoutRetryAttempts;

  
  // ---------------------------------
  // Callbacks
  // ---------------------------------
  
  pr_tftp_recv_data_fn RecvDataCallback;
  void *               RecvDataCallbackArg;


  // ---------------------------------
  // Options
  // ---------------------------------
  pr_tftp_options_s   *pOptions;
};


struct pr_tftp_data_packet_s{
  u16 OpCode;
  u16 BlockID;
  char  firstByteOfData;
};

struct pr_tftp_ack_packet_s{
        u16 OpCode;
        u16 BlockID;
};

struct pr_tftp_error_packet_s{
        u16 OpCode;
        u16 ErrorCode;
        char *ErrorMsg;
};

/*
 *  A struct to hold information about a single Reconfigurable Module.  
 */

typedef struct pr_tftp_rm_s {
  u16 Id;                   // The Reconfigurable Module Identifier
  u8 ResetDuration;         // The length of reset (in clock cycles) that this Reconfigurable Module needs
  u8 ResetRequired;         // The type of reset this Reconfigurable Module needs.  See the PRC IP Product Guide for encodings 
  u8 StartupRequired;       // The type of startup this Reconfigurable Module needs.  See the PRC IP Product Guide for encodings
  u8 ShutdownRequired;      // The type of shutdown this Reconfigurable Module needs.  See the PRC IP Product Guide for encodings
                            
  u16 BsIndex;              // The index of the Bitstream in the Bitstream Information register bank that holds information about 
                            // the partial bitstream for this Reconfigurable Module. See the PRC IP Product Guide for more information.
                            // NOTE: This could theoretically end up overflowing if clearing bitstreams are needed as it can be (2*Id)+1 
                            //       and Id is also 16 bits.  However, it's only used for the PRC and that limits it to 16 bits so it's 
                            //       unlikely to be a real issue
                            
  u32 BsSize;               // The size of the partial bitstream (in bytes)
  char * pFileName;         // The full name (including the path) of the partial bitstream on the TFTP server

#ifdef PR_TFTP_REQUIRES_CLEARING_BITSTREAM
  u16 ClearingBsIndex;      // The index of the bitstream in the PRC's Bitstream Information Register Bank that holds information 
                            // about the clearing bitstream for this Reconfigurable Module. See the PRC IP Product Guide for 
                            // more information
                            // NOTE: This could theoretically end up overflowing if clearing bitstreams are needed as it can be (2*Id)+1 
                            //       and Id is also 16 bits.  However, it's only used for the PRC and that limits it to 16 bits so it's 
                            //       unlikely to be a real issue
  u32 ClearingBsSize;       // The size of the clearing bitstream (in bytes)
  char * pClearingFileName; // The full name (including the path) of the clearing bitstream on the TFTP server.
#endif
} pr_tftp_rm_s;


/*
 *  A struct to hold information about a single Reconfigurable Partition.  
 */
typedef struct pr_tftp_rp_s {
  u16 Id;
  u16 NumberOfRMs;
  s32 ActiveRM;   // -1 if nothing is loaded
  pr_tftp_rm_s ** pRMInfos;
} pr_tftp_rp_s;


// ==============================================================================================================================
// Function Prototypes
// ==============================================================================================================================

// ---------------------------------------------------------------------------------------------
// Functions to fetch data over TFTP
// ---------------------------------------------------------------------------------------------

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//
// These functions fetch data in network byte order (Big Endian).  
// Depending on how you prepared your bitstreams and the endiness of your 
// system, you may need to manually convert the data to host byte order 
// using functions such as ntohl().
// For example:
//     * A Little Endian system using bin files created by write_bitstream 
//       will have to convert the data
//
//     * A Little Endian system using bin files created by 
//       write_cfgmem -interface SMAPx32 will not have to convert the data
//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


// Fetch a partial bitstream and store in a memory buffer.
//
pr_tftp_err_t PR_TFTP_FetchPartialToMem      (struct netif           *LocalNetif,
                                              struct ip_addr          ServerIPAddr,
                                              char                   *Filepath,
                                              char                  **ppDataBuffer,
                                              u32                    *DataBufferSize,
                                              u32                    *DataBufferUsed,
                                              volatile int unsigned  *pTimeoutTickCount,
                                              int unsigned            TimeoutThreshold,
                                              int unsigned            TimeoutRetryAttempts,
                                              pr_tftp_options_s      *pOptions);

// Fetch a partial bitstream and call a callback for every data packet that is received
//
pr_tftp_err_t PR_TFTP_FetchPartialToFunction( struct netif          *LocalNetif,
                                              struct ip_addr         ServerIPAddr,
                                              char                  *Filepath,
                                              pr_tftp_recv_data_fn   RecvDataCallback,
                                              void                  *RecvDataCallbackArg,
                                              volatile int unsigned *pTimeoutTickCount,
                                              int unsigned           TimeoutThreshold,
                                              int unsigned           TimeoutRetryAttempts,
                                              pr_tftp_options_s     *pOptions);


// Fetch an RM Information (CSV) file and store in a memory buffer.
//
pr_tftp_err_t PR_TFTP_FetchRmInfoToMem      ( struct netif          *LocalNetif,
                                              struct ip_addr         ServerIPAddr,
                                              char                  *Filepath,
                                              char                 **ppDataBuffer,
                                              u32                   *DataBufferSize,
                                              u32                   *DataBufferUsed,
                                              volatile int unsigned *pTimeoutTickCount,
                                              int unsigned           TimeoutThreshold,
                                              int unsigned           TimeoutRetryAttempts,
                                              pr_tftp_options_s     *pOptions);

// Fetch an RM Information (CSV) file and call a callback for every data packet that is received
//
pr_tftp_err_t PR_TFTP_FetchRmInfoToFunction ( struct netif          *LocalNetif,
                                              struct ip_addr         ServerIPAddr,
                                              char                  *Filepath,
                                              pr_tftp_recv_data_fn   RecvDataCallback,
                                              void                  *RecvDataCallbackArg,
                                              volatile int unsigned *pTimeoutTickCount,
                                              int unsigned           TimeoutThreshold,
                                              int unsigned           TimeoutRetryAttempts,
                                              pr_tftp_options_s     *pOptions);

// ---------------------------------------------------------------------------------------------
// Functions to access RM Information
// ---------------------------------------------------------------------------------------------

// Return a pointer to the RPInfo struct for RpId or null if it doesn't exist
//
pr_tftp_rp_s * PR_TFTP_GetRPInfoByIndex                     (pr_tftp_rp_s *RPInfo, u16 NumRPs, u16 Index);
pr_tftp_rp_s * PR_TFTP_GetRPInfoByID                        (pr_tftp_rp_s *RPInfo, u16 NumRPs, u16 RpId);

// Return a pointer to the RMInfo struct for rm_id or null if it doesn't exist
//
pr_tftp_rm_s * PR_TFTP_GetRMInfoByID                        (pr_tftp_rp_s *pRPInfo, u16 RmId);
pr_tftp_rm_s * PR_TFTP_GetRMInfoByIndex                     (pr_tftp_rp_s *pRPInfo, u16 Index);

pr_tftp_err_t PR_TFTP_CreateDataStructure                   (u16 NumRPs            , pr_tftp_rp_s **pRPInfo);
pr_tftp_err_t PR_TFTP_FreeDataStructure                     (u16 NumRPs            , pr_tftp_rp_s **pRPInfoArray);
pr_tftp_err_t PR_TFTP_PrintDataStructure                    (u16 NumRPs            , pr_tftp_rp_s *pRPInfoArray);
pr_tftp_err_t PR_TFTP_CreateRMStructure                     (pr_tftp_rp_s *pRPInfo , u16 RmId, pr_tftp_rm_s ** pRM);

pr_tftp_err_t PR_TFTP_InitialiseDataStructureFromRmInfoFile (char          *pRmInfoFile,
                                                             u32            FileSize, 
                                                             u16            NumRPs,  
                                                             pr_tftp_rp_s **pRPInfoArray);


// --------------------------------------------------------------------------------------------
// [FUNCTION] Get the library version
// --------------------------------------------------------------------------------------------
//
int unsigned PR_TFTP_GetVersion();


#endif /* PR_TFTP_H_ */


