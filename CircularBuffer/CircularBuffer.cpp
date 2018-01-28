// CircularBuffer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <cassert>
#include <algorithm>
#include <vector>
#include <memory>

using namespace std;

class CircularBuffer
{
public:
    CircularBuffer(int capacityInBytes, bool overwriteWhenFull) :
        _capacity(capacityInBytes),
        _iProducer(0),
        _iConsumer(0), // consumer index should never pass producer index
        _size(0),
        _overwriteWhenFull(overwriteWhenFull),
        _buffer(make_unique<unsigned char[]>(capacityInBytes))
    {
        // todo: handle the _overwriteWhenFull variation of CircularBuffer along with tests
        assert(_overwriteWhenFull == false);
    }

    ~CircularBuffer()
    {
    }

    bool empty()
    {
        return (_size == 0);
    }

    int size()
    {
        return _size;
    }

    bool write(void *const buffer, int bufferSize, int& bytesWritten)
    {
        if (buffer == nullptr)
        {
            return false;
        }

        const int totalBytesToWrite = min(_availableSpace(), bufferSize);
        if (totalBytesToWrite <= 0)
        {
            return false;
        }

        if (_iProducer < _iConsumer)
        {
            // all the available write space (i.e. writableBytes) is between _iProducer and _iConsumer
            memmove((_buffer.get() + _iProducer), buffer, totalBytesToWrite);
        }
        else //(_iProducer >= _iConsumer)
        {
            // need to write in two parts based on writableBytes: 
            // 1. from _iProducer to end of buffer 
            // 2. (potentially) from 0 to _iConsumer
            const int bytesAvailableBetweeniProducerAndEndOfBuffer = _capacity - _iProducer;
            if (bytesAvailableBetweeniProducerAndEndOfBuffer >= totalBytesToWrite)
            {
                memmove((_buffer.get() + _iProducer), buffer, totalBytesToWrite);
            }
            else
            {
                memmove((_buffer.get() + _iProducer), buffer, bytesAvailableBetweeniProducerAndEndOfBuffer);
                void *callerBufferOffset = static_cast<void*>(static_cast<unsigned char *>(buffer) + bytesAvailableBetweeniProducerAndEndOfBuffer);
                memmove(_buffer.get(), callerBufferOffset, (totalBytesToWrite - bytesAvailableBetweeniProducerAndEndOfBuffer));
            }
        }

        _size += totalBytesToWrite;
        _updateIndex(_iProducer, totalBytesToWrite);
        bytesWritten = totalBytesToWrite;

        return true;
    }

    bool read(void *const buffer, int bufferSize, int& bytesRead)
    {
        if (buffer == nullptr)
        {
            return false;
        }

        const int totalBytesToRead = min(_size, bufferSize);
        if (totalBytesToRead <= 0)
        {
            return false;
        }

        if (_iConsumer < _iProducer)
        {
            // all the requested read bytes (i.e. totalBytesToRead) are between _iConsumer and _iProducer 
            memmove(buffer, (_buffer.get() + _iConsumer), totalBytesToRead);
        }
        else // (_iConsumer > _iProducer)
        {
            // need to read in two parts: 
            // 1. from _iConsumer to end of buffer 
            // 2. (potentially) from 0 to _iProducer
            const int bytesAvailableBetweeniConsumerAndEndOfBuffer = _capacity - _iConsumer;
            if (totalBytesToRead <= bytesAvailableBetweeniConsumerAndEndOfBuffer)
            {
                memmove(buffer, (_buffer.get() + _iConsumer), totalBytesToRead);
            }
            else
            {
                memmove(buffer, (_buffer.get() + _iConsumer), bytesAvailableBetweeniConsumerAndEndOfBuffer);
                void *callerBufferOffset = static_cast<void*>(static_cast<unsigned char *>(buffer) + bytesAvailableBetweeniConsumerAndEndOfBuffer);
                memmove(callerBufferOffset, _buffer.get(), (totalBytesToRead - bytesAvailableBetweeniConsumerAndEndOfBuffer));
            }
        }

        _size -= totalBytesToRead;
        _updateIndex(_iConsumer, totalBytesToRead);
        bytesRead = totalBytesToRead;

        return true;
    }

    void clear()
    {
        _size = 0;
        _iProducer = 0;
        _iConsumer = 0;
    }

private:
    int _capacity;
    int _iProducer; // index of next write location
    int _iConsumer; // index of next read location
    int _size;
    int _overwriteWhenFull;
    unique_ptr<unsigned char[]> _buffer;
    int _availableSpace()
    {
        return (_capacity - _size);
    }
    void _updateIndex(int& index, int bytes)
    {
        index += bytes;
        if (index >= _capacity)
        {
            index -= _capacity;
        }
    }
};


void testCircularBufferNoOverwriteWhenFullMultiByte()
{
    unsigned char input[] = { 1,2,3,4,5,6,7,8, };
    const int inputBufferSizeInBytes = (_countof(input) * sizeof(input[0]));
    int capacityInBytes = inputBufferSizeInBytes / 2; // we want a smaller buffer
    int output[inputBufferSizeInBytes] = { 0 };
    const int outputBufferSizeInBytes = inputBufferSizeInBytes;
    int bytesRead = -1;
    int bytesWritten = -1;
    unsigned char valR = -1;
    unsigned char valW = -1;
    bool flag = false;
    int testBytesWrite = -1;
    int testBytesRead = -1;
    int expectedBytesWritten = -1;
    int expectedBytesRead = -1;
    const int capacity = 3;
    CircularBuffer cb(capacity, false);

    // empty state
    assert(cb.empty() == true);
    assert(cb.size() == 0);
    assert(cb.read(static_cast<void*>(output), outputBufferSizeInBytes, bytesRead) == false);

    vector<pair<int,int>> testMatrix{
        { -1, -1 },
        { -1, 0 },
        { -1, +1 },
        { 1, -1 },
        { 1, 0 },
        { 1, +1 },
        { +1, -1 },
        { +1, 0 },
        { +1, +1 },
    };
    for (int i = 0; i < testMatrix.size(); i++)
    {
        testBytesWrite = capacity + testMatrix[i].first;
        testBytesRead = capacity + testMatrix[i].second;
        expectedBytesWritten = min(capacity, testBytesWrite);
        expectedBytesRead = min(expectedBytesWritten, testBytesRead);
        memset(output, -1, outputBufferSizeInBytes);
        assert(cb.write((void*)input, testBytesWrite, bytesWritten) == true);
        assert(cb.empty() == false);
        assert(cb.size() == expectedBytesWritten);
        assert(bytesWritten == expectedBytesWritten);
        assert(cb.read((void*)output, testBytesRead, bytesRead) == true);
        assert(bytesRead == expectedBytesRead);
        assert(memcmp(input, output, expectedBytesRead) == 0);
        bool isEmptyExpected = expectedBytesRead >= expectedBytesWritten;

        // flush out buffer
        assert(cb.empty() == isEmptyExpected);
        if (isEmptyExpected == false)
        {
            int residualSize = expectedBytesWritten - expectedBytesRead;
            assert(cb.size() == residualSize);
            assert(cb.read(static_cast<void*>(output), residualSize, bytesRead) == true);
        }
        // empty state
        assert(cb.empty() == true);
        assert(cb.size() == 0);
        assert(cb.read(static_cast<void*>(output), 1, bytesRead) == false);
    }
}

void testCircularBufferNoOverwriteWhenFullMultiByteWritesAndReadsInterspersed()
{
    // todo
    cout << "testCircularBufferNoOverwriteWhenFullMultiByteWritesAndReadsInterspersed() --- TODO ---" << endl;
}

void testCircularBufferNoOverwriteWhenFullSingleByte()
{
    unsigned char input[] = { 1,2,3,4,5,6,7,8, };
    const int inputBufferSizeInBytes = (_countof(input) * sizeof(input[0]));
    int capacityInBytes = inputBufferSizeInBytes / 2; // we want a smaller buffer
    int output[inputBufferSizeInBytes] = { 0 };
    const int outputBufferSizeInBytes = inputBufferSizeInBytes;
    int bytesRead = -1;
    int bytesWritten = -1;
    unsigned char valR = -1;
    unsigned char valW = -1;
    bool flag = false;
    CircularBuffer cb(3, false);

    // empty state
    assert(cb.empty() == true);
    assert(cb.size() == 0);
    assert(cb.read(static_cast<void*>(output), outputBufferSizeInBytes, bytesRead) == false);

    // write one item
    valW = 1;
    assert(cb.write((void*)&valW, sizeof(valW), bytesWritten) == true);
    assert(cb.empty() == false);
    assert(cb.size() == 1);
    assert(bytesWritten == sizeof(valW));
    assert(cb.read((void*)&valR, sizeof(valR), bytesRead) == true);
    assert(bytesRead == sizeof(valR));
    assert(valR == valW);
    // empty state
    assert(cb.empty() == true);
    assert(cb.size() == 0);
    assert(cb.read(static_cast<void*>(output), outputBufferSizeInBytes, bytesRead) == false);

    // write two items
    valW = 1;
    assert(cb.write((void*)&valW, sizeof(valW), bytesWritten) == true);
    assert(cb.empty() == false);
    assert(cb.size() == 1);
    assert(bytesWritten == sizeof(valW));
    valW = 2;
    assert(cb.write((void*)&valW, sizeof(valW), bytesWritten) == true);
    assert(cb.empty() == false);
    assert(cb.size() == 2);
    assert(bytesWritten == sizeof(valW));
    assert(cb.read((void*)&valR, sizeof(valR), bytesRead) == true);
    assert(bytesRead == sizeof(valR));
    assert(valR == 1);
    assert(cb.empty() == false);
    assert(cb.size() == 1);
    assert(cb.read((void*)&valR, sizeof(valR), bytesRead) == true);
    assert(bytesRead == sizeof(valR));
    assert(valR == 2);
    // empty state
    assert(cb.empty() == true);
    assert(cb.size() == 0);
    assert(cb.read(static_cast<void*>(output), outputBufferSizeInBytes, bytesRead) == false);

    // write three items
    valW = 1;
    assert(cb.write((void*)&valW, sizeof(valW), bytesWritten) == true);
    assert(cb.empty() == false);
    assert(cb.size() == 1);
    assert(bytesWritten == sizeof(valW));
    valW = 2;
    assert(cb.write((void*)&valW, sizeof(valW), bytesWritten) == true);
    assert(cb.empty() == false);
    assert(cb.size() == 2);
    assert(bytesWritten == sizeof(valW));
    valW = 3;
    assert(cb.write((void*)&valW, sizeof(valW), bytesWritten) == true);
    assert(cb.empty() == false);
    assert(cb.size() == 3);
    assert(bytesWritten == sizeof(valW));
    assert(cb.read((void*)&valR, sizeof(valR), bytesRead) == true);
    assert(bytesRead == sizeof(valR));
    assert(valR == 1);
    assert(cb.empty() == false);
    assert(cb.size() == 2);
    assert(cb.read((void*)&valR, sizeof(valR), bytesRead) == true);
    assert(bytesRead == sizeof(valR));
    assert(valR == 2);
    assert(cb.empty() == false);
    assert(cb.size() == 1);
    assert(cb.read((void*)&valR, sizeof(valR), bytesRead) == true);
    assert(bytesRead == sizeof(valR));
    assert(valR == 3);
    // empty state
    assert(cb.empty() == true);
    assert(cb.size() == 0);
    assert(cb.read(static_cast<void*>(output), outputBufferSizeInBytes, bytesRead) == false);

    // try write four items
    valW = 1;
    assert(cb.write((void*)&valW, sizeof(valW), bytesWritten) == true);
    assert(cb.empty() == false);
    assert(cb.size() == 1);
    assert(bytesWritten == sizeof(valW));
    valW = 2;
    assert(cb.write((void*)&valW, sizeof(valW), bytesWritten) == true);
    assert(cb.empty() == false);
    assert(cb.size() == 2);
    assert(bytesWritten == sizeof(valW));
    valW = 3;
    assert(cb.write((void*)&valW, sizeof(valW), bytesWritten) == true);
    assert(cb.empty() == false);
    assert(cb.size() == 3);
    assert(bytesWritten == sizeof(valW));
    valW = 4;
    assert(cb.write((void*)&valW, sizeof(valW), bytesWritten) == false);
    assert(cb.empty() == false);
    assert(cb.size() == 3);
    assert(bytesWritten == sizeof(valW));
    assert(cb.read((void*)&valR, sizeof(valR), bytesRead) == true);
    assert(bytesRead == sizeof(valR));
    assert(valR == 1);
    assert(cb.empty() == false);
    assert(cb.size() == 2);
    assert(cb.read((void*)&valR, sizeof(valR), bytesRead) == true);
    assert(bytesRead == sizeof(valR));
    assert(valR == 2);
    assert(cb.empty() == false);
    assert(cb.size() == 1);
    assert(cb.read((void*)&valR, sizeof(valR), bytesRead) == true);
    assert(bytesRead == sizeof(valR));
    assert(valR == 3);
    // empty state
    assert(cb.empty() == true);
    assert(cb.size() == 0);
    assert(cb.read(static_cast<void*>(output), outputBufferSizeInBytes, bytesRead) == false);

    // intersperse read/writes
    // multi byte read/read
    // multi byte read/write interspersed
}

void testCircularBufferYesOverwriteWhenFull()
{
    // todo
    cout << "testCircularBufferYesOverwriteWhenFull() --- TODO ---" << endl;
}

int main()
{
    testCircularBufferNoOverwriteWhenFullSingleByte();
    testCircularBufferNoOverwriteWhenFullMultiByte();
    testCircularBufferNoOverwriteWhenFullMultiByteWritesAndReadsInterspersed();
    testCircularBufferYesOverwriteWhenFull();

    return 0;
}

