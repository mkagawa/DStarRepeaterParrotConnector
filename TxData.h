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
#ifndef __TxData__
#define __TxData__
#include <wx/wx.h>
#include <wx/regex.h>
#include <wx/fileconf.h>
#include <wx/memory.h> 
#include <wx/log.h>
#include <wx/msgqueue.h> 
#include <wx/buffer.h> 
#include <wx/time.h> 

enum packetType {
  NONE,
  HEADER,
  HEADER_NOSEND,
  CLOSING,
  DATA
};

//
//Data structure used to exchange GMSK data between threads
//
class CTxData {
  private:
    wxMemoryBuffer m_buffer;
    wxString m_myCallSign;
    ulong m_sessionId;
    ulong m_packetNo;
    wxLongLong m_lastPacketTimeStamp;
    packetType m_packetType;

    //whether this packet is already sent
    bool m_bSent;

  public:
    CTxData(unsigned char* data, size_t data_len, wxString cs, ulong sessionId, packetType);
    ~CTxData();

    void UpdatePacketType(packetType t) { m_packetType = t; }
    const bool IsSent() { return m_bSent; }
    const bool IsNoSend() { return m_packetType == packetType::HEADER_NOSEND; }
    const packetType GetPacketType() { return m_packetType; }
    const ulong GetSessionId() { return m_sessionId; }
    const wxString GetCallSign() { return m_myCallSign; }
    const unsigned char* GetData() { return (unsigned char*)m_buffer.GetData(); }
    const size_t GetDataLen() { return m_buffer.GetDataLen(); }
    void MarkAsSent() { m_bSent = true; }
};

#endif
