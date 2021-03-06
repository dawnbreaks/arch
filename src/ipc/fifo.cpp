/*
 * fifo.cpp
 *
 *  Created on: 2010-10-4
 *      Author: wqy
 */
#include "ipc/fifo.hpp"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>


using namespace arch::ipc;
FIFO::FIFO(const string& location) :
    m_FD(-1), m_location(location)
{
    if (mkfifo(m_location.c_str(), 0600) == -1)
    {
        //check if current file is FIFO
        struct stat st;
        if (stat(m_location.c_str(), &st) == 0)
        {
            if (!S_ISFIFO(st.st_mode))
            {
                throw APIException("Existed file is not FIFO", EEXIST);
            }
        }
        else
        {
            throw APIException("Create FIFO failed.", EEXIST);
        }
    }
}

int FIFO::Open(int flag)
{
    if (-1 == m_FD)
    {
        m_FD = ::open(m_location.c_str(), GetOpenFlag() | flag);
        if (m_FD < 0)
        {
            return -1;
        }
    }
    return 0;
}

int FIFO::Open()
{
    if (-1 == m_FD)
    {
        m_FD = ::open(m_location.c_str(), GetOpenFlag());
        if (m_FD < 0)
        {
            return -1;
        }
    }
    return 0;
}

int FIFO::GetFD()
{
    return m_FD;
}

void FIFO::Close()
{
    if (-1 != m_FD)
    {
        ::close(m_FD);
        m_FD = -1;
    }
}

FIFO::~FIFO()
{
    Close();
}

ReadFIFO::ReadFIFO(const string& location) :
    FIFO(location)
{
}

int ReadFIFO::GetOpenFlag()
{
    return O_RDONLY | O_NONBLOCK;
}

int ReadFIFO::Read(void* buf, int buflen)
{
    int ret = ::read(m_FD, buf, buflen);
    if (-1 == ret)
    {
        if (EAGAIN == errno)
        {
            return 0;
        }
    }
    return ret;
}

WriteFIFO::WriteFIFO(const string& location) :
    FIFO(location)
{
}

int WriteFIFO::GetOpenFlag()
{
    return O_WRONLY;
}

int WriteFIFO::Write(const void* buf, int buflen)
{
    int ret = ::write(m_FD, buf, buflen);
    if (-1 == ret)
    {
        if (EAGAIN == errno)
        {
            return 0;
        }
    }
    return ret;
}
