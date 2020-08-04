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

#include "pr_tftp.h"
#include "netif/xadapter.h"
#include <ctype.h>

// TODO: Properly document code, especially the public functions

// ============================================================================================
// File structure
// ============================================================================================
//   Private Functions
//   Public Functions to fetch files using TFTP
//   Public Functions to parse files using TFTP
//   Public Functions to work with the data structure
//
// Each section starts with [SECTION] for easy navigation

// ============================================================================================
// Annotations
// ============================================================================================
//     // ====<full width of page>  = Section Delimiter
//     // ----<full width of page>  = Function Delimiter


// ============================================================================================
// [SECTION] Private Functions
// ============================================================================================

// --------------------------------------------------------------------------------------------
// [FUNCTION] Mark a TFTP Transfer as complete
// --------------------------------------------------------------------------------------------
// Called from various places when a transfer completes.  It sets a complete flag and 
// appropriate error fields
// 
void PR_TFTP_MarkTransferComplete(struct pr_tftp_information_s *pInfo, pr_tftp_err_t ErrorCode, u16 TftpErrorCode){
    if(ErrorCode != PR_TFTP_ERR_OK || TftpErrorCode != ERR_OK) {
      pInfo->Error = 1;
    }

    pInfo->Complete      = 1;
    pInfo->ErrorCode     = ErrorCode;
    pInfo->TftpErrorCode = TftpErrorCode;
}

// --------------------------------------------------------------------------------------------
// [FUNCTION] Called when a TFTP Packet is received
// --------------------------------------------------------------------------------------------
// This is the callback function passed to udp_recv.  It's called when a packet arrives from 
// the server
//
void PR_TFTP_ReceiveCallback(void *Arg, struct udp_pcb *Pcb, struct pbuf *p, ip_addr_t *ServerAddr, u16 ServerPort){

  struct pr_tftp_data_packet_s   *pDataPacket;
  struct pr_tftp_error_packet_s  *pErrPacket;
  struct pr_tftp_information_s   *pInfo = (struct pr_tftp_information_s*)Arg;
  u32 RequiredBufferSize;
  u32 RequiredBufferSizeUsingIncrement;
  u32 NumberOfNewBytes;
  u16 OpCode;

  // Set to 1 if a block is seen with the same Block ID as the previous one.  This is used as 
  // a gate on storing the data in memory.  If it's 1, the block is skipped and the data
  // isn't stored
  //
  char          DuplicateBlockSeen = 0;
  
  /* do not read the packet if we are not in ESTABLISHED state */
  if (!p) { return; }
  
  if (pInfo->RequestedFile == 0) {
    xil_printf ("  %s PR TFTP ERROR:  UDP packet received but no request has been made\n\r", __FILE__);
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_UNEXPECTED_PACKET, ERR_OK);
    goto cleanup_and_exit;
  }
  
  if (pInfo->pOptions->DebugTftp){
    xil_printf ("---------------------------------------------------------------------\n\r");
    xil_printf ("PR_TFTP_ReceiveCallback for file %s\n\r", pInfo->Filename);
    xil_printf ("---------------------------------------------------------------------\n\r");
    
    xil_printf ("  UDP received from %"U16_F".%"U16_F".%"U16_F".%"U16_F",port %"U16_F"\n\r",
                ip4_addr1_16(ServerAddr), ip4_addr2_16(ServerAddr),
                ip4_addr3_16(ServerAddr), ip4_addr4_16(ServerAddr),
                ServerPort);
  }
  
  pDataPacket = (struct pr_tftp_data_packet_s*)p->payload;
  
  OpCode = (u16) ntohs(pDataPacket->OpCode);
  switch (OpCode) {
  case 3: // Data Packet
    
    // Check the block number.  If it's the same as the last one I got then that means the ACK was lost
    // and the server is retransmitting.  In this case, ignore the data but send a new ACK.
    //
    // If there's a gap then that means the server sent a packet, I missed it, and something else 
    // acknowledged it.  That's hopefully never going to happen.  In this case, abort the transfer

    if ((u16) ntohs(pDataPacket->BlockID) == pInfo->LastBlockId) {
      if (pInfo->pOptions->DebugTftp){
        xil_printf("WARNING: PR_TFTP_ReceiveCallback: Duplicate block ID received.\n\r");
        xil_printf("WARNING: This can occur if the timout period is too short.  The PR TFTP code\n\r");
        xil_printf("WARNING: resends the RRQ but the server hasn't really timedout out so responds\n\r");
        xil_printf("WARNING: to both RRQs.  The PR TFTP code will ignore this packet.\n\r");
      }
      DuplicateBlockSeen = 1;
    }  else if ((u16) ntohs(pDataPacket->BlockID) != (u16)(pInfo->LastBlockId + 1)) {

      // The cast to u16 handles the wraparound case where the BlockID rolls over from 65535 to 0 

      xil_printf("%s ERROR: PR_TFTP_ReceiveCallback: ERROR received block %0d but was expecting block %0d.  A block has gone missing somewhere.\n\r",
                 __FILE__,
                 (u16) ntohs(pDataPacket->BlockID), pInfo->LastBlockId + 1);
      goto cleanup_and_exit;
    }  

    break;
    
  case 5: // Error Packet
    pErrPacket = (struct pr_tftp_error_packet_s*)p->payload;
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_TFTP_ERR, ntohs(pErrPacket->ErrorCode));
    xil_printf("%s ERROR: PR_TFTP_ReceiveCallback: ERROR opcode received. Error Code = %d. Err Msg = %s\n\r",
               __FILE__,
               ntohs(pErrPacket->ErrorCode), pErrPacket->ErrorMsg);
    goto cleanup_and_exit;
    
    //case 6: //Option Acknowledgement (not used yet)
    //      break;
    //case 8: // Fail due to options negotiation (not used yet)
    //      return;
    //      break;
    
  default:
    xil_printf("%s ERROR: PR_TFTP_ReceiveCallback: Unknown or unexpected OpCode = %d in received TFTP packet\n\r", 
               __FILE__, OpCode);
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_BAD_OPCODE, ERR_OK);

    goto cleanup_and_exit;
  }
    
  // There are two options here depending on the system architecture:
  //   1. Buffer the data in memory and then load the RM when that's complete
  //   2. Send the received data straight to the ICAP.
  // Which one to use is based on the values set for the callback and the buffer.  If either is set it will
  // be used.  This means you can call the callback *and* fill in the buffer
  
  if ((*pInfo->ppBuffer == NULL && pInfo->pOptions->ReallocateMemoryIfRequired == 0) && pInfo->RecvDataCallback == NULL) {
    // Error case.  The buffer is null and we're not allowed to allocate it *AND* there's no callback
    // function specified.  I have no where to send the data.
    //
    xil_printf("%s ERROR: PR_TFTP_ReceiveCallback: No data buffer has been allocated and no receive data callback supplied\n\r",
               __FILE__);
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_NOBUF_NOCALLBACK, ERR_OK);

    goto cleanup_and_exit;
  }

  pInfo->LastBlockId         = (u16) ntohs(pDataPacket->BlockID);
  pInfo->LastBlockLength     = p->len;
  pInfo->LastBlockInfoValid  = 1;
  pInfo->BlocksToProcess++; // Using increment so I can check if I ever get multiple blocks without handling them properly
  if (pInfo->pOptions->DebugTftp){
    xil_printf("  pInfo->LastBlockId     = %d\n\r", pInfo->LastBlockId);
    xil_printf("  pInfo->LastBlockLength = %d\n\r", pInfo->LastBlockLength);
    xil_printf("  pInfo->BlocksToProcess = %d\n\r", pInfo->BlocksToProcess);
  }

  pInfo->Pcb->remote_port = ServerPort;



  // Call the callback with the new data
  // ======================================
  
  if (pInfo->RecvDataCallback != NULL) {
    NumberOfNewBytes = p->len - 4;

    pInfo->RecvDataCallback((void *)pInfo->RecvDataCallbackArg, (char *) &pDataPacket->firstByteOfData, NumberOfNewBytes);

    if(pInfo->pOptions->ReallocateMemoryIfRequired == 0) {
      // Data is handled using the callback, and I've not been requested to
      // store it in memory as well. Clean up
      goto cleanup_and_exit;
    }
  }
  
  // Store the payload in the data buffer
  // ======================================

  if (DuplicateBlockSeen == 0) {
    NumberOfNewBytes = p->len - 4;
    
    if (*pInfo->ppBuffer != NULL  || pInfo->pOptions->ReallocateMemoryIfRequired == 1) {
      // I either have a valid pointer, or I'm allowed to allocate memory for a null pointer
      
      RequiredBufferSize = (u32)pInfo->NumberOfBytesWritten + (u32)NumberOfNewBytes;
      if (RequiredBufferSize > (u32) pInfo->BufferSize) {
        if (pInfo->pOptions->ReallocateMemoryIfRequired == 1) {
          if (pInfo->pOptions->DebugMemoryAllocation){
            xil_printf("PR_TFTP_ReceiveCallback: The data buffer was not large enough so reallocating\n\r");
          }
          
          // The new buffer should be the greater of:
          //  1. the old buffer + the number of extra bytes required
          //  2. the old buffer + the user specified increment amount
          RequiredBufferSizeUsingIncrement = ((u32)pInfo->NumberOfBytesWritten + (u32)pInfo->pOptions->IncrementAmount);
          if (RequiredBufferSize > RequiredBufferSizeUsingIncrement){
            // This acts like malloc if ptr is null so no need to detect the null condition
            //
            *pInfo->ppBuffer = realloc(*pInfo->ppBuffer, RequiredBufferSize);
            pInfo->BufferSize = RequiredBufferSize;
          } else {
            // This acts like malloc if ptr is null so no need to detect the null condition
            //
            *pInfo->ppBuffer = realloc(*pInfo->ppBuffer, RequiredBufferSizeUsingIncrement);
            pInfo->BufferSize = RequiredBufferSizeUsingIncrement;
          }
          
          if (*pInfo->ppBuffer == NULL) {
            xil_printf("%s ERROR: PR_TFTP_ReceiveCallback: The data buffer was not large enough and a realloc has failed.\n\r",
                       __FILE__);
            PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_CANT_ALLOC_MEM, ERR_OK);
            goto cleanup_and_exit;
          }
        } else{
          // I don't have enough memory and I'm not allowed to allocate any more
          xil_printf("%s ERROR: PR_TFTP_ReceiveCallback: The data buffer (%u) is too small for the partial (%lu).\n\r",
                     __FILE__,
                     pInfo->BufferSize, RequiredBufferSize);
          PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_BUF_TOO_SMALL, ERR_OK);
          goto cleanup_and_exit;
        }
      }
      
      //if (pInfo->pOptions->DebugMemoryAllocation){
      //  // xil_printf doesn't handle %p to display pointers 
      //  // This code pulls in printf which might increase teh code size to an unacceptable level, 
      //  // so it defaults to being commented out
      //  printf("  Writing %lu bytes to the data buffer starting at %p\n\r", NumberOfNewBytes,
      //         *pInfo->ppBuffer + pInfo->NumberOfBytesWritten);
      //  printf("  Buffer max   address = %p\n\r", *pInfo->ppBuffer + pInfo->BufferSize-1);
      //  printf("  Buffer write address = %p\n\r", *pInfo->ppBuffer + pInfo->NumberOfBytesWritten);
      //  printf("  Buffer base  address = %p\n\r", *pInfo->ppBuffer );
      //}
      
      memcpy(*pInfo->ppBuffer + pInfo->NumberOfBytesWritten,
             &pDataPacket->firstByteOfData,
             NumberOfNewBytes);
      
      pInfo->NumberOfBytesWritten += NumberOfNewBytes;
      if (pInfo->pOptions->DebugMemoryAllocation){
          xil_printf("%s: Received %0d. %0d bytes remaining in buffer\n\r", pInfo->Filename, pInfo->NumberOfBytesWritten,
                     pInfo->BufferSize - pInfo->NumberOfBytesWritten
                     );
      }
    }
  } // If DuplicateBlockSeen == 0

  // The common exit point from various places in this function.  
 cleanup_and_exit:
  
  if(p != NULL) { pbuf_free(p); }

  if (pInfo->pOptions->DebugTftp){
    xil_printf ("---------------------------------------------------------------------\n\r");
    xil_printf ("PR_TFTP_ReceiveCallback for partial bitstream %s ...done\n\r", pInfo->Filename);
    xil_printf ("---------------------------------------------------------------------\n\r");
  }
}


// --------------------------------------------------------------------------------------------
// [FUNCTION] Send a UDP packet containing "payload" and wait for a response.
// --------------------------------------------------------------------------------------------
//
// The two scenarios this is used in are:
//   1. Send RRQ and wait for DATA
//   2. Send ACK and wait for DATA if the transfer isn't complete
//
// !!! This function takes ownership of "payload" and frees it
//
pr_tftp_err_t PR_TFTP_SendPacketAndWait(struct pr_tftp_information_s *pInfo,
                                        void                         *Payload,
                                        int unsigned                 PayloadLength,
                                        struct netif                 *netif) {
  err_t         Err;
  int           RetryCount = 0;
  struct pbuf  *pBuf;
  pr_tftp_err_t ReturnCode = PR_TFTP_ERR_OK;

  if (pInfo->pOptions->DebugTftp){
    xil_printf("----------------------------------------------\n\r");
    xil_printf("PR_TFTP_SendPacketAndWait called\n\r");
    xil_printf("----------------------------------------------\n\r");
    
    xil_printf("  pInfo->BlocksToProcess = %d\n\r", pInfo->BlocksToProcess);
    if (pInfo->BlocksToProcess == 0) {
      xil_printf("  There have been no previous blocks so this should be an RRQ\n\r");
    }
    if (pInfo->BlocksToProcess == 1) {
      xil_printf("  There has been one previous block so this should be an ACK\n\r");
    }
    if (pInfo->BlocksToProcess > 1) {
      xil_printf("  There has been more than one previous block which is strange\n\r");
    }
  }

  // NOTE: This is set in the receive callback which might become interrupt based at some point.  
  //       If it does then there's a risk it will interrupt the "--" here (a read-modify-write operation).  
  //       It might be necessary to disable the interrupts here, and then re-enable them when I'm done

  if (pInfo->BlocksToProcess > 0) {
    // If a block had previously been received then we can clear that now as we're about to 
    // send a new block which invalidates the last one
    //
    pInfo->BlocksToProcess--;
  }

  // If a timeout occurs then I need to repeat the whole process. Recursively calling the function 
  // would be expensive,  and using an outer loop and flags to control the exit/continuation of the 
  // two loops would be clumsy.  In this particular case, a goto is a clean solution
  //
  // This target is used when a timeout occurs and a retransmit is required.
  
 send_packet:

  // ------------------------------------------
  // Start of sending a packet
  // ------------------------------------------
  if (pInfo->pOptions->DebugTftp){
    u16  *pOpCode;
    u16  *pBlockID;
    char *pFileName;
    char *pMode;
    int   FilenameLength;

    FilenameLength = strlen(pInfo->Filename);
    xil_printf("  Sending Packet to %"U16_F".%"U16_F".%"U16_F".%"U16_F",port %"U16_F"\n\r",
                ip4_addr1_16(&pInfo->Pcb->remote_ip), ip4_addr2_16(&pInfo->Pcb->remote_ip),
                ip4_addr3_16(&pInfo->Pcb->remote_ip), ip4_addr4_16(&pInfo->Pcb->remote_ip),
                pInfo->Pcb->remote_port);
    
    pOpCode   = (u16*) (Payload+0);

    if (ntohs(*pOpCode) == 1 ) {
      pFileName = (char *) (Payload+2);
      pMode     = (char *) (Payload+2 + FilenameLength + 1);
      xil_printf ("  Packet = | OpCode = %d | Filename = %s | Mode = %s |\n\r", ntohs(*pOpCode), (char*)pFileName, (char*)pMode);
    }
    
    if (ntohs(*pOpCode) == 4 ) {
      pBlockID   = (u16*) (Payload+2);
      xil_printf ("  Packet = | OpCode = %d | Block ID = %d |\n\r", ntohs(*pOpCode), ntohs(*pBlockID));
    }
  }

  // Create the pbuf to send
  pBuf = pbuf_alloc(PBUF_TRANSPORT, PayloadLength, PBUF_RAM);
  if (pBuf == NULL) {
    xil_printf("%s ERROR: PR_TFTP_RequestFile: Unable to allocate memory for retransmit PBUF\n\r",
               __FILE__);
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_CANT_ALLOC_MEM, ERR_OK);
    ReturnCode = pInfo->ErrorCode;
    goto cleanup_and_exit;
  }
  
  // Populate the PBuf's payload
  Err = pbuf_take(pBuf, Payload, PayloadLength);
  if (Err != ERR_OK) {
    xil_printf("%s ERROR: PR_TFTP_SendPacketAndWait: Unable to copy pbuf payload\n\r", __FILE__);
    xil_printf("  End of timeout handling\n\r");
   
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_CANT_ALLOC_MEM, ERR_OK);
    ReturnCode = pInfo->ErrorCode;
    goto cleanup_and_exit;
  }
  
  // Restart the timeout
  *pInfo->pTimeoutTickCount = 0;
  
  Err = udp_sendto_if(pInfo->Pcb, 
                      pBuf, 
                      &pInfo->Pcb->remote_ip,
                      pInfo->Pcb->remote_port, netif);

  if (Err != ERR_OK) {
    xil_printf("%s ERROR: PR_TFTP_SendPacketAndWait: Unable to send packet: err = %d\n\r", __FILE__, Err);
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_PACKET_FAILED, ERR_OK);
    
    if (pInfo->pOptions->DebugTftp){
      xil_printf("----------------------------------------------\n\r");
      xil_printf("PR_TFTP_SendPacketAndWait called ...done\n\r");
      xil_printf("----------------------------------------------\n\r");
    }
    ReturnCode = pInfo->ErrorCode;
    goto cleanup_and_exit;
  }

  // ------------------------------------------
  // End of sending a packet
  // ------------------------------------------
 
  // If the last Block ID was zero then we haven't received any DATA and we are sending a RRQ.
  // I must wait for a response to that.
  // If the last Block ID was not zero then we have received DATA and we are sending an ACK.
  // I only need to wait for a response to that (it will be a DATA packet) if the last packet wasn't a terminating packet.

  
  if (pInfo->LastBlockLength < 516 && pInfo->LastBlockInfoValid > 0 ){
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_OK, ERR_OK);
    if (pInfo->pOptions->DebugTftp){
      xil_printf("  Packet has been sent.  The last block ID was %u and the packet length was %d (< 516) so not expecting any more responses\n\r", pInfo->LastBlockId, pInfo->LastBlockLength);
    }
  } else {
    if (pInfo->pOptions->DebugTftp){
      xil_printf("  Packet has been sent.  Waiting on a response to be seen\n\r");
    }
    // Now the packet has been sent, wait for the data coming back.
    // PR_TFTP_ReceiveCallback() is called when data is received, so all I need to do here is
    //   1. Suck packets from the ethernet interface 
    //   2. Look at the tftp_transfer information to see when a packet has been received,
    
    while (1) {
  
      // ****************************************************
      // Check for ethernet packets
      // ****************************************************

      xemacif_input(netif);  // Returns 0 if there was no packet to read and 1 if there was

      // ****************************************************
      // Handle timeouts
      // ****************************************************

      if(*pInfo->pTimeoutTickCount > pInfo->TimeoutThreshold) {
        xil_printf("%s ERROR: TFTP TIMEOUT\n\r", __FILE__);

        // udp_sendto_if modifies the buffer to add a header to it.  If the transmit fails I can't
        // use the buffer again directly as udp_sendto_if fails with ERR_BUF.  To get round this,
        // free the buffer and allocate a new one on the retransmit

        if (RetryCount >= pInfo->TimeoutRetryAttempts){
          // There's been a timeout and I've exhausted my retry attempts.  Give up
          //
          PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_TIMEOUT, ERR_OK);
       
          if (pInfo->pOptions->DebugTftp){
            xil_printf("----------------------------------------------\n\r");
            xil_printf("PR_TFTP_SendPacketAndWait called ...done\n\r");
            xil_printf("----------------------------------------------\n\r");
          }
          ReturnCode = pInfo->ErrorCode;
          goto cleanup_and_exit;
        } else {
          // There's been a timeout but I have some retry attempts left

          // I'm about to repeat sending a packet so this will get reallocated.  
          // Free it here to prevent a resource leak
          //
          pbuf_free(pBuf);
          RetryCount++;
          if (pInfo->pOptions->DebugTftp){
            xil_printf("  Packet has timed out.  Retry attempt %d of %d\n\r", RetryCount, pInfo->TimeoutRetryAttempts);
          }
          // Just jump out of the loop and start the function again at the appropriate place
          //
          goto send_packet;
        }
      }  // End of timeout handling
      
      if (pInfo->BlocksToProcess > 1) {
        // This is an unexpected condition.  I've received >1 blocks to process.  
        xil_printf("%s pInfo->BlocksToProcess = %d.  This might indicate a problem\n\r", __FILE__, pInfo->BlocksToProcess);
      }
      
      // ----------------------------------------------------
      // Check for block reception
      // ----------------------------------------------------
      //
      if (pInfo->BlocksToProcess > 0) {
        // I've sent a packet and got a response.  My job is done.
        // Something else will need to acknowledge that packet
        if (pInfo->pOptions->DebugTftp){
          xil_printf("  Packet has been sent.  Waiting on a response to be seen ...done\n\r");
          xil_printf("  Exiting PR_TFTP_SendPacketAndWait\n\r");
        }
        goto cleanup_and_exit;
      } // End of "if (pInfo->BlocksToProcess > 0)".  
    } // End of "while(1)" loop that checks for ethernet packets, timeouts and completions
  } // End of "if (pInfo->LastBlockLength < 516 && pInfo->LastBlockInfoValid> 0 )" else branch that handles the DATA packet response to your RRQ or ACK

  // There are multiple failure points in this proc and each one needs to free payload and probably pBuf.
  // I'm using a goto target to make sure I don't leak any resources.
 cleanup_and_exit:
  if(pBuf    != NULL) { pbuf_free(pBuf);   }
  if(Payload != NULL) { free     (Payload); }

  if (pInfo->pOptions->DebugTftp){
    xil_printf("---------------------------------------------------------------\n\r");
    xil_printf("PR_TFTP_SendPacketAndWait called ...done. Return code = %d\n\r", ReturnCode);
    xil_printf("---------------------------------------------------------------\n\r");
  }
  return ReturnCode;
}

// --------------------------------------------------------------------------------------------
// [FUNCTION] Initialise the Transfer Info data structure
// --------------------------------------------------------------------------------------------
//
pr_tftp_err_t PR_TFTP_InitialiseTransferInfo(struct pr_tftp_information_s *pInfo){
  pInfo->LastBlockInfoValid   = 0;
  pInfo->LastBlockId          = 0;
  pInfo->LastBlockLength      = 0;
  pInfo->BlocksToProcess      = 0;
  pInfo->RequestedFile        = 0;
  pInfo->Complete             = 0;
  pInfo->Error                = 0;
  pInfo->ErrorCode            = PR_TFTP_ERR_OK;
  pInfo->TftpErrorCode        = 0;
  pInfo->RecvDataCallback     = NULL;
  pInfo->RecvDataCallbackArg  = NULL;
  pInfo->pTimeoutTickCount    = NULL;
  pInfo->TimeoutThreshold      = 0;
  pInfo->TimeoutRetryAttempts = 0;
  
  
  //create new UDP PCB structure 
  pInfo->Pcb = udp_new();
  if (!pInfo->Pcb) {
    xil_printf("ERROR: PR_TFTP_InitialiseTransferInfo: Error creating PCB. Out of Memory\n\r");
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_CANT_ALLOC_MEM, ERR_OK);
    return pInfo->ErrorCode;
  }
  
  // Don't allocate memory.  I need to let the user handle that because they may want 
  // to keep the partial after it has been received
  //
  pInfo->ppBuffer             = NULL;
  pInfo->NumberOfBytesWritten = 0;
  pInfo->BufferSize           = 0;
  
  pInfo->pOptions = (pr_tftp_options_s *) malloc(sizeof(pr_tftp_options_s));
  if (!pInfo->pOptions) {
    xil_printf("%s ERROR: PR_TFTP_InitialiseTransferInfo: Error creating pOptions. Out of Memory\n\r", 
               __FILE__);
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_CANT_ALLOC_MEM, ERR_OK);
    return pInfo->ErrorCode;
  }

  // Set up some defaults 
  pInfo->pOptions->DebugTftp                  = 0;
  pInfo->pOptions->DebugMemoryAllocation      = 0;
  pInfo->pOptions->ReallocateMemoryIfRequired = 1;
  pInfo->pOptions->IncrementAmount            = 1024;

  return PR_TFTP_ERR_OK;
}

// --------------------------------------------------------------------------------------------
// [FUNCTION] Deallocate memory used in the Transfer Info data sructure
// --------------------------------------------------------------------------------------------
//
pr_tftp_err_t PR_TFTR_DeallocateTransferInfo(struct pr_tftp_information_s *pInfo){
  udp_remove(pInfo->Pcb);
  free(pInfo->pOptions);
  return PR_TFTP_ERR_OK;
}

// --------------------------------------------------------------------------------------------
// [FUNCTION] Setup a TFTP Connection
// --------------------------------------------------------------------------------------------
//
pr_tftp_err_t PR_TFTP_SetupConnection(struct pr_tftp_information_s *pInfo){
  err_t Err;
  u16 LocalPort = 0; // RFC1350: The local port should be randomized.  
                       // The UDP code does that if you set it to 0
  
  if (pInfo->pOptions->DebugTftp){
    xil_printf("===============================================\n\r");
    xil_printf("= Setting up UDP connection                    \n\r");
    xil_printf("===============================================\n\r");
  }
  
  Err = udp_bind(pInfo->Pcb, &pInfo->LocalNetif->ip_addr, LocalPort);
  pInfo->Pcb->remote_ip = pInfo->ServerIPAddr;
  pInfo->Pcb->remote_port = 69; // The TFTP port
  
  if (Err != ERR_OK) {
    xil_printf("%s ERROR: PR_TFTP_SetupConnection: Unable to bind to port %d: Err = %d\n\r", 
               __FILE__, LocalPort, Err);
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_CANT_SETUP_UDP, ERR_OK);

    return pInfo->ErrorCode;
  }
  
  udp_recv(pInfo->Pcb, PR_TFTP_ReceiveCallback, (void *)pInfo);
  return PR_TFTP_ERR_OK;
}


// --------------------------------------------------------------------------------------------
// [FUNCTION] Request a file from the TFTP server
// --------------------------------------------------------------------------------------------
//
pr_tftp_err_t PR_TFTP_RequestFile(struct pr_tftp_information_s *pInfo){
  void *Payload;
  err_t Err;
  u16 OpCode = 1; // Read Request
  
  u16 *pOpCode;
  char  *pFileName;
  char  *pMode;
  
  int Length;
  int FilenameLength;
  int ModeLength;
  
  if (pInfo->pOptions->DebugTftp){
    xil_printf("-----------------------------------------------\n\r");
    xil_printf("- Setting up RRQ Packet                        \n\r");
    xil_printf("-----------------------------------------------\n\r");
  }
  
  // -----------------------------------------------------------
  // Packet Format
  // -----------------------------------------------------------
  //
  //    2 bytes    string     1 byte    string    1 byte
  //   ------------------------------------------------
  //   | Opcode |  Filename  |  0   |    Mode    |  0  |
  //   ------------------------------------------------
  //
  //   Opcode = 1 for RRQ
  
  // -----------------------------------------------------------
  // Create the Packet
  // -----------------------------------------------------------
  //
  FilenameLength = strlen(pInfo->Filename);
  ModeLength     = strlen(pInfo->Mode);
  
  Length = 2 + FilenameLength + 1 + ModeLength + 1;
  
  Payload = calloc(Length, sizeof(char));
  if (Payload == NULL) {
    xil_printf("%s ERROR: PR_TFTP_RequestFile: Unable to allocate memory for RRQ payload\n\r",
               __FILE__);
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_CANT_ALLOC_MEM, ERR_OK);

    return pInfo->ErrorCode;
  }

 
  // -----------------------------------------------------------
  // Populate the Packet
  // -----------------------------------------------------------
  //
  pOpCode   = (u16  *) (Payload+0);
  pFileName = (char *) (Payload+2);
  pMode     = (char *) (Payload+2 + FilenameLength + 1);
  *pOpCode = htons(OpCode);
  
  memcpy((void *)pFileName, (const void *) pInfo->Filename, FilenameLength);
  memcpy((void *)pMode    , (const void *) pInfo->Mode    , ModeLength);
  
  // -----------------------------------------------------------
  // Send the Packet
  // -----------------------------------------------------------
  //
  if (pInfo->pOptions->DebugTftp){
    xil_printf ("  Sending RRQ to %"U16_F".%"U16_F".%"U16_F".%"U16_F",port %"U16_F"\n\r",
                ip4_addr1_16(&pInfo->ServerIPAddr), ip4_addr2_16(&pInfo->ServerIPAddr),
                ip4_addr3_16(&pInfo->ServerIPAddr), ip4_addr4_16(&pInfo->ServerIPAddr),
                pInfo->Pcb->remote_port);
    
    xil_printf ("  | OpCode   = %d  |  Filename = %s | Mode = %s |\n\r", ntohs(*pOpCode), (char*)pFileName, (char*)pMode);
  }
  
  pInfo->RequestedFile = 1;
  
  // Send RRQ and wait for first DATA.
  // This only returns when the first DATA is received, or all retransmits have been exhausted
  Err = PR_TFTP_SendPacketAndWait(pInfo, Payload, Length, pInfo->LocalNetif);

  // payload has been freed
  //
  if (Err != ERR_OK) {
    xil_printf("%s ERROR: PR_TFTP_RequestFile: Failed to send packet to server\n\r",
               __FILE__);
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_PACKET_FAILED, ERR_OK);
    return pInfo->ErrorCode;
  }
  return PR_TFTP_ERR_OK;
}

// --------------------------------------------------------------------------------------------
// [FUNCTION] Send an ACK packet
// --------------------------------------------------------------------------------------------
//
pr_tftp_err_t PR_TFTP_SendAck(struct pr_tftp_information_s *pInfo){
  void                   *Payload;
  struct pr_tftp_ack_packet_s *pAckPacket;
  err_t                   Err;

  if (pInfo->pOptions->DebugTftp){
    xil_printf("-----------------------\n\r");
    xil_printf("PR_TFTP_SendAck called\n\r");
    xil_printf("-----------------------\n\r");
  }

  Payload = calloc(4, sizeof(char));
  if (Payload == NULL) {
    xil_printf("%s ERROR: PR_TFTP_SendAck: Unable to allocate memory for ACK Payload\n\r",
               __FILE__);
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_CANT_ALLOC_MEM, ERR_OK);

    if (pInfo->pOptions->DebugTftp){
      xil_printf("-------------------------------\n\r");
      xil_printf("PR_TFTP_SendAck called ...done\n\r");
      xil_printf("-------------------------------\n\r");
    }
    return pInfo->ErrorCode;
  }

  pAckPacket          = (struct pr_tftp_ack_packet_s*)Payload;
  pAckPacket->OpCode  = htons(4);  // ACK OpCode
  pAckPacket->BlockID = htons(pInfo->LastBlockId);

  //  Send ACK and wait for DATA if the transfer isn't complete
  Err = PR_TFTP_SendPacketAndWait(pInfo, Payload, 4, pInfo->LocalNetif);
  // This only returns when the first DATA is received, or all retransmits have been exhausted

  if (Err != ERR_OK) {
    xil_printf("%s ERROR: PR_TFTP_SendAck: Unable to send packet to server port %d: Err = %d\n\r", 
               __FILE__, pInfo->Pcb->remote_port, Err);
    PR_TFTP_MarkTransferComplete(pInfo, PR_TFTP_ERR_PACKET_FAILED, ERR_OK);
    
    if (pInfo->pOptions->DebugTftp){
      xil_printf("-------------------------------\n\r");
      xil_printf("PR_TFTP_SendAck called ...done\n\r");
      xil_printf("-------------------------------\n\r");
    }
    return pInfo->ErrorCode;
  }

  if (pInfo->pOptions->DebugTftp){
    xil_printf("-------------------------------\n\r");
    xil_printf("PR_TFTP_SendAck called ...done\n\r");
    xil_printf("-------------------------------\n\r");
  }
  return PR_TFTP_ERR_OK;
}


// --------------------------------------------------------------------------------------------
// [FUNCTION] Fetch a file form the TFTP server.
// --------------------------------------------------------------------------------------------
// The file is buffered in memory if a memory buffer is supplied and/or a callback function
// is called for each received data packet if that is supplied.
// The function returns when the full file has been fetched
//
pr_tftp_err_t PR_TFTP_FetchFile_PRIVATE( // Network information
                                        //
                                        struct netif          *LocalNetif,
                                        struct ip_addr         ServerIPAddr,

                                        // Transfer information
                                        //
                                        char                  *Filepath,
                                        char                  *Mode,
                                        volatile int unsigned *pTimeoutTickCount,
                                        int unsigned           TimeoutThreshold,
                                        int unsigned           TimeoutRetryAttempts,

                                        // Memory buffer information (optional)
                                        // This is a pointer to a pointer as we might reallocate and change the address
                                        char                 **ppDataBuffer,
                                        u32                   *DataBufferSize,
                                        u32                   *DataBufferUsed,

                                        // Callback information (optional)
                                        pr_tftp_recv_data_fn   RecvDataCallback,
                                        void                  *RecvDataCallbackArg,
                                        pr_tftp_options_s     *pOptions) {
  
  struct pr_tftp_information_s TftpTransfer;


  // The pointer needs to be null or else the following code will try and reallocate it and use it.
  // Set the pointer to NULL if the size is 0
  if (*DataBufferSize == 0) {
    *ppDataBuffer = NULL;
  }


  PR_TFTP_InitialiseTransferInfo(&TftpTransfer);
  
  TftpTransfer.ServerIPAddr        =  ServerIPAddr;
  TftpTransfer.LocalNetif           =  LocalNetif;
  TftpTransfer.BufferSize           = *DataBufferSize;
  TftpTransfer.ppBuffer             =  ppDataBuffer;
  TftpTransfer.RecvDataCallback     =  RecvDataCallback;
  TftpTransfer.RecvDataCallbackArg  =  RecvDataCallbackArg;
  TftpTransfer.pTimeoutTickCount    =  pTimeoutTickCount;
  TftpTransfer.TimeoutThreshold      =  TimeoutThreshold;
  TftpTransfer.TimeoutRetryAttempts =  TimeoutRetryAttempts;
 
  strcpy(TftpTransfer.Filename, Filepath);
  strcpy(TftpTransfer.Mode    , Mode);

  if (pOptions != NULL) {
    *TftpTransfer.pOptions = *pOptions;
  }

  // Set up the UDP interface and register a receive callback
  PR_TFTP_SetupConnection(&TftpTransfer);
  
  // Send the RRQ.  The receive callback handles all the data packets
  // This only returns when the first DATA is received, or all retransmits have been exhausted
  
  // If this returns an error, don't proceed with the remainder of the function
  PR_TFTP_RequestFile(&TftpTransfer);
  if (TftpTransfer.Error) {
    PR_TFTR_DeallocateTransferInfo(&TftpTransfer);
    return TftpTransfer.ErrorCode;
  }
  
  // The RRQ has been sent and the receive callback handles all the data packets.  When each receive callback completes, 
  // I need to send an ACK
  
  // To get to this point the RRQ has been sent and the first data received (or failed).  
  while (1) {

    if (TftpTransfer.Complete == 0) {
      // If a packet is received, acknowledge it.  The ACK needs to be acknowledged as well, so this function 
      // waits until the next data is loaded
      // If this returns an error, don't proceed with the remainder of the function
      PR_TFTP_SendAck (&TftpTransfer); //      ---> Sends an ACK and waits for DATA if more is expected
      if (TftpTransfer.Error) {
        PR_TFTR_DeallocateTransferInfo(&TftpTransfer);
        return TftpTransfer.ErrorCode;
      }
    } else {
      if (TftpTransfer.Error == 0) {
        if (TftpTransfer.pOptions->DebugTftp) {
          xil_printf("PR_TFTP_FetchFile_PRIVATE passed\n\r");
          xil_printf("  TftpTransfer.NumberOfBytesWritten = %x\n\r", TftpTransfer.NumberOfBytesWritten);
          xil_printf("  TftpTransfer.BufferSize           = %x\n\r", TftpTransfer.BufferSize);
        }
      } else {
        xil_printf("%s PR_TFTP_FetchFile_PRIVATE failed with error code %d and/or TFTP error code %d\n\r", 
                   __FILE__, TftpTransfer.ErrorCode, TftpTransfer.TftpErrorCode);
      }
      break;
    }
  }

  // Copy back data to the calling function

  if (DataBufferUsed != NULL) {
    *DataBufferUsed = TftpTransfer.NumberOfBytesWritten;
  }

  if (DataBufferSize != NULL) {
    *DataBufferSize = TftpTransfer.BufferSize;
  }

  PR_TFTR_DeallocateTransferInfo(&TftpTransfer);
  return PR_TFTP_ERR_OK;
}


// --------------------------------------------------------------------------------------------
// [FUNCTION] Convert an integer field from the RM Information file to an integer 
// --------------------------------------------------------------------------------------------
//
int PR_TFTP_ConvertStringToUnsignedInt(char * Str, int unsigned * Result) {
  char * ptr;
  
  *Result = (int unsigned)strtoul(Str, &ptr, 0);
  
  // ptr points to the first character after the number.  If it's '\0' then the whole
  // string was the number (good).  If it's not '\0' then the string contained something 
  // that wasn't a number.  This is a fail unless that was whitespace.  
  // NOTE: This won't catch cases like "123 ABCD" because ptr will point at the space.
  // 
  if (*ptr != '\0' && !isspace(*ptr)){
    return -1;
  }

  // Detect a blank string.
  if (ptr == Str){
    return -1;
  }
  return 0;

}



// --------------------------------------------------------------------------------------------
// [FUNCTION] Parse the next line from the RM Info file
// --------------------------------------------------------------------------------------------
//
pr_tftp_err_t PR_TFTP_ParseNextLineFromRMInfoFile(char **ppStartOfBuffer,               // A pointer to a pointer to the file.  This will be updated on   
                                                                                        // return to point to the next line of the file to process        
                                                  u32 *BytesRemainingInBuffer,          // A pointer to the number of bytes left to process.  This will   
                                                                                        // be updated on return                                           
                                                  u16 *RpId,                            // A pointer to the RpId result                                   
                                                  u16 *RmId,                            // A pointer to the RmId result                                    
                                                  u8  *ResetDuration,                   // A pointer to the ResetDuration result                           
                                                  u8  *ResetRequired,                   // A pointer to the ResetRequired result                           
                                                  u8  *StartupRequired,                 // A pointer to the StartupRequired result                         
                                                  u8  *ShutdownRequired,                // A pointer to the ShutdownRequired result                        
#ifdef PR_TFTP_REQUIRES_CLEARING_BITSTREAM
                                                  u32 *ClearingBsSize,                  // A pointer to the Clearing BsSize result
                                                  char **ppClearingFilename,            // A pointer to a buffer containing the filename.  
                                                                                        // Ownership of the memory passes back to the 
                                                                                        // calling function      
#endif
                                                  u32 *BsSize,                          // A pointer to the BsSize result                                  
                                                  char **ppFilename                     // A pointer to a buffer containing the filename.  
                                                                                        // Ownership of the memory passes back to the 
                                                                                        // calling function                  
                                             ){
  
  char *pEndOfBuffer = *ppStartOfBuffer + (*BytesRemainingInBuffer-1);
  char *pNextHashChar;
  char *pNextNewlineChar;
  int unsigned NumBytesLeftInLine;
   
  char *pNextComma; // Will point at the next comma in a line
  char *pstr_Field; // A pointer to a memory buffer that contains the field being processed

  int FieldWidth;
  int NextParsedIntArrayIndex = 0;
  int NextParsedStringArrayIndex = 0;


#ifdef PR_TFTP_REQUIRES_CLEARING_BITSTREAM
  int unsigned ExpectedNumberOfFields = 10;
  char FieldIsString[10] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 1};
  int unsigned ParsedInts [8];
  char         *ParsedStrings [2];
#else
  int unsigned ExpectedNumberOfFields = 8;
  char FieldIsString[10] = {0, 0, 0, 0, 0, 0, 0, 1};
  int unsigned ParsedInts [7];
  char         *ParsedStrings [1];
#endif

  // ParsedInts contains the unsigned integers we have parsed from this line
  // ParsedStrings contains pointers to the strings we have parsed from this line (will be the filenames).
  //               These are allocated when we parse them to ensure we allocate just enough memory
  
  // The line format (7 series) is
  //   RpId, RMID, Reset Duration, Reset Required, Startup Required, Shutdown Required, BS Size, File Name
  // The line format (UltraScale) is
  //   RpId, RMID, Reset Duration, Reset Required, Startup Required, Shutdown Required, BS Size, File Name, Clearing BS Size, Clearing File Name
  //
  // Lines starting with # are comments.
  // Lines cannot be broken


  // Skip comment lines. These start with # and end with \n
  //
  while(1){
    pNextHashChar     = strchr(*ppStartOfBuffer, '#');
    pNextNewlineChar  = strchr(*ppStartOfBuffer, '\n');
  
    if (strchr(*ppStartOfBuffer, ',') == NULL){
      // There's nothing left to do in this file but I can't update the fields.  I have to return an error.  
      xil_printf("ERROR: There are no commas left in this CSV file.\n\r");
      return PR_TFTP_ERR_BAD_FILE;
    }

    // Find the locations of the next comment char and the next end-of-line
    if (pNextNewlineChar == NULL){ pNextNewlineChar = pEndOfBuffer; }
    if (pNextHashChar    == NULL){ pNextHashChar    = pNextNewlineChar + 1; }
    
    if(pNextHashChar < pNextNewlineChar) {
      // It's a comment line so skip it.  Move the pointer to the start of the next line
      *ppStartOfBuffer = (pNextNewlineChar + 1);
    } else {
      break;
    }
  };

  NumBytesLeftInLine = (pNextNewlineChar - *ppStartOfBuffer)+1;
  // Create a field buffer that's big enough to hold any field on this line
  pstr_Field =         (char *)malloc(NumBytesLeftInLine);
  if (pstr_Field == NULL) {
    xil_printf("%s ERROR: Error creating pstr_Field buffer. Out of Memory\n\r", __FILE__);
    return PR_TFTP_ERR_CANT_ALLOC_MEM;
  }
  
  // Now process each field in the line
  for (int i = 0; i< ExpectedNumberOfFields; i++) {
    pNextComma  = strchr(*ppStartOfBuffer, ',');

    if (pNextComma == NULL) {
      if (i != ExpectedNumberOfFields-1) {
        printf("%s ERROR: There weren't enough fields in the CSV file's line.  Expected %d but got %d.\n\r", 
               __FILE__, ExpectedNumberOfFields, i+1);
        return PR_TFTP_ERR_BAD_FILE;
      }
      // Handle the last field which doesn't end with a comman
      strcpy (pstr_Field, *ppStartOfBuffer);
      FieldWidth = strlen(pstr_Field);
    } else {
      FieldWidth = pNextComma - *ppStartOfBuffer;
      strncpy (pstr_Field, *ppStartOfBuffer, FieldWidth);
      *(pstr_Field+FieldWidth) = '\0';
    }

    // Now parse it
    if (FieldIsString[i] == 0) {
      // Parsing an unsigned int field
      if (PR_TFTP_ConvertStringToUnsignedInt(pstr_Field, &ParsedInts[NextParsedIntArrayIndex])) {
        printf("ERROR: Failed to parse field %s\n\r", pstr_Field);
        return PR_TFTP_ERR_BAD_FILE;
      }
      NextParsedIntArrayIndex++;
    } else {
      // Parsing a string field
      ParsedStrings[NextParsedStringArrayIndex] = malloc(FieldWidth+1);
      
      // This strips out leading and trailing whitespace
      sscanf(pstr_Field, "%s", ParsedStrings[NextParsedStringArrayIndex]);
      NextParsedStringArrayIndex++;
    }
    *ppStartOfBuffer = pNextComma+1;
  }
  free(pstr_Field);

  *RpId             = ParsedInts[0];
  *RmId             = ParsedInts[1];
  *ResetDuration    = ParsedInts[2];
  *ResetRequired    = ParsedInts[3];
  *StartupRequired  = ParsedInts[4];
  *ShutdownRequired = ParsedInts[5];
  *BsSize           = ParsedInts[6];
#ifdef PR_TFTP_REQUIRES_CLEARING_BITSTREAM
  *ClearingBsSize   = ParsedInts[7];
#endif


 *ppFilename = ParsedStrings[0];

#ifdef PR_TFTP_REQUIRES_CLEARING_BITSTREAM
  *ppClearingFilename = ParsedStrings[1];
  //
  //  printf ("  Parsed line as RP %d RM %d, ResetDuration = %d, ResetRequired = %x, StartupRequired = %x, ShutdownRequired = %x, Clearing BsSize = %d, Clearing file name = %s, BsSize = %d, file name = %s\n", 
  //          (int) *RpId, 
  //          (int) *RmId, 
  //          (int) *ResetDuration, 
  //          (int) *ResetRequired, 
  //          (int) *StartupRequired, 
  //          (int) *ShutdownRequired,
  //          (int) *ClearingBsSize,
  //          *ppClearingFilename, 
  //          (int) *BsSize,
  //          *ppFilename);
  //
  //#else
  //  printf ("  Parsed line as RP %d RM %d, ResetDuration = %d, ResetRequired = %x, StartupRequired = %x, ShutdownRequired = %x, BsSize = %d, file name = %s\n", 
  //          (int) *RpId, 
  //          (int) *RmId, 
  //          (int) *ResetDuration, 
  //          (int) *ResetRequired, 
  //          (int) *StartupRequired, 
  //          (int) *ShutdownRequired, 
  //          (int) *BsSize, 
  //          *ppFilename);
#endif
  
  *ppStartOfBuffer = pNextNewlineChar+1;
  
  // Was that the last line in the file?
  // 
  if (strchr(*ppStartOfBuffer, ',') == NULL){
    // This was the last line as there are no more "," left in the file
    *BytesRemainingInBuffer = 0;
  } else {
    *BytesRemainingInBuffer = (pEndOfBuffer-*ppStartOfBuffer)+1;
  }
  
  return PR_TFTP_ERR_OK;
}
                              
// --------------------------------------------------------------------------------------------
// [FUNCTION] Adds a null terminator to the RM Info file 
// --------------------------------------------------------------------------------------------
// This file is parsed with sscanf which expects a null terminated string.
//
pr_tftp_err_t PR_TFTP_TerminateRMInfoFile(
                                          char                 **ppDataBuffer,
                                          u32                   *DataBufferSize,
                                          u32                   *DataBufferUsed,
                                          pr_tftp_options_s     *pOptions
                                          ) {
  u32 RequiredBufferSize;
  if (*DataBufferUsed >= *DataBufferSize){
    // The buffer is too small to add a '\0' to.  Reallocate to add one byte
    if (pOptions->ReallocateMemoryIfRequired == 1) {
      // I'm allowed to allocate memory
      
      RequiredBufferSize = *DataBufferSize + 1;
      if (pOptions->DebugMemoryAllocation){
        xil_printf("PR_TFTP_TerminateRMInfoFile: The data buffer was not large enough so reallocating\n\r");
      }

      *ppDataBuffer = realloc(*ppDataBuffer, RequiredBufferSize);

      *DataBufferSize = RequiredBufferSize;
      *DataBufferUsed = RequiredBufferSize;

      if (*ppDataBuffer == NULL) {
        xil_printf("%s ERROR: PR_TFTP_TerminateRMInfoFile: The data buffer was not large enough and a realloc has failed.\n\r",
                   __FILE__);
      }
    } else {
      // I'm not allowed to allocate memory
      xil_printf("%s ERROR: RM Info file uses all of the allocated buffer so there is no space for a null terminator.  Please increase the size of the buffer by 1\n\r",
                 __FILE__);
      return PR_TFTP_ERR_BUF_TOO_SMALL;
    }
  }
  
  // Add a null terminator to make the buffer into a string.
  *(*ppDataBuffer + *DataBufferUsed-1) = '\0';

  return PR_TFTP_ERR_OK;
}
             
// ============================================================================================
// [SECTION] Private Functions: End
// ============================================================================================


// ============================================================================================
// [SECTION] Public Functions to fetch files using TFTP
// ============================================================================================

// --------------------------------------------------------------------------------------------
// [FUNCTION] Fetches the RM Info file from the TFTP server.  
// --------------------------------------------------------------------------------------------
// The file is buffered in memory.
// The function returns when the full file has been fetched
//
pr_tftp_err_t PR_TFTP_FetchRmInfoToMem(struct netif          *LocalNetif,
                                       struct ip_addr         ServerIPAddr,
                                       char                  *Filepath,
                                       char                 **ppDataBuffer,
                                       u32                   *DataBufferSize,
                                       u32                   *DataBufferUsed,
                                       volatile int unsigned *pTimeoutTickCount,
                                       int unsigned           TimeoutThreshold,
                                       int unsigned           TimeoutRetryAttempts,
                                       pr_tftp_options_s     *pOptions) {

  pr_tftp_err_t err;
  
  err = PR_TFTP_FetchFile_PRIVATE(
                                  LocalNetif,
                                  ServerIPAddr,
                                  Filepath,
                                  "netascii",
                                  pTimeoutTickCount,
                                  TimeoutThreshold,
                                  TimeoutRetryAttempts,
                                  ppDataBuffer,
                                  DataBufferSize,
                                  DataBufferUsed,  // This is the address of the variable
                                  NULL,
                                  NULL,
                                  pOptions);

  if (err != PR_TFTP_ERR_OK) {
    return err;
  }
  
  // Null terminate the memory buffer to turn it into a string for sscanf to parse later
  //
  PR_TFTP_TerminateRMInfoFile(ppDataBuffer, DataBufferSize, DataBufferUsed, pOptions);
  return err;
}




// --------------------------------------------------------------------------------------------
// [FUNCTION] Fetches the RM Info file from the TFTP server
// --------------------------------------------------------------------------------------------
// A callback is called for every data packet that is received.
// The function returns when the full file has been fetched
//
// NOTE: This does not null terminate the received file.  The file is parsed using sscanf
// which expects a null terminated string.  
// 
pr_tftp_err_t PR_TFTP_FetchRmInfoToFunction(struct netif          *LocalNetif, 
                                            struct ip_addr         ServerIPAddr, 
                                            char                  *Filepath, 
                                            pr_tftp_recv_data_fn   RecvDataCallback,
                                            void                  *RecvDataCallbackArg,
                                            volatile int unsigned *pTimeoutTickCount,
                                            int unsigned           TimeoutThreshold,
                                            int unsigned           TimeoutRetryAttempts,
                                            pr_tftp_options_s     *pOptions){

        // If we set this to 1 then we will store in memory as well as use the callback, 
        // which we don't want
        pOptions->ReallocateMemoryIfRequired = 0; 

        return PR_TFTP_FetchFile_PRIVATE(
                                        LocalNetif,
                                        ServerIPAddr,
                                        Filepath,
                                        "netascii",
                                        pTimeoutTickCount,
                                        TimeoutThreshold,
                                        TimeoutRetryAttempts,
                                        NULL,
                                        0,
                                        NULL,
                                        RecvDataCallback,
                                        RecvDataCallbackArg,
                                        pOptions);
}



// --------------------------------------------------------------------------------------------
// [FUNCTION] Fetches a partial bitstream file from the TFTP server.  
// --------------------------------------------------------------------------------------------
// The file is buffered in memory.
// The function returns when the full file has been fetched
//
pr_tftp_err_t PR_TFTP_FetchPartialToMem( struct netif                  *LocalNetif,
                                         struct ip_addr         ServerIPAddr,
                                         char                  *Filepath,
                                         char                 **ppDataBuffer,
                                         u32                   *DataBufferSize,
                                         u32                   *DataBufferUsed,
                                         volatile int unsigned *pTimeoutTickCount,
                                         int unsigned           TimeoutThreshold,
                                         int unsigned           TimeoutRetryAttempts,
                                         pr_tftp_options_s       *pOptions){
  
  return PR_TFTP_FetchFile_PRIVATE(LocalNetif,
                                         ServerIPAddr,
                                         Filepath,
                                         "octet",
                                         pTimeoutTickCount,
                                         TimeoutThreshold,
                                         TimeoutRetryAttempts,
                                         ppDataBuffer,
                                         DataBufferSize,
                                         DataBufferUsed,  
                                         NULL,
                                         NULL,
                                         pOptions);
}


// --------------------------------------------------------------------------------------------
// [FUNCTION] Fetches a partial bitstream file from the TFTP server
// --------------------------------------------------------------------------------------------
// A callback is called for every data packet that is received.
// The function returns when the full file has been fetched
//
pr_tftp_err_t PR_TFTP_FetchPartialToFunction(struct netif            *LocalNetif,
                                               struct ip_addr         ServerIPAddr, 
                                               char                  *Filepath, 
                                               pr_tftp_recv_data_fn   RecvDataCallback, 
                                               void                  *RecvDataCallbackArg,
                                               volatile int unsigned *pTimeoutTickCount,
                                               int unsigned           TimeoutThreshold,
                                               int unsigned           TimeoutRetryAttempts,
                                               pr_tftp_options_s     *pOptions){

        // If we set this to 1 then we will store in memory as well as use the callback, 
        // which we don't want
        pOptions->ReallocateMemoryIfRequired = 0; 
        return PR_TFTP_FetchFile_PRIVATE(
                                         LocalNetif,
                                         ServerIPAddr,
                                         Filepath,
                                         "octet",
                                         pTimeoutTickCount,
                                         TimeoutThreshold,
                                         TimeoutRetryAttempts,
                                         NULL,
                                         0,
                                         NULL,
                                         RecvDataCallback,
                                         RecvDataCallbackArg,
                                         pOptions);
}

// ============================================================================================
// [SECTION] Public Functions to fetch files using TFTP: End
// ============================================================================================


// ============================================================================================
// [SECTION] Public Functions to parse files using TFTP
// ============================================================================================


// ============================================================================================
// [SECTION] Public Functions to parse files using TFTP: End
// ============================================================================================


// ============================================================================================
// [SECTION] Public Functions to work with the data structure
// ============================================================================================

// --------------------------------------------------------------------------------------------
// [FUNCTION] Create the data structure
// --------------------------------------------------------------------------------------------
//
// The RP entries are created.  As we don't know how many RMs each will have, these are left
// unallocated at this stage.
//
// pr_tftp_rp_s is allocated in this proc.  Ownership of it returns to the calling function
// which should  eventually free it

pr_tftp_err_t PR_TFTP_CreateDataStructure(u16 NumRPs, pr_tftp_rp_s **pRPInfo){

  if (NumRPs <= 0) {
    return PR_TFTP_ERR_BAD_PARAM;
  }

  *pRPInfo = ( pr_tftp_rp_s *) calloc(NumRPs, sizeof(pr_tftp_rp_s));
  if (*pRPInfo == NULL) {
    xil_printf("%s:  Unable to allocate memory for data structure\n\r", __FILE__ );
    return PR_TFTP_ERR_CANT_ALLOC_MEM;
  }
  for (int i = 0; i < NumRPs; i++){
    (*pRPInfo+i)->Id = i;
    (*pRPInfo+i)->NumberOfRMs = 0;
    (*pRPInfo+i)->pRMInfos = NULL;
    (*pRPInfo+i)->ActiveRM = -1;
  }

  return PR_TFTP_ERR_OK;
}



// --------------------------------------------------------------------------------------------
// [FUNCTION] Free the data structure
// --------------------------------------------------------------------------------------------
//
pr_tftp_err_t PR_TFTP_FreeDataStructure(u16 NumRPs, pr_tftp_rp_s **pRPInfoArray){

  if (pRPInfoArray == NULL) {
    return PR_TFTP_ERR_OK;
  }

  for (int RpId = 0; RpId < NumRPs; RpId++){
    for (int RmId = 0; RmId < (*pRPInfoArray+RpId)->NumberOfRMs; RmId++){
      
      // Free the filename buffer
      free((*pRPInfoArray+RpId)->pRMInfos[RmId]->pFileName);
      
      // and the RM information struct
      free((*pRPInfoArray+RpId)->pRMInfos[RmId]);
    }
    
    // Free the (now invalid) array holding RM info structs
    if ((*pRPInfoArray+RpId)->NumberOfRMs > 0) {
      xil_printf("  Freeing RMInfo array\n\r");
      free((*pRPInfoArray+RpId)->pRMInfos);
    }
  }

  free(*pRPInfoArray);
  *pRPInfoArray = NULL;

  return PR_TFTP_ERR_OK;
}


// --------------------------------------------------------------------------------------------
// [FUNCTION] Print the data structure
// --------------------------------------------------------------------------------------------
//
pr_tftp_err_t PR_TFTP_PrintDataStructure(u16 NumRPs, pr_tftp_rp_s *pRPInfoArray){
  pr_tftp_rp_s * pRP;
  pr_tftp_rm_s * pRM;

  if (pRPInfoArray == NULL) {
    return PR_TFTP_ERR_OK;
  }

  for (int RpId = 0; RpId < NumRPs; RpId++){
    pRP = pRPInfoArray+RpId;
    xil_printf("RP index %0d\r\n", RpId);
    xil_printf("  ID %0d | NumberOfRMs = %0d | ActiveRM = %0d\r\n", pRP->Id, pRP->NumberOfRMs, pRP->ActiveRM);
    
    for (int RmId = 0; RmId < pRP->NumberOfRMs; RmId++){
      pRM = PR_TFTP_GetRMInfoByIndex(pRP, RmId);
      xil_printf("  RM index %0d\r\n", RmId);
      xil_printf("    ID               = %0d\r\n", pRM->Id);
      xil_printf("    ShutdownRequired = %0d\r\n", pRM->ShutdownRequired);
      xil_printf("    StartupRequired  = %0d\r\n", pRM->StartupRequired);
      xil_printf("    ResetRequired    = %0d\n\r", pRM->ResetRequired);
      xil_printf("    ResetDuration    = %0d\n\r", pRM->ResetDuration);
      xil_printf("    BsSize           = %0d\n\r", pRM->BsSize);
      xil_printf("    FileName         = %s \n\r", pRM->pFileName);
#ifdef PR_TFTP_REQUIRES_CLEARING_BITSTREAM
      xil_printf("    ClearingBsSize   = %0d\n\r", pRM->ClearingBsSize);
      xil_printf("    ClearingFileName = %s \n\r", pRM->pClearingFileName);

#endif

    }
    
  }
  return PR_TFTP_ERR_OK;

}


// --------------------------------------------------------------------------------------------
// [FUNCTION] Return a pointer to the RPInfo struct for index or null if it doesn't exist
// --------------------------------------------------------------------------------------------
//
// Use this version if the RPs are stored in order in the array.  For example, 
// RPInfo+0 = RP ID 0 
// RPInfo+1 = RP ID 1 
//
// This is faster than searching for the correct RP ID

//
pr_tftp_rp_s * PR_TFTP_GetRPInfoByIndex(pr_tftp_rp_s *RPInfoArray, u16 NumRPs, u16 Index){
  if (Index >= NumRPs){
    xil_printf("%s ERROR: PR_TFTP_GetRPInfoByIndex called with an index (%0d) larger than the number of RP (%0d)\n\r", __FILE__, Index, NumRPs );
    return NULL;
  }
  return (RPInfoArray+Index);
}


// --------------------------------------------------------------------------------------------
// [FUNCTION] Return a pointer to the RPInfo struct for RpId or null if it doesn't exist
// --------------------------------------------------------------------------------------------
//
// Use this version if the RPs are not stored in order in the array.  For example, 
// RPInfo+0 = RP ID 1 
// RPInfo+1 = RP ID 0 
//
// This is slower than just looking up the array index
//
pr_tftp_rp_s * PR_TFTP_GetRPInfoByID(pr_tftp_rp_s *RPInfo, u16 NumRPs, u16 RpId){
  pr_tftp_rp_s * p;
  for (int i = 0; i < NumRPs; i++) {
    p = RPInfo+i;
    if (p->Id == RpId) {
      return p;
    }
  }
  return NULL;
}


// --------------------------------------------------------------------------------------------
// [FUNCTION] Return a pointer to the RMInfo struct for RmId or null if it doesn't exist
// --------------------------------------------------------------------------------------------
// Note that this takes a single RP Info struct as a parameter, not an array of them
//
// Use this version if the RMs are not stored in order in the array.  For example, 
// pRPInfo->pRMInfos+0 = RM ID 1 
// pRPInfo->pRMInfos+1 = RM ID 0 
//
// This is slower than just looking up the array index
//
pr_tftp_rm_s * PR_TFTP_GetRMInfoByID(pr_tftp_rp_s *pRPInfo, u16 RmId){
  if (pRPInfo == NULL) {
    xil_printf("%s ERROR: PR_TFTP_GetRMInfoByID called will null RPInfo\n\r", __FILE__ );
    return NULL;
  }
  
  pr_tftp_rm_s * p;
  for (int i = 0; i < pRPInfo->NumberOfRMs; i++) {
    p = *(pRPInfo->pRMInfos+i);
    if (p->Id == RmId) {
      return p;
    }
  }

  return NULL;
}



// --------------------------------------------------------------------------------------------
// [FUNCTION] Return a pointer to the RMInfo struct for index or null if it doesn't exist
// --------------------------------------------------------------------------------------------
// Note that this takes a single RP Info struct as a parameter, not an array of them
//
// Use this version if the RMs are stored in order in the array.  For example, 
// pRPInfo->pRMInfos+0 = RM ID 0 
// pRPInfo->pRMInfos+1 = RM ID 1 
//
// This is faster than searching for the correct RM ID

pr_tftp_rm_s * PR_TFTP_GetRMInfoByIndex(pr_tftp_rp_s *pRPInfo, u16 Index){
  if (pRPInfo == NULL) {
    xil_printf("%s: ERROR PR_TFTP_GetRMInfoByIndex called will null RPInfo\n\r", __FILE__);
    return NULL;
  }

  if (Index >= pRPInfo->NumberOfRMs){
    xil_printf("%s: ERROR PR_TFTP_GetRMInfoByIndex called with an index (%0d) larger than the number of RMs (%0d)\n\r", __FILE__, Index, pRPInfo->NumberOfRMs );
    return NULL;
  }
  
  return *(pRPInfo->pRMInfos+Index);
}

// --------------------------------------------------------------------------------------------
// [FUNCTION] Create a new RM entry in a RP entry
// --------------------------------------------------------------------------------------------
// This takes a single RP struct, not an array of RP structs and an index.
// Returns a pointer to the new RM object through a parameter.
// If the RM struct already exists, it is returned as-is.  It is not reallocated or reinitialised.
//
pr_tftp_err_t PR_TFTP_CreateRMStructure(pr_tftp_rp_s *pRPInfo, u16 RmId, pr_tftp_rm_s ** pRM){
  if (pRPInfo == NULL) {
    xil_printf("%s: PR_TFTP_CreateRMStructure called with NULL RP Data Structure\n\r", __FILE__ );
    return PR_TFTP_ERR_BAD_PARAM;
  }

  // Does an RM struct with this RM ID already exist in the RP Struct?  If so, don't create anything
  // but just return that.
  //
  *pRM = PR_TFTP_GetRMInfoByID(pRPInfo, RmId);
  if (*pRM != NULL) {
    return PR_TFTP_ERR_OK;
  }

  // Create the RM Struct
  *pRM = calloc(1, sizeof(pr_tftp_rm_s));
  if (*pRM == NULL) {
    xil_printf("%s: PR_TFTP_CreateRMStructure Unable to allocate memory for data structure\n\r", __FILE__ );
    return PR_TFTP_ERR_CANT_ALLOC_MEM;
  }

  
  (*pRM)->Id               = RmId;
  (*pRM)->ResetDuration    = 0;
  (*pRM)->ResetRequired    = 0;
  (*pRM)->StartupRequired  = 0;
  (*pRM)->ShutdownRequired = 0;
  (*pRM)->BsIndex          = 0;
  (*pRM)->BsSize           = 0; 
  (*pRM)->pFileName        = NULL; 
#ifdef PR_TFTP_REQUIRES_CLEARING_BITSTREAM
  (*pRM)->ClearingBsIndex          = 0;
  (*pRM)->ClearingBsSize           = 0; 
  (*pRM)->pClearingFileName        = NULL; 
#endif

  
  pRPInfo->NumberOfRMs++;
          
  pRPInfo->pRMInfos = realloc(pRPInfo->pRMInfos, sizeof(pr_tftp_rm_s) * pRPInfo->NumberOfRMs);
  *(pRPInfo->pRMInfos + (pRPInfo->NumberOfRMs-1)) = *pRM;

  return PR_TFTP_ERR_OK;
}



// --------------------------------------------------------------------------------------------
// [FUNCTION] Create and populate the data structure from an RM Info CSV file
// --------------------------------------------------------------------------------------------
//
// pRPInfoArray is created and populated here.  Ownership of the memory passes back to the 
// calling function which should eventually free it

pr_tftp_err_t PR_TFTP_InitialiseDataStructureFromRmInfoFile(char          *pRmInfoFile, // A pointer to the file buffer
                                                            u32            FileSize,    // The size of the file
                                                            u16            NumRPs,      // The number of RPs 
                                                            pr_tftp_rp_s **pRPInfoArray // The data structure
                                                            ){
  pr_tftp_err_t err;

  char * pCurrentStartOfBuffer = pRmInfoFile;
  u32   BytesRemainingInBuffer = FileSize;
  pr_tftp_rp_s *pRPInfo;
  pr_tftp_rm_s *pRMInfo;
  
  u16 RpId;
  u16 RmId;
  u8  ResetDuration;
  u8  ResetRequired;
  u8  StartupRequired;
  u8  ShutdownRequired;
  u32 BsSize;
 
  char * pFileName = NULL;  // A pointer to the partial bitstream's file name.  This is allocated by the
  // PR_Get* functions used below, but this function owns the memory so has to free it.

#ifdef PR_TFTP_REQUIRES_CLEARING_BITSTREAM
  u32 ClearingBsSize;
  char * pClearingFileName = NULL;  
#endif

  
 
  if ( PR_TFTP_ERR_OK != PR_TFTP_CreateDataStructure(NumRPs, pRPInfoArray)){
    xil_printf("%s ERROR: Failed to allocate memory for PR TFTP Data Structure", __FILE__ );
    return -1;
  }
  
  // ------------------------------------------------------------------------
  // Loop round the RM Information CSV file and process it one line at a time
  // ------------------------------------------------------------------------
  // This is the most efficient way of handling this file as it only needs to be parsed once.  
  // Relevant data is extracted and stored for later
  //
  while (BytesRemainingInBuffer>0){
    err = PR_TFTP_ParseNextLineFromRMInfoFile(&pCurrentStartOfBuffer,  // A pointer to a pointer to the file.  This will be updated on
                                         // return to point to the next line of the file to process
                                         &BytesRemainingInBuffer, // A pointer to the number of bytes left to process.  This will
                                         // be updated on return
                                         &RpId,                  // A pointer to the RpId result
                                         &RmId,                   // A pointer to the RmId result
                                         &ResetDuration,          // A pointer to the ResetDuration result
                                         &ResetRequired,          // A pointer to the ResetRequired result
                                         &StartupRequired,        // A pointer to the StartupRequired result
                                         &ShutdownRequired,       // A pointer to the ShutdownRequired result
#ifdef PR_TFTP_REQUIRES_CLEARING_BITSTREAM
                                         &ClearingBsSize,         // A pointer to the BsSize result
                                         &pClearingFileName,       // A pointer to a buffer containing the filename.  Ownership of the
#endif 
                                         &BsSize,                 // A pointer to the BsSize result
                                         &pFileName               // A pointer to a buffer containing the filename.  Ownership of the
                                                                  // memory passes back to the calling function
                                         );
    if ( err != PR_TFTP_ERR_OK){
      xil_printf("%s : ERROR Processing RM Info file", __FILE__);
      return err;
    }

  
     if (RpId < NumRPs) {
       
       pRPInfo = PR_TFTP_GetRPInfoByIndex(*pRPInfoArray, NumRPs, RpId);
       
       if (pRPInfo == NULL) {
         xil_printf("%s ERROR: Cannot retrieve information for RP %0d\n\r", __FILE__, RpId );
         return -1;
       }
       
       pRMInfo = PR_TFTP_GetRMInfoByID(pRPInfo, RmId);
       if (pRMInfo == NULL) {
         // If it's NULL then this is the first time we have seen anything for this RM.
         // Create a struct instance for it and then initialise it
         //
         if ( PR_TFTP_ERR_OK != PR_TFTP_CreateRMStructure(pRPInfo, RmId, &pRMInfo)){
           xil_printf("%s ERROR: Failed to allocate memory for PR TFTP RM Data Structure", __FILE__ );
           return -1;
         }
         
         pRMInfo->ResetDuration    = ResetDuration;
         pRMInfo->ResetRequired    = ResetRequired;
         pRMInfo->StartupRequired  = StartupRequired;
         pRMInfo->ShutdownRequired = ShutdownRequired;
         
         pRMInfo->BsSize           = BsSize; 
         pRMInfo->pFileName        = pFileName; 

#ifdef PR_TFTP_REQUIRES_CLEARING_BITSTREAM
         // Using the RM ID * 2 as the BS Index is easiest unless you are doing something exotic 
         pRMInfo->BsIndex          = RmId*2;
         pRMInfo->ClearingBsIndex  = pRMInfo->BsIndex+1;

         pRMInfo->ClearingBsSize   = ClearingBsSize; 
         pRMInfo->pClearingFileName= pClearingFileName; 
#else
         // Using the RM ID as the BS Index is easiest unless you are doing something exotic in 7 series.
         pRMInfo->BsIndex          = RmId;

#endif
       } 
     } else {
       xil_printf("%s ERROR: Processing RM Info file.  RP ID %0d has been used but the data structure only has %0d RPs", __FILE__, RpId, NumRPs);
       return -1;
     }
  } // End of while loop looping round RM Info file
  return PR_TFTP_ERR_OK;
}

// ============================================================================================
// [SECTION] Miscellaneous Public Functions 
// ============================================================================================

// --------------------------------------------------------------------------------------------
// [FUNCTION] Get the library version
// --------------------------------------------------------------------------------------------
//
int unsigned PR_TFTP_GetVersion(){
  return 1;
}


