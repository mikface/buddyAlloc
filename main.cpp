#ifndef __PROGTEST__

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <vector>

using namespace std;
#endif /* __PROGTEST__ */

#define SMALLEST_BLOCK_SIZE 64
struct HeapInfo {
    void *startBlock;
    void *serviceMemStartBlock;
    void *mainMemStartBlock;
    void *mainMemEndBlock;
    unsigned blockCount;
    unsigned servMemSize;
    unsigned mainMemSize;
    unsigned smallestBlockSize;
    unsigned allocBlockCount;
};

HeapInfo hInfo{
        nullptr, 0
};

struct BlockHeader {
    unsigned allocTableIndex;
    void *id;

    BlockHeader(unsigned allocTableIndex, void *id) : allocTableIndex(allocTableIndex), id(id) {

    }
};

struct ServiceBlock {
    bool isFree;
    bool isFull;
};

void setBlockFree(unsigned index, bool isFree) {
    ((ServiceBlock *) hInfo.serviceMemStartBlock + index)->isFree = isFree;
}

void setBlockFull(unsigned index, bool isFull) {
    ((ServiceBlock *) hInfo.serviceMemStartBlock + index)->isFull = isFull;
}

bool isBlockFree(unsigned index) {
    return ((ServiceBlock *) hInfo.serviceMemStartBlock + index)->isFree;
}


bool isBlockFull(unsigned index) {
    return ((ServiceBlock *) hInfo.serviceMemStartBlock + index)->isFull;
}


void *HeapAllocRec(unsigned index, unsigned size, bool isServiceBlock) {
    if (isBlockFull(index)) {
        return nullptr;
    }
    bool blockIsFree = isBlockFree(index);
    auto modulo = (unsigned) floor(log2(index + 1));
    unsigned halfBlockSize = hInfo.mainMemSize / (unsigned) (pow(2, modulo + 1));
    unsigned fullBlockSize = halfBlockSize * 2;
    if (!isServiceBlock) {
        halfBlockSize -= (sizeof(BlockHeader) / 2);
    }
    unsigned blockSize = halfBlockSize * 2;
    unsigned leftChildIndex = (index + 1) * 2 - 1;
    unsigned rightChildIndex = (index + 1) * 2;

    // NO SPACE IN BLOCK
    if (((size + sizeof(BlockHeader)) > halfBlockSize && !isServiceBlock) || size > halfBlockSize ||
        blockSize <= hInfo.smallestBlockSize) {
        if (!blockIsFree) {
            return nullptr;
        }
        setBlockFree(index, false);
        setBlockFull(index, true);
        unsigned indexInBinaryTreeLine = (index + 1) % ((unsigned) pow(2, modulo));
        ++hInfo.allocBlockCount;
        if (blockSize > hInfo.smallestBlockSize) {
            setBlockFree(leftChildIndex, false);
            setBlockFree(rightChildIndex, false);
        }
        uint8_t *returnPointer = (uint8_t *) hInfo.mainMemStartBlock + (indexInBinaryTreeLine * fullBlockSize);
        if (!isServiceBlock) {
            BlockHeader header{index, (returnPointer + sizeof(BlockHeader))};
            *((BlockHeader *) ((uint8_t *) hInfo.mainMemStartBlock + (indexInBinaryTreeLine * fullBlockSize))) = header;
            returnPointer += sizeof(BlockHeader);
        }
        return returnPointer;
    }

    // SPLITTING ONE BIG BLOCK IN TWO PARTS
    if (blockIsFree) {
        setBlockFree(leftChildIndex, true);
        setBlockFull(leftChildIndex, false);
        setBlockFree(rightChildIndex, true);
        setBlockFull(rightChildIndex, false);
    }
    void *childResult = HeapAllocRec(leftChildIndex, size, isServiceBlock);
    // LEFT CHILD FULL
    if (childResult == nullptr) {
        childResult = HeapAllocRec(rightChildIndex, size, isServiceBlock);
    }

    if (childResult != nullptr) {
        // CHILD IS FREE, THEREFORE PARENT IS NOT FULLY FREE FROM NOW ON
        setBlockFree(index, false);
        if (isBlockFull(leftChildIndex) && isBlockFull(rightChildIndex)) {
            setBlockFull(index, true);
        }
    }
    return childResult;
}

void *HeapAlloc(int size) {
    if (size <= 0 || (unsigned) size > hInfo.mainMemSize) {
        return nullptr;
    }
    return HeapAllocRec(0, (unsigned) size, false);
}

unsigned allocExtraSize(unsigned extra) {
    unsigned  chunk, reallyAllocated = 0;
    while (extra > hInfo.servMemSize) {
        chunk = (unsigned) pow(2, ((unsigned) floor(log2(extra))));
        HeapAllocRec(0, chunk, true);
        extra -= chunk;
        hInfo.allocBlockCount--;
        reallyAllocated+=chunk;
    }
    return reallyAllocated;
}

void HeapInit(void *memPool, int memSize) {
    hInfo.startBlock = memPool;
    hInfo.serviceMemStartBlock = memPool;
    hInfo.mainMemStartBlock = memPool;
    hInfo.smallestBlockSize = SMALLEST_BLOCK_SIZE;
    hInfo.mainMemSize = (unsigned) pow(2, ((unsigned) ceil(log2(memSize))));
    hInfo.mainMemEndBlock = (uint8_t *) hInfo.mainMemStartBlock + hInfo.mainMemSize;
    hInfo.blockCount = hInfo.mainMemSize / SMALLEST_BLOCK_SIZE;
    hInfo.servMemSize = hInfo.blockCount * sizeof(ServiceBlock) * 2;
    hInfo.allocBlockCount = 0;

    // SET ROOT BLOCK FREE
    unsigned extra = hInfo.mainMemSize - memSize;
    setBlockFree(0, true);
    setBlockFull(0, false);
    hInfo.allocBlockCount --;
    unsigned mainMemShift = allocExtraSize(extra);
    hInfo.mainMemStartBlock = (uint8_t *) hInfo.mainMemStartBlock - mainMemShift;
    hInfo.mainMemEndBlock = (uint8_t *) hInfo.mainMemEndBlock - mainMemShift;
    hInfo.serviceMemStartBlock = HeapAllocRec(0, hInfo.servMemSize, true);
}

bool parentIsNotFullRec(int myIndex) {
    if (myIndex == 0) {
        return true;
    }
    auto parentIndex = (unsigned) floor((myIndex - 1) / 2);
    setBlockFull(parentIndex, false);
    return parentIsNotFullRec(parentIndex);
}

bool HeapFreeRec(unsigned myIndex) {
    if (myIndex == 0) {
        return true;
    }
    unsigned siblingIndex, parentIndex;
    myIndex % 2 == 0 ? siblingIndex = myIndex - 1 : siblingIndex = myIndex + 1;
    parentIndex = (unsigned) floor((myIndex - 1) / 2);

    // IF SIBLING FREE MERGE AND FREE PARENT
    if (isBlockFree(siblingIndex)) {
        setBlockFree(myIndex, false);
        setBlockFull(myIndex, false);
        setBlockFree(siblingIndex, false);
        setBlockFull(siblingIndex, false);
        setBlockFree(parentIndex, true);
        setBlockFull(parentIndex, false);
        return HeapFreeRec(parentIndex);
    }
    // FREE JUST ME
    setBlockFree(myIndex, true);
    setBlockFull(myIndex, false);
    setBlockFull(parentIndex, false);
    return parentIsNotFullRec(parentIndex);
}


bool HeapFree(void *blk) {
    BlockHeader *servicePtr = (BlockHeader *) blk - 1;
    if (servicePtr < hInfo.mainMemStartBlock || blk >= hInfo.mainMemEndBlock ||
        ((BlockHeader *) blk - 1)->id != blk) {
        return false;
    }
//    cout << "FREE BEGINS" << endl;

    int myIndex = ((BlockHeader *) blk - 1)->allocTableIndex;
    --hInfo.allocBlockCount;
    return HeapFreeRec(myIndex);
}

void HeapDone(int *pendingBlk) {
    *pendingBlk = hInfo.allocBlockCount;
    hInfo.allocBlockCount = 0;
    hInfo.mainMemSize = 0;
    hInfo.servMemSize = 0;
    hInfo.startBlock = nullptr;
    hInfo.serviceMemStartBlock = nullptr;
    hInfo.mainMemStartBlock = nullptr;
    hInfo.smallestBlockSize = 0;
    hInfo.mainMemSize = 0;
    hInfo.blockCount = 0;
    hInfo.servMemSize = 0;
}

#ifndef __PROGTEST__

int main(void) {
    uint8_t *p0, *p1, *p2, *p3, *p4;
    int pendingBlk;
    static uint8_t memPool[3 * 1048576];

    HeapInit(memPool, 2097152);
    assert ((p0 = (uint8_t *) HeapAlloc(512000)) != NULL);
    memset(p0, 0, 512000);
    assert ((p1 = (uint8_t *) HeapAlloc(511000)) != NULL);
    memset(p1, 0, 511000);
    assert ((p2 = (uint8_t *) HeapAlloc(26000)) != NULL);
    memset(p2, 0, 26000);
    HeapDone(&pendingBlk);
    assert (pendingBlk == 3);


    HeapInit(memPool, 2097152);
    assert ((p0 = (uint8_t *) HeapAlloc(1000000)) != NULL);
    memset(p0, 0, 1000000);
    assert ((p1 = (uint8_t *) HeapAlloc(250000)) != NULL);
    memset(p1, 0, 250000);
    assert ((p2 = (uint8_t *) HeapAlloc(250000)) != NULL);
    memset(p2, 0, 250000);
    assert ((p3 = (uint8_t *) HeapAlloc(250000)) != NULL);
    memset(p3, 0, 250000);
    assert ((p4 = (uint8_t *) HeapAlloc(50000)) != NULL);
    memset(p4, 0, 50000);
    assert (HeapFree(p2));
    assert (HeapFree(p4));
    assert (HeapFree(p3));
    assert (HeapFree(p1));
    assert ((p1 = (uint8_t *) HeapAlloc(500000)) != NULL);
    memset(p1, 0, 500000);
    assert (HeapFree(p0));
    assert (HeapFree(p1));
    HeapDone(&pendingBlk);
    assert (pendingBlk == 0);


    HeapInit(memPool, 2359296);
    assert ((p0 = (uint8_t *) HeapAlloc(1000000)) != NULL);
    memset(p0, 0, 1000000);
    assert ((p1 = (uint8_t *) HeapAlloc(500000)) != NULL);
    memset(p1, 0, 500000);
    assert ((p2 = (uint8_t *) HeapAlloc(500000)) != NULL);
    memset(p2, 0, 500000);
    assert ((p3 = (uint8_t *) HeapAlloc(500000)) == NULL);
    assert (HeapFree(p2));
    assert ((p2 = (uint8_t *) HeapAlloc(300000)) != NULL);
    memset(p2, 0, 300000);
    assert (HeapFree(p0));
    assert (HeapFree(p1));
    HeapDone(&pendingBlk);
    assert (pendingBlk == 1);


    HeapInit(memPool, 2359296);
    assert ((p0 = (uint8_t *) HeapAlloc(1000000)) != NULL);
    memset(p0, 0, 1000000);
    assert (!HeapFree(p0 + 1000));
    HeapDone(&pendingBlk);
    assert (pendingBlk == 1);


    return 0;
}

#endif /* __PROGTEST__ */

