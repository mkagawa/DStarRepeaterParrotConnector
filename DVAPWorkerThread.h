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
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>

#include <wx/wx.h>
#include <wx/regex.h>
#include <wx/memory.h> 
#include <wx/log.h> 

#include <pty.h>
#include "BaseWorkerThread.h"
#include "TxWorkerThread.h"

using namespace std;
class CDVAPWorkerThread : public CBaseWorkerThread {
  public:
    CDVAPWorkerThread(char siteId, unsigned int,wxString);
    ~CDVAPWorkerThread();

  protected:
    virtual int ProcessData();
    virtual ExitCode Entry();
    //Write locker
    wxMutex *m_pMutexSerialWrite;
    void PostData(CTxData * data);

  private:
    int _ProcessMessage(size_t data_len);
    wxLongLong m_lastStatusSentTimeStamp;
    wxLongLong m_lastAckTimeStamp;
    CTxWorkerThread* m_pTxWorker;
};

