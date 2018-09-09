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

#include "RepeaterConnector.h"
#include "DVAPWorkerThread.h"
#include "DVMegaWorkerThread.h"
#include "Const.h"

using namespace std;
class CDVAPWorkerThread;

CBaseWorkerThread* CBaseWorkerThread::CreateInstance(InstType type, char siteId, unsigned int portNumber, wxString appName) {
  switch(type) {
    case InstType::DVAP:
    return new CDVAPWorkerThread(siteId, portNumber, appName);
    case InstType::DVMega:
    return new CDVMegaWorkerThread(siteId, portNumber, appName);
  }
}


CBaseWorkerThread::CBaseWorkerThread(char siteId, unsigned int portNumber, wxString appName)
  : wxThread(wxTHREAD_JOINABLE),
    m_siteId(siteId),
    //m_pSendingQueue(new wxMessageQueue<CTxData*>()),
    //m_curTxSessionId(0),
    m_rAppName(appName),
    m_curRxSessionId(0),
    m_lastHeaderPacketTimeStamp(0),
    m_portNumber(portNumber),
    m_bStarted(false),
    m_bTxToHost(false),
    m_bInvalid(false)
{
  //m_pSendingQueue->Clear();

  //Initialization -- this part must be in construtor
  // wxExecute should be invoked from the main thread
  char devname[50];
  if(::openpty(&m_fd, &m_slavefd, devname, NULL, NULL)== -1) {
    throw new MyException(wxString::Format(wxT("Failed to open virtual port, errno:%d"), errno));
  }
  m_devName = devname;
  if(m_devName == "") {
    throw new MyException(wxString::Format(wxT("Virtual device name wasn't assigned by system. need to reboot the machine?")));
  }

  struct termios options;
  tcgetattr(m_fd, &options);
  cfsetispeed(&options, B230400);
  cfsetospeed(&options, B230400);
  tcsetattr(m_fd, TCSANOW, &options);

  //set non-blocking
  //int flags = ::fcntl(m_fd, F_GETFL, 0);
  //::fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);

  wxLogMessage(wxT("%s: Device has been created: %s"), m_rAppName, m_devName);

  //
  //Build dstarrepeter config file
  //
  wxString str,var,localConfigFile = wxString::Format(wxT("%s/dstarrepeater_%s"), m_rConfDir, m_rAppName);
  wxLogInfo("localConfigFile=%s", localConfigFile);
  wxConfigBase *config = new wxFileConfig("","", localConfigFile, "", wxCONFIG_USE_LOCAL_FILE);
  for(int i=0;;i++) {
    str = DStarRepeaterConfNames[i];
    if(str == "") {
      break;
    }
    bool ret;
    var = ""; 
    if(str==wxT("localAddress")) {
      ret = config->Write(str,wxT("0.0.0.0"));
    } else if(str==wxT("networkName")) {
      ret = config->Write(str,wxT("127.0.0.1"));
    } else if(str==wxT("gatewayAddress")) {
      ret = config->Write(str,m_dstarGatewayAddr);
    } else if(str==wxT("announcementTime") || 
       str==wxT("beaconTime") || 
       str==wxT("controlEnabled") || 
       str==wxT("rpt1Validation") || 
       str==wxT("dtmfBlanking") || 
       str==wxT("errorReply") || 
       str==wxT("pttInvert") || 
       str==wxT("output1") || 
       str==wxT("output2") || 
       str==wxT("output3") || 
       str==wxT("output4") || 
       str==wxT("windowX") || 
       str==wxT("windowY") || 
       str==wxT("announcementEnabled") || 
       str==wxT("language") || 
       str==wxT("beaconVoice") || 
       str==wxT("logging") ||
       str==wxT("ackTime") ||
       str==wxT("ack") ||
       str==wxT("restriction")) {
      ret = config->Write(str,wxT("0"));
    } else if(str==wxT("dvapPower") ||
       str==wxT("mode")) {
      ret = config->Write(str,wxT("1"));
    } else if(str==wxT("gatewayPort")) {
      ret = config->Write(str,wxT("20010"));
    } else if(str==wxT("gateway")) {
      var = wxString::Format(wxT("%-7s%c"), m_dstarRepeaterCallSign, 'G');
      ret = config->Write(str,var);
      m_myGatewayCallSign = var;
    } else if(str==wxT("callsign")) {
      var = wxString::Format(wxT("%-7s%c"), m_dstarRepeaterCallSign, m_siteId);
      ret = config->Write(str,var);
      m_myNodeCallSign = var;
    } else if(str==wxT("serialConfig")) {
      ret = config->Write(str,wxT("1"));
    } else if(str==wxT("activeHangTime")) {
      ret = config->Write(str,wxT("45"));
    } else if(str==wxT("timeout")) {
      ret = config->Write(str,wxT("180"));
    } else if(str==wxT("modemType")) {
      ret = config->Write(str,wxT("DVAP"));
    } else if(str==wxT("localPort")) {
      ret = config->Write(str,wxString::Format("%d",m_portNumber));
    } else if(str==wxT("dvapPort")) {
      ret = config->Write(str,m_devName);
    } else if(str==wxT("dvapFrequency")) {
      ret = config->Write(str,wxT("145500000"));
    } else if(str==wxT("dvapSquelch")) {
      ret = config->Write(str,wxT("-99"));
    } else {
      ret = config->Write(str,wxT(""));
    }
  }
  delete config;

  if(m_myGatewayCallSign == "") {
    throw new MyException(wxString::Format(wxT("Gateway CallSign is not set in config file")));
  }
  if(m_myNodeCallSign == "") {
    throw new MyException(wxString::Format(wxT("Repeater CallSign is not set in config file")));
  }

  wxString dstarRepeaterCmdLine = wxString::Format(wxT("%s -logdir:%s -confdir:%s \"%s\""),
     m_dstarRepeaterExe, m_rLogDir, m_rConfDir, m_rAppName);
  if(wxLog::GetVerbose()) {
    dstarRepeaterCmdLine += " --verbose";
  }
  if(m_bStartDstarRepeater) {
    wxLogInfo(wxT("Execute: %s"), dstarRepeaterCmdLine);
    wxExecute(dstarRepeaterCmdLine);
  } else {
    wxLogInfo(wxT("run this command manually: %s"), dstarRepeaterCmdLine);
  }
  m_myNodeCallSign = wxString::Format(wxT("%-7s%c"),m_myNodeCallSign.Mid(0,7), m_siteId);
  wxLogMessage(wxT("Repeater CallSign: %s, Gateway CallSign: %s"), m_myNodeCallSign, m_myGatewayCallSign);
}

//
//Main loop
//
CBaseWorkerThread::ExitCode CBaseWorkerThread::Entry() {
  wxLogMessage(wxT("CBaseWorkerThread started (%c)"), m_siteId);


  return static_cast<ExitCode>(0);
}

void CBaseWorkerThread::RegisterOtherInstance(CBaseWorkerThread *ptr) {
  if(ptr != this) {
    wxLogInfo(wxT("Other thread instance is added"));
    m_threads.Add(ptr);
  }
}

wxArrayCTxData CBaseWorkerThread::SendToInstance(unsigned char* data, size_t len, packetType ptype) {
  wxArrayCTxData arr;
  for(int i = 0; i < m_threads.GetCount(); i++) {
    CTxData* pTxData = new CTxData(data, len, m_curCallSign, m_curRxSessionId, ptype);
    ((CBaseWorkerThread*)m_threads[i])->PostData(pTxData);
    arr.Add(pTxData);
  }
  return arr;
}

static const unsigned int crc_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

//
// caclulate CRC16-CCITT value
// parameters:
//   data: byte array blob data.
//   len: size of the data buffer (including extra space for 2 byte CRC)
// description:
//   the function calculates CCITT CRC value. the target range
//   is data[0]...data[len-3], and the 2 bytes of result will 
//   be stored in data[len-2] and data[len-1]
//
void CBaseWorkerThread::CalcCRC(unsigned char* data, int len) {
  unsigned short crc = 0xffff;

  for (int i = 0; i <= len - 2; i++) {
    crc = (unsigned short)((crc >> 8) ^ crc_table[(unsigned char)(crc ^ data[i])]);
    //unsigned char temp = (crc >> 8) ^ data[i];
    //crc = (crc << 8) ^ crc_table[temp];
  }                  
  data[len - 1] = (crc & 0xFF);
  data[len - 2] = ((crc >> 8) & 0xFF);
}

void CBaseWorkerThread::dumper(const char* header, unsigned char* buff, int len)
{
  wxString dump = "", s = "";
  for(int i = 0; i < len; i++ ) {
    s.Printf(wxT("%02.2X "), buff[i]);
    dump += s;
  }
  wxLogInfo(wxT("%5s: %s"), header, dump);
}


CBaseWorkerThread::~CBaseWorkerThread()
{
  wxLogMessage(wxT("%c Destructor of CBaseWorkerThread"), m_siteId);
  if(m_fd) {
    ::close(m_fd);
  }
  if(m_slavefd) {
    ::close(m_slavefd);
  }
}

size_t CBaseWorkerThread::WriteData(const unsigned char* data, size_t len)
{
  return ::write(m_fd, data, len);
}

int CBaseWorkerThread::ProcessData() {
}

void CBaseWorkerThread::OnExit() {
  wxLogMessage(wxT("CBaseWorkerThread::OnExit"));
}

void CBaseWorkerThread::PostData(CTxData* data) {
}

wxString CBaseWorkerThread::m_dstarRepeaterExe = "";
wxString CBaseWorkerThread::m_dstarRepeaterCallSign = "";
wxString CBaseWorkerThread::m_rConfDir = "";
wxString CBaseWorkerThread::m_rLogDir = "";
wxString CBaseWorkerThread::m_dstarGatewayAddr = "127.0.0.1";
long CBaseWorkerThread::m_dstarGatewayPort = 20010;
bool CBaseWorkerThread::m_bStartDstarRepeater = false;
//bool CBaseWorkerThread::m_bEnableForwardPackets = false;
bool CBaseWorkerThread::m_bEnableDumpPackets = false;
