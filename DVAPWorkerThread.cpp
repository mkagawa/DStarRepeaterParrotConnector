/*
 *  Copyright (C) 2018 by Masahito Kagawa NW6UP <mkagawa@hotmail.com>
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy 
 *  of this software and associated documentation files (the "Software"),  to deal
 *  in the Software without restriction, including without limitation the rights 
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is furnished to do so,
 *  subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */
//
// this is a DVAP simulator
//
// the program provides two virtual serial ports, which is
// acting as DVAP board serial port

#include "RepeaterConnector.h"
#include "DVAPWorkerThread.h"
#include "Const.h"

CDVAPWorkerThread::~CDVAPWorkerThread()
{
  delete m_pMutexSerialWrite;
}

CDVAPWorkerThread::CDVAPWorkerThread(char siteId, unsigned int portNumber,wxString appName)
 :CBaseWorkerThread(siteId, portNumber, appName),
  m_lastAckTimeStamp(wxGetUTCTimeMillis()),
  m_lastStatusSentTimeStamp(wxGetUTCTimeMillis())
{
  m_pMutexSerialWrite = new wxMutex();
  srand(time(NULL));
  //Create TxWorker thread
  m_pTxWorker = new CTxWorkerThread(this, m_siteId, m_pMutexSerialWrite);
}

int CDVAPWorkerThread::ProcessData() {

  //Send current status to the host in every 250ms (at least)
  if(m_bStarted && wxGetUTCTimeMillis() - m_lastStatusSentTimeStamp > 250) {
    ::memcpy(m_wbuffer,DVAP_STATUS,DVAP_STATUS_LEN);
    //[07][20] [90][00] [B5] [01][7F] //-75dBm squelch open
    //[07][20] [90][00] [9C] [00][7F] //-100dBm squelch closed
    //[07][20] [90][00] [00] [00][12] //Transmitting Tx fifo can except up to 18 new packets
    //7f == Indicates Queue is empty and Tx operation will soon terminate.
    m_wbuffer[4] = 0xb5;
    m_wbuffer[5] = 0x00;
    m_wbuffer[6] = 0x7f;
    m_pMutexSerialWrite->Lock();
    ::write(m_fd, m_wbuffer, DVAP_STATUS_LEN);
    m_pMutexSerialWrite->Unlock();
    m_lastStatusSentTimeStamp = wxGetUTCTimeMillis();
  }

  //Set read timeout 250ms
  struct timeval timeout;
  fd_set set;
  FD_ZERO(&set);
  FD_SET(m_fd, &set);
  timeout.tv_sec = 0;
  timeout.tv_usec = 250000;
  int rv = ::select(m_fd + 1, &set, NULL, NULL, &timeout);
  if(rv == -1) {
    wxLogMessage(wxT("select received error: %d"), (int)errno);
    return -1; //error
  } else if(rv == 0) {
    if(m_bStarted && wxGetUTCTimeMillis() - m_lastAckTimeStamp > 3000) {
      m_bTxToHost = false;
      m_bStarted = false;
      m_packetSerialNo = 0U;
      m_curRxSessionId = 0U;
      wxLogMessage("No ack from the host. may be repeater process stopped.");
      return -1;
    }
    return 0; //timeout
  }

  //Read from the host
  size_t len = ::read(m_fd, m_buffer, DVAP_HEADER_LENGTH);
  if(len == -1) {
    wxLogError(wxT("serial read error, err: %d"), (int)errno);
    return 0; //error
  }

  //data must be more than 2 bytes
  if(len < 2) {
    wxLogError(wxT("serial read got 1 byte"));
    return -1;
  }

  //calculate data length
  size_t data_len = m_buffer[0] + 256 * (m_buffer[1] & 0x1F);
  char type = (m_buffer[1] & 0xe0) >> 5;
  m_lastReceivedFromHostTimeStamp = wxGetUTCTimeMillis();

  //the data_len should not be longer than DVAP_HEADER_LEN
  //if so, data may be garbled. ignore this series
  if(data_len > sizeof(m_buffer)) {
    return -1;
  }

  while(data_len > len) {
    //each packet must be received within 1s (HOST_TIMEOUT) from the previous one from the host
    //otherwise discard this series
    if(wxGetUTCTimeMillis() - m_lastReceivedFromHostTimeStamp > HOST_TIMEOUT) {
      wxLogMessage("Host data timeout");
      return -1;
    }
    size_t temp_len = ::read(m_fd, &m_buffer[len], data_len - len);
    if(temp_len > 0) {
      len += temp_len;
      m_lastReceivedFromHostTimeStamp = wxGetUTCTimeMillis();
    }
  }

  m_lastStatusSentTimeStamp = 0UL;
  return _ProcessMessage(data_len);
}

int CDVAPWorkerThread::_ProcessMessage(size_t data_len) {
  if(::memcmp(m_buffer,DVAP_GMSK_DATA,2) == 0) {
    if(!m_bStarted) {
      wxLogInfo("GMSK received before system starts. Ignoring");
      return 0;
    }
    int diff = (uint)m_buffer[5] - (uint)m_packetSerialNo;
    //Detect closing packet pattern
    bool bClosingPacket = false;
    if(data_len > 12 && ::memcmp(DVAP_GMSK_DATA,m_buffer,2) == 0 && ::memcmp(GMSK_END, &m_buffer[6], 6) == 0) {
      bClosingPacket = true;
    }

    if(m_curRxSessionId != 0) {
      if(diff <= 1 || diff == -255) {
        m_packetSerialNo = m_buffer[5];
      } else {
        //just log it and go forward
        wxLogMessage(wxT("the packet is out of order, diff:%d"), diff);
      }
      m_iRxPacketCnt++;
      //Detect garbage packts (less than 10 counts), won't forward
      if(bClosingPacket && m_iRxPacketCnt < 10) {
        for(int i = 0; i < m_arrHeaderPacket.GetCount(); i++ ) {
          wxLogMessage(wxT("Frame size is too small (%d), won't be forwarded"), (uint)m_iRxPacketCnt);
          m_arrHeaderPacket[i]->UpdatePacketType( packetType::HEADER_NOSEND );
        }
      }
      SendToInstance(m_buffer, data_len, bClosingPacket ? packetType::CLOSING : packetType::DATA);
    }

    if(bClosingPacket) {
      m_curRxSessionId = 0;
    }
    return 1;

  } else if(::memcmp(m_buffer,DVAP_ACK,DVAP_ACK_LEN)==0) {
    wxLogInfo(wxT("DVAP_ACK"));
    m_lastAckTimeStamp = wxGetUTCTimeMillis();
    return 1; 

  } else if(::memcmp(m_buffer,DVAP_HEADER,2)==0) {
    ::memcpy(m_wbuffer, m_buffer, DVAP_HEADER_LEN);
    //CalcCRC(&m_wbuffer[6], DVAP_HEADER_LEN-6);
    if(!m_bStarted) {
      wxLogInfo("HEADER received before system starts. Ignoring");
      return 0;
    }

    //For logging purpose
    wxString cs,r1,r2,my,sx;
    char buffer[9];
    buffer[8] = 0U;
    ::memcpy(buffer, &m_buffer[9],  8); r2 = wxString::FromAscii(buffer);
    ::memcpy(buffer, &m_buffer[17], 8); r1 = wxString::FromAscii(buffer);
    ::memcpy(buffer, &m_buffer[25], 8); cs = wxString::FromAscii(buffer);
    ::memcpy(buffer, &m_buffer[33], 8); my = wxString::FromAscii(buffer);
    ::memcpy(buffer, &m_buffer[41], 4); buffer[4] = 0U; sx = wxString::FromAscii(buffer);

    //Store my local dstar repeater info
    m_curCallSign = my;
    m_curSuffix = sx;
    m_curRxSessionId = (ulong)rand();
    while(m_curRxSessionId==0) {
      m_curRxSessionId = (ulong)rand();
    }
    wxLogMessage(wxT("Headr: Sess:%X to: %s, r2: %s, r1: %s, my: %s/%s"), 
          (uint)(m_curRxSessionId % 0xFFFF), cs, r2, r1, my, sx);
    m_packetSerialNo = 0U;

    //write back to the host
    m_wbuffer[1] = 0x60U;
    m_pMutexSerialWrite->Lock();
    ::write(m_fd, m_wbuffer, DVAP_RESP_HEADER_LEN);
    m_pMutexSerialWrite->Unlock();

    //check if sender callsign is not myNode call sign
    if(m_curCallSign.StartsWith(" ") || 
       ::memcmp(m_myNodeCallSign.c_str(),m_curCallSign.c_str(),7)==0 && ((char)m_curCallSign[7]==m_siteId ||  sx == "INFO")) {
      wxLogMessage("This message is sent by repeater. won't be forwarded");
      m_curRxSessionId = 0U;
      return 1;
    }
    if(cs.EndsWith("L") || cs == wxT("       U")) {
      wxLogMessage("This message is repeater command. ignoring.");
      m_curRxSessionId = 0U;
      return 1;
    }
    //if(m_curTxSessionId != 0) {
    //  wxLogMessage("other side is being sent. ignoring.");
    //  m_curRxSessionId = 0U;
    //  return 1;
    //}

    //restore the seoncd byte
    m_wbuffer[1] = 0xa0U;
    //empty G1/G2 value, and force CQCQCQ to To field
    ::memcpy(&m_wbuffer[25], "CQCQCQ  ", 8);
    ::memcpy(&m_wbuffer[9],  "                ", 16);
    //CalcCRC(&m_wbuffer[6], DVAP_HEADER_LEN-6);
    //Dummy CRC
    m_wbuffer[DVAP_HEADER_LEN-2] = 0x00;
    m_wbuffer[DVAP_HEADER_LEN-1] = 0x0b;
    m_iRxPacketCnt = 0;
    m_arrHeaderPacket = SendToInstance(m_wbuffer, DVAP_HEADER_LEN, packetType::HEADER);
    return 1;

  } else if(::memcmp(m_buffer,DVAP_REQ_NAME,DVAP_REQ_NAME_LEN)==0) {
    //treat as reset signal
    wxLogInfo(wxT("DVAP_REQ_NAME"));
    ::memcpy(m_wbuffer,DVAP_RESP_NAME,DVAP_RESP_NAME_LEN);
    m_pMutexSerialWrite->Lock();
    ::write(m_fd, m_wbuffer, DVAP_RESP_NAME_LEN);
    m_pMutexSerialWrite->Unlock();
    m_bTxToHost = false;
    m_bStarted = false;
    return 1;
 
  } else if(::memcmp(m_buffer,DVAP_REQ_SERIAL,DVAP_REQ_SERIAL_LEN)==0) {
    wxLogInfo(wxT("DVAP_REQ_SERIAL"));
    ::memcpy(m_wbuffer,DVAP_RESP_SERIAL,DVAP_RESP_SERIAL_LEN);
    ::memcpy(&m_wbuffer[4], "MK123456", 9); //including 0x00
    ::write(m_fd, m_wbuffer, DVAP_RESP_SERIAL_LEN+8);
    return 1;

  } else if(::memcmp(m_buffer,DVAP_REQ_FIRMWARE,DVAP_REQ_FIRMWARE_LEN)==0) {
    wxLogInfo(wxT("DVAP_REQ_FIRMWARE"));
    ::memcpy(m_wbuffer,DVAP_RESP_FIRMWARE,DVAP_RESP_FIRMWARE_LEN);
    ::write(m_fd, m_wbuffer, DVAP_RESP_FIRMWARE_LEN);
    return 1;

  } else if(::memcmp(m_buffer,DVAP_REQ_MODULATION,DVAP_REQ_MODULATION_LEN)==0) {
    wxLogInfo(wxT("DVAP_REQ_MODULATION"));
    ::memcpy(m_wbuffer,DVAP_RESP_MODULATION,DVAP_RESP_MODULATION_LEN);
    ::write(m_fd, m_wbuffer, DVAP_RESP_MODULATION_LEN);
    return 1;

  } else if(::memcmp(m_buffer,DVAP_REQ_MODE,DVAP_REQ_MODE_LEN)==0) {
    wxLogInfo(wxT("DVAP_REQ_MODE"));
    ::memcpy(m_wbuffer,DVAP_RESP_MODE,DVAP_RESP_MODE_LEN);
    ::write(m_fd, m_wbuffer, DVAP_RESP_MODE_LEN);
    return 1;

  } else if(::memcmp(m_buffer,DVAP_REQ_POWER,DVAP_REQ_POWER_LEN-2)==0) {
    wxLogInfo(wxT("DVAP_REQ_POWER"));
    ::memcpy(m_wbuffer,DVAP_RESP_POWER,DVAP_RESP_POWER_LEN);
    ::write(m_fd, m_wbuffer, DVAP_RESP_POWER_LEN);
    return 1;

  } else if(::memcmp(m_buffer,DVAP_REQ_SQUELCH,DVAP_REQ_SQUELCH_LEN-2)==0) {
    wxLogInfo(wxT("DVAP_REQ_SQUELCH"));
    ::memcpy(m_wbuffer,DVAP_RESP_SQUELCH,DVAP_RESP_SQUELCH_LEN);
    ::write(m_fd, m_wbuffer, DVAP_RESP_SQUELCH_LEN);
    return 1;

  } else if(::memcmp(m_buffer,DVAP_REQ_FREQUENCY,DVAP_REQ_FREQUENCY_LEN-4)==0) {
    wxLogInfo(wxT("DVAP_REQ_FREQUENCY"));
    ::memcpy(m_wbuffer,DVAP_RESP_FREQUENCY,DVAP_RESP_FREQUENCY_LEN);
    ::write(m_fd, m_wbuffer, DVAP_RESP_FREQUENCY_LEN);
    return 1;

  } else if(::memcmp(m_buffer,DVAP_REQ_START,DVAP_REQ_START_LEN)==0) {
    wxLogInfo(wxT("DVAP_REQ_START"));
    ::memcpy(m_wbuffer,DVAP_RESP_START,DVAP_RESP_START_LEN);
    m_pMutexSerialWrite->Lock();
    ::write(m_fd, m_wbuffer, DVAP_RESP_START_LEN);
    m_pMutexSerialWrite->Unlock();
    m_lastAckTimeStamp = wxGetUTCTimeMillis();
    m_bStarted = true;
    m_bTxToHost = false;
    return 1;

  } else if(::memcmp(m_buffer,DVAP_REQ_STOP,DVAP_REQ_STOP_LEN)==0) {
    wxLogInfo(wxT("DVAP_REQ_STOP"));
    ::memcpy(m_wbuffer,DVAP_RESP_STOP,DVAP_RESP_STOP_LEN);
    m_pMutexSerialWrite->Lock();
    ::write(m_fd, m_wbuffer, DVAP_RESP_STOP_LEN);
    m_pMutexSerialWrite->Unlock();
    m_bStarted = false;
    m_bTxToHost = false;
    return 1;

  } else if(::memcmp(m_buffer,DVAP_REQ_FREQLIMITS,DVAP_REQ_FREQLIMITS_LEN)==0) {
    wxLogInfo(wxT("DVAP_REQ_FREQLIMITS"));
    ::memcpy(m_wbuffer,DVAP_RESP_FREQLIMITS,DVAP_RESP_FREQLIMITS_LEN);
    long low = 144000000L;
    long high = 145999999L;
    ::memcpy(&m_wbuffer[4], &low, 4);
    ::memcpy(&m_wbuffer[8], &high, 4);
    ::write(m_fd, m_wbuffer, DVAP_RESP_FREQLIMITS_LEN+8);
    return 1;

  } else {
    dumper("Other", m_buffer, data_len);
    return 1;

  }
}

CDVAPWorkerThread::ExitCode CDVAPWorkerThread::Entry() {
  cout << "CDVAPWorkerThread::Entry " << endl;
  auto e = m_pTxWorker->Create();
  if(e != wxThreadError::wxTHREAD_NO_ERROR) {
    delete m_pTxWorker;
    m_pTxWorker = NULL;
    wxLogMessage(wxT("Error in creating Tx thread: %d"), e);
    return 0; 
  }
  m_pTxWorker->SetPriority(100);
  m_pTxWorker->Run();

  //Main Loop
  while(!TestDestroy()){
    int ret = ProcessData();
    ::memset(m_buffer, 0, 10);
  }

  //request TxWorkerThread for exit
  m_pTxWorker->Stop();
  m_pTxWorker->Delete();
  while(m_pTxWorker->IsRunning()) {
    wxMilliSleep(100);
  }
  delete m_pTxWorker;
  m_pTxWorker = NULL;
}

void CDVAPWorkerThread::PostData(CTxData* data) {
  ((CTxWorkerThread*)m_pTxWorker)->PostData(data);
}
