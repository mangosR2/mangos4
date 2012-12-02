/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ByteBuffer.h"
#include "Log.h"

void ByteBufferException::PrintPosError() const
{
    char const* traceStr;

#ifdef HAVE_ACE_STACK_TRACE_H
    ACE_Stack_Trace trace;
    traceStr = trace.c_str();
#else
    traceStr = NULL;
#endif

    sLog.outError(
        "Attempted to %s in ByteBuffer (pos: " SIZEFMTD " size: "SIZEFMTD") "
        "value with size: " SIZEFMTD "%s%s",
        (add ? "put" : "get"), pos, size, esize,
        traceStr ? "\n" : "", traceStr ? traceStr : "");
}

void ByteBuffer::print_storage() const
{
    if (!sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))   // optimize disabled debug output
        return;

    std::ostringstream ss;
    ss <<  "STORAGE_SIZE: " << size() << "\n";

    if (sLog.IsIncludeTime())
        ss << "         ";

    for (size_t i = 0; i < size(); ++i)
        ss << uint32(read<uint8>(i)) << " - ";

    sLog.outDebug(ss.str().c_str());
}

void ByteBuffer::textlike() const
{
    if (!sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))   // optimize disabled debug output
        return;

    std::ostringstream ss;
    ss <<  "STORAGE_SIZE: " << size() << "\n";

    if (sLog.IsIncludeTime())
        ss << "         ";

    for (size_t i = 0; i < size(); ++i)
        ss << read<uint8>(i);

    sLog.outDebug(ss.str().c_str());
}

void ByteBuffer::hexlike() const
{
    if (!sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))   // optimize disabled debug output
        return;

    std::ostringstream ss;
    ss <<  "STORAGE_SIZE: " << size() << "\n";

    if (sLog.IsIncludeTime())
        ss << "         ";

    size_t j = 1, k = 1;

    for (size_t i = 0; i < size(); ++i)
    {
        if ((i == (j * 8)) && ((i != (k * 16))))
        {
            ss << "| ";
            ++j;
        }
        else if (i == (k * 16))
        {
            ss << "\n";

            if (sLog.IsIncludeTime())
                ss << "         ";

            ++k;
            ++j;
        }

        char buf[4];
        snprintf(buf, 4, "%02X", read<uint8>(i));
        ss << buf << " ";
    }

    sLog.outDebug(ss.str().c_str());
}

uint32 ByteBuffer::ReadPackedTime()
{
    uint32 packedDate = read<uint32>();

    tm _localtime;
    memset(&_localtime, 0, sizeof(_localtime));

    _localtime.tm_min     = packedDate & 0x3F;
    _localtime.tm_hour    = (packedDate >> 6) & 0x1F;
    //_localtime.tm_wday   = (packedDate >> 11) & 7;
    _localtime.tm_mday    = ((packedDate >> 14) & 0x3F) + 1;
    _localtime.tm_mon     = (packedDate >> 20) & 0xF;
    _localtime.tm_year    = ((packedDate >> 24) & 0x1F) + 100;

    return mktime(&_localtime) + timezone;
}

void ByteBuffer::AppendPackedTime(time_t time)
{
    tm* _localtime = localtime(&time);
    append<uint32>((_localtime->tm_year - 100) << 24 | _localtime->tm_mon  << 20 | (_localtime->tm_mday - 1) << 14 | _localtime->tm_wday << 11 | _localtime->tm_hour << 6 | _localtime->tm_min);
}

void BitStream::Clear()
{
    _data.clear();
    _rpos = _wpos = 0;
}

uint8 BitStream::GetBit(uint32 bit)
{
    MANGOS_ASSERT(_data.size() > bit);
    return _data[bit];
}

uint8 BitStream::ReadBit()
{
    MANGOS_ASSERT(_data.size() < _rpos);
    uint8 b = _data[_rpos];
    ++_rpos;
    return b;
}

void BitStream::WriteBit(uint32 bit)
{
    _data.push_back(bit ? uint8(1) : uint8(0));
    ++_wpos;
}

template <typename T> void BitStream::WriteBits(T value, size_t bits)
{
    for (int32 i = bits-1; i >= 0; --i)
        WriteBit((value >> i) & 1);
}

bool BitStream::Empty()
{
    return _data.empty();
}

void BitStream::Reverse()
{
    uint32 len = GetLength();
    std::vector<uint8> b = _data;
    Clear();

    for(uint32 i = len; i > 0; --i)
        WriteBit(b[i-1]);
}

void BitStream::Print()
{
    std::stringstream ss;
    ss << "BitStream: ";
    for (uint32 i = 0; i < GetLength(); ++i)
        ss << uint32(GetBit(i)) << " ";

    sLog.outDebug(ss.str().c_str());
}
