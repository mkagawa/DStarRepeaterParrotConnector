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
#ifndef _CBaseWorkerThread_
#define _CBaseWorkerThread_

#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include <wx/wx.h>
#include <wx/regex.h>
#include <wx/fileconf.h>
#include <wx/memory.h> 
#include <wx/log.h>
#include <wx/msgqueue.h> 
#include <wx/buffer.h> 
#include <wx/time.h> 

#include "Const.h"
#include "TxData.h"

#include <pty.h>

enum InstType {
  DVMega = 1,
  DVAP = 2
};

WX_DEFINE_ARRAY_PTR(CTxData*, wxArrayCTxData);
WX_DEFINE_ARRAY_PTR(wxThread*, wxArrayThread);
WX_DECLARE_STRING_HASH_MAP( wxString, repeaterConfigMapType);

class MyException : public std::exception {
  public:
    MyException(wxString msg) 
      :m_msg(msg)
    {
  
    }
    wxString GetMessage() { return m_msg; }
  private:
    wxString m_msg;
};

class CBaseWorkerThread : public wxThread {
  public:
    CBaseWorkerThread(char,unsigned int,wxString);
    virtual ~CBaseWorkerThread();

    virtual size_t WriteData(const unsigned char* data, size_t len);
    // void RegisterOtherInstance(CBaseWorkerThread* ptr);

    static CBaseWorkerThread* CreateInstance(InstType, char siteId, unsigned int portNumber, wxString appName);

    //public static property
    static wxString m_dstarRepeaterExe;
    static wxString m_dstarRepeaterCallSign; //base callsign no suffix
    static wxString m_dstarGatewayAddr;
    static long m_dstarGatewayPort;
    static bool m_bStartDstarRepeater;
    //static bool m_bEnableForwardPackets;
    static bool m_bEnableDumpPackets;
    static wxString m_rConfDir;
    static wxString m_rLogDir;

  private:
    InstType m_type;
    int m_slavefd;
    unsigned int m_portNumber;
    wxArrayThread m_threads;
    wxString m_rAppName; //for dstarrepeater
    CTxData* m_pTxHeaderPacket;

  protected: 
    virtual void PostData(CTxData * data);
    bool m_bStarted;
    void ProcessTxToHost();
    char m_siteId;
    //wxMessageQueue<CTxData *> *m_pSendingQueue;
    wxArrayCTxData m_arrHeaderPacket;
    wxArrayCTxData SendToInstance(const unsigned char* data, size_t len, packetType);
    // thread execution starts here
    virtual ExitCode Entry();
    virtual int ProcessData() = 0;
    virtual void OnExit();

    wxString m_devName;
    int m_fd = -1;
    unsigned char m_buffer[DVAP_BUFFER_LENGTH];
    unsigned char m_wbuffer[DVAP_BUFFER_LENGTH];

    wxString m_myNodeCallSign;
    wxString m_myGatewayCallSign;
    wxString m_curCallSign;
    wxString m_curSuffix;
    unsigned char m_packetSerialNo;
    ulong m_curRxSessionId;

    ulong m_iTxPacketCnt;
    ulong m_iRxPacketCnt;

    //bool m_txEnabled, m_checksum, m_tx, m_txSpace;
    //int space;

    wxLongLong m_lastHeaderPacketTimeStamp;
    wxLongLong m_lastTxPacketTimeStamp;
    wxLongLong m_lastReceivedFromHostTimeStamp;
    bool m_bTxToHost = false; // DVAP->Host Stream
    bool m_bInvalid;


    //inticats ready to receive packet
    bool m_initialized = false;

    static void CalcCRC(unsigned char* data, int len);

    //dump m_buffer
    static void dumper(const char* head, unsigned char* buff, int len);
};

#endif
