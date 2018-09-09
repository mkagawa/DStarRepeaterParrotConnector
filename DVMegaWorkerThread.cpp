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
//
// this is a DVMega simulator
//
// the program provides two virtual serial ports, which is
// acting as DVMega board serial port

#include "RepeaterConnector.h"
#include "DVMegaWorkerThread.h"
#include "Const.h"

using namespace std;

CDVMegaWorkerThread::~CDVMegaWorkerThread()
{
}

CDVMegaWorkerThread::CDVMegaWorkerThread(char siteId, unsigned int portNumber, wxString appName)
 :CBaseWorkerThread(siteId,portNumber,appName)
{
}

int CDVMegaWorkerThread::ProcessData() {
  //wxLogMessage(wxT("%d ProcessData"), m_fd);

  size_t len = 0; 
  while(m_buffer[0]!=DVRPTR_FRAME_START) {
    len = ::read(m_fd, m_buffer, 1);
    if(len != 1) {
      return 0;
    }

#ifdef DEBUG
    //write back to client
    ::write(m_fd, m_buffer, 1);
    wxLogMessage(wxT("buff = %X"), m_buffer[0]);
#endif
  }

  len = ::read(m_fd, m_buffer, 3);
  size_t size = m_buffer[0] + m_buffer[1] * 256;
  int cmd = m_buffer[2];
  int pos = 0;
  switch(cmd) {
    case DVRPTR_GET_VERSION:
      if( size == 1 ) {
        len = ::read(m_fd, &m_buffer[4], size - 1 + 2); //check sum only
        m_wbuffer[pos++] = DVRPTR_FRAME_START;
        m_wbuffer[pos++] = 17;
        m_wbuffer[pos++] = 0;
        m_wbuffer[pos++] = cmd | DVRPTR_RESPONSE_BIT;
        ::memcpy(&m_wbuffer[pos], _DVMEGA_VERSION_STR, sizeof(_DVMEGA_VERSION_STR)-1);
        pos += sizeof(_DVMEGA_VERSION_STR)-1;
        m_wbuffer[pos++] = 0x00;
        m_wbuffer[pos++] = 0x0b;
        ::write(m_fd, m_wbuffer, pos);
        wxLogMessage(wxT("reponsed to DVRPTR_GET_VERSION"));
      } else {
        wxLogMessage(wxT("length was incorrect for DVRPTR_GET_VERSION: %d"), size);
      }
      break;

    case DVRPTR_GET_STATUS :
      if( size == 1 ) {
        len = ::read(m_fd, &m_buffer[4], size - 1 + 2); //check sum only
        //m_txEnabled = true; //(m_buffer[4U] & 0x02U) == 0x02U;
        //m_checksum  = false; //(m_buffer[4U] & 0x08U) == 0x08U;
        //m_tx        = false; //(m_buffer[5U] & 0x02U) == 0x02U;
        //m_txSpace   = 0xfc; //m_buffer[8U];
        //space       = m_txSpace - m_buffer[9U];

        m_wbuffer[pos++] = DVRPTR_FRAME_START;
        m_wbuffer[pos++] = 8;
        m_wbuffer[pos++] = 0;
        m_wbuffer[pos++] = cmd | DVRPTR_RESPONSE_BIT;
        m_wbuffer[pos++] = 0x47; //flags txEnabled:0x2(on),checkSum:0x8(off)
        m_wbuffer[pos++] = 0;    //flags tx:0x02 
        m_wbuffer[pos++] = 0x15; //??
        m_wbuffer[pos++] = 0; //m_txSpace;
        m_wbuffer[pos++] = 0;    //status unsend frames
        m_wbuffer[pos++] = 0;    //total packets to send
        m_wbuffer[pos++] = 0x00; //cs
        m_wbuffer[pos++] = 0x0b; //cs
        ::write(m_fd, m_wbuffer, pos);
        wxLogMessage(wxT("reponsed to DVRPTR_GET_STATUS"));
      } else {
        wxLogMessage(wxT("length was incorrect DVRPTR_GET_STATUS: %d"), size);
      }
      break;

    case DVRPTR_GET_SERIAL :
      if( size == 1 ) {
        len = ::read(m_fd, &m_buffer[4], size - 1 + 2); //check sum only
        m_wbuffer[pos++] = DVRPTR_FRAME_START;
        m_wbuffer[pos++] = 5;
        m_wbuffer[pos++] = 0;
        m_wbuffer[pos++] = cmd | DVRPTR_RESPONSE_BIT;
        m_wbuffer[pos++] = 0x13;
        m_wbuffer[pos++] = 0x09;
        m_wbuffer[pos++] = 0x10;
        m_wbuffer[pos++] = 0x01;
        m_wbuffer[pos++] = 0x00; //cs
        m_wbuffer[pos++] = 0x0b; //cs
        ::write(m_fd, m_wbuffer, pos);
        wxLogMessage(wxT("reponsed to DVRPTR_GET_STATUS"));
      } else {
        wxLogMessage(wxT("length was incorrect DVRPTR_GET_STATUS: %d"), size);
      }
      break;

    case DVRPTR_GET_CONFIG :
      if( size == 1 ) {
        len = ::read(m_fd, &m_buffer[4], size - 1 + 2); //check sum only
        m_wbuffer[pos++] = DVRPTR_FRAME_START;
        m_wbuffer[pos++] = 5;
        m_wbuffer[pos++] = 0;
        m_wbuffer[pos++] = cmd | DVRPTR_RESPONSE_BIT;
        m_wbuffer[pos++] = 0x13;
        m_wbuffer[pos++] = 0x09;
        m_wbuffer[pos++] = 0x10;
        m_wbuffer[pos++] = 0x01;
        m_wbuffer[pos++] = 0x00; //cs
        m_wbuffer[pos++] = 0x0b; //cs
        ::write(m_fd, m_wbuffer, pos);
        wxLogMessage(wxT("reponsed to DVRPTR_GET_STATUS"));
      } else {
        wxLogMessage(wxT("length was incorrect DVRPTR_GET_STATUS: %d"), size);
      }
      break;

    case DVRPTR_SET_CONFIG :
      if( size < 4 ) {
        wxLogMessage(wxT("length was incorrect DVRPTR_SET_CONFIG: %d"), size);
        break;
      }
      len = ::read(m_fd, &m_buffer[4], size - 1 + 2); //data and check sum

      /* config value parser fgor debug
      if(m_buffer[4] != 0xc0) {
        int config_flags = m_buffer[6];
        int rx_inverse = config_flags & 0x01 == 0x01;
        int tx_inverse = config_flags & 0x02 == 0x02;
        int band = config_flags & 0x04 == 0x04; //true if 70cm, otherwise 2m 
        int config_modulation = m_buffer[7];
        int config_TX_delay = m_buffer[8] * m_buffer[9] * 256;

        if(band == true) {
          //Set_ADF7021_UHF();
          m_buffer[7] = 0x40;
          m_buffer[8] = 0x78;
          m_buffer[9] = 0xE7;
          m_buffer[10] = 0x19;  
          CALC_REG0();
          ADF7021_ini();  
          rx_inverse = false;
          tx_inverse = false;
        } else { 
          //Set_ADF7021_VHF();
          m_buffer[7] = 0xF8;
          m_buffer[8] = 0x9D;
          m_buffer[9] = 0xA2;
          m_buffer[10] = 0x08;  
          CALC_REG0();
          ADF7021_ini();
          rx_inverse = false;
          tx_inverse = false;
        }
      } else if(m_buffer[4] != 0xc1) { //rf layer
        //detachInterrupt(0);
        //detachInterrupt(1);    
        rx_inverse = false;
        tx_inverse = false;
        CALC_REG0();
        if(band ==false) { //vhf
          ipinAD7021_SLE = 12;  
        } else {
          ipinAD7021_SLE = 11;  
        }      
        ADF7021_ini();                        // Initiate ADF7021
      }
      */

      if( size == 1 ) {
        m_wbuffer[pos++] = DVRPTR_FRAME_START;
        m_wbuffer[pos++] = 2;
        m_wbuffer[pos++] = 0;
        m_wbuffer[pos++] = cmd | DVRPTR_RESPONSE_BIT;
        m_wbuffer[pos++] = 0x06;
        m_wbuffer[pos++] = 0x00; //cs
        m_wbuffer[pos++] = 0x0b; //cs
        ::write(m_fd, m_wbuffer, pos);
        wxLogMessage(wxT("reponsed to DVRPTR_SET_CONFIG"));
      } else {
        wxLogMessage(wxT("length was incorrect DVRPTR_GET_STATUS: %d"), size);
      }
      break;

    case DVRPTR_RXPREAMBLE :
      break;

    case DVRPTR_START      :
      break;

    case DVRPTR_HEADER     :
      break;

    case DVRPTR_RXSYNC     :
      break;

    case DVRPTR_DATA       :
      break;

    case DVRPTR_EOT        :
      break;

    case DVRPTR_RXLOST     :
      break;

    case DVRPTR_MSG_RSVD1  :
      break;

    case DVRPTR_MSG_RSVD2  :
      break;

    case DVRPTR_MSG_RSVD3  :
      break;

    case DVRPTR_SET_TESTMDE:
      break;

    default:
      wxLogMessage(wxT("Unknown cmd %X"), cmd);
      break;
  }
  return 1;
}
