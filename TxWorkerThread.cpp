/*
 *  Copyright (C) 2017 by Masahito Kagawa NW6UP <mkagawa@hotmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "TxWorkerThread.h"
#include "Const.h"

//
// Ticker notification
//
void CTxTicker::Notify() {
  m_pTxThread->ProcessTxToHost();
}

CTxWorkerThread::CTxWorkerThread(CBaseWorkerThread* pThread, char siteId, wxMutex* mtx)
  : wxThread(wxTHREAD_JOINABLE), 
    m_pBaseThread(pThread),
    m_siteId(siteId),
    m_ptimer(new CTxTicker(m_siteId, this)),
    m_pSendingQueue(new wxMessageQueue<CTxData*>()),
    m_iTxPacketCnt(0U),
    m_bTxToHost(false),
    m_pMutexSerialWrite(mtx),
    m_curTxSessionId(0),
    m_curWrongSessionIdNotified(false),
    m_bRunning(new wxSemaphore(0,1))
{
  m_ptimer->Start(20, wxTIMER_CONTINUOUS);
}

CTxWorkerThread::~CTxWorkerThread() {
  m_ptimer->Stop();
  delete m_ptimer;
  delete m_bRunning;
}

void CTxWorkerThread::PostData(CTxData* data) {
  m_lastPacketType = data->GetPacketType();
  if(m_lastPacketType == packetType::CLOSING) {
    wxLogMessage("Received CLOSING");
    m_ReadyToSendTimeStamp = wxGetUTCTimeMillis();
  }
  m_lastDataPacketTimeStamp = wxGetUTCTimeMillis();
  m_pSendingQueue->Post(data);
}

void CTxWorkerThread::DisableSend() {
  m_bTxToHost = false;
}

//
// Send buffer to the host
//
int CTxWorkerThread::ProcessTxToHost() {
  CTxData *pBuf;
  packetType pType = packetType::NONE;

  bool elapsed = m_lastDataPacketTimeStamp == 0 ? FALSE : (wxGetUTCTimeMillis() - m_lastDataPacketTimeStamp) > 500;
  if(m_lastPacketType == packetType::DATA && elapsed) {
    wxLogMessage("sending pretend CLOSING");
    auto data = new CTxData((const unsigned char*)"", 0, m_curCallSign, m_curTxSessionId, packetType::CLOSING);
    this->PostData(data);
    m_ReadyToSendTimeStamp = wxGetUTCTimeMillis();
  }

  bool elapsed2 = m_ReadyToSendTimeStamp == 0 ? FALSE : (wxGetUTCTimeMillis() - m_ReadyToSendTimeStamp) > 2000;
  if(elapsed2) {
    return 0;
  }

  if(m_pSendingQueue->ReceiveTimeout(0, pBuf) == wxMSGQUEUE_TIMEOUT) {
    return 0;
  }

  pType = pBuf->GetPacketType();
  if(pType == packetType::HEADER && m_curTxSessionId == 0) {
    m_curTxSessionId = pBuf->GetSessionId();
    m_bTxToHost = true;
    m_bInvalid = false;
    //Save header packet, will be deleted when the session ends
    wxLogInfo("Header received, SessId: %X", (uint)(pBuf->GetSessionId() % 0xFFFF));
  }

  if(m_curTxSessionId != pBuf->GetSessionId()) {
    if(m_curWrongSessionIdNotified!= pBuf->GetSessionId()) {
      wxLogMessage(wxT("Wrong session id: %4X <> %4X, won't be forwarded"),
               (uint)(pBuf->GetSessionId() % 0xFFFF), (uint)(m_curTxSessionId & 0xFFFF));
      m_curWrongSessionIdNotified = pBuf->GetSessionId();
    }
    delete pBuf;
    return 0;
  }

  size_t len = 0;
  auto data = pBuf->GetData();
  auto data_len = pBuf->GetDataLen();
  m_pMutexSerialWrite->Lock();
  len = m_pBaseThread->WriteData(data, data_len);
  m_pMutexSerialWrite->Unlock();
  if(len == -1) {
     wxLogMessage(wxT("Error in sending header to the host, err: %d"), errno);
  }
  m_iTxPacketCnt++;

  if (pType == packetType::CLOSING) {
    wxLogMessage(wxT("Connector -> Host Stream Ends by type: %d (%d packets)"), pType, (int)m_iTxPacketCnt);
    m_bTxToHost = false;
    m_curCallSign.Clear();
    m_curSuffix.Clear();
    m_curTxSessionId = 0L;
    m_lastDataPacketTimeStamp = 0L;
    m_ReadyToSendTimeStamp = 0L;
  }
  delete pBuf;
  return 0;
}

void CTxWorkerThread::OnExit() {
}

wxThread::ExitCode CTxWorkerThread::Entry() {
  wxLogMessage(wxT("CTxWorkerThread started (%c)"), m_siteId);
  m_bRunning->Wait();
  return static_cast<ExitCode>(0);
}

void CTxWorkerThread::Stop() {
  m_bRunning->Post();
}

bool CTxWorkerThread::m_bEnableForwardPackets = false;
