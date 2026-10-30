#ifndef SEGMENTINDEXALLOCATOR_H
#define SEGMENTINDEXALLOCATOR_H

#include <QtGlobal>

class SegmentIndexAllocator
{
private:
    quint32 _next;

public:
    // Index framing rule: an index whose uint32_be encoding contains 0x0A or 0x0D would split
    // the Line 1 bootstrap on the wire and is forbidden; skip such indices.
    static bool hasForbiddenByte(quint32 index)
    {
        for(int shift = 0; shift < 32; shift += 8)
        {
            const uchar byte = static_cast<uchar>((index >> shift) & 0xFFU);
            if(byte == 0x0A || byte == 0x0D)
                return true;
        }
        return false;
    }

    SegmentIndexAllocator(quint32 start = 1) : _next(start) {}

    bool next(quint32 &segmentIndex)
    {
        do {
            if(_next == 0)
                return false;
            segmentIndex = _next++;
        } while(hasForbiddenByte(segmentIndex));
        return true;
    }
};

#endif // SEGMENTINDEXALLOCATOR_H
