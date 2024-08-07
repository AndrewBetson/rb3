#ifndef OS_ARKFILE_H
#define OS_ARKFILE_H
#include "os/File.h"
#include <revolution/DVD.h>

class ArkFile : public File {
public:
    ArkFile(const char *, int);
    virtual ~ArkFile();
    virtual String Filename() const { return String(mFilename); }
    virtual int Read(void *, int);
    virtual bool ReadAsync(void *, int);
    virtual int Write(const void *, int);
    virtual int Seek(int, int);
    virtual int Tell();
    virtual void Flush(){}
    virtual bool Eof();
    virtual bool Fail();
    virtual int Size();
    virtual int UncompressedSize();
    virtual bool ReadDone(int &);
    virtual int GetFileHandle(DVDFileInfo*&);

    void TaskDone(int);

    void operator delete(void* v){
        _MemFree(v);
    }

    int mArkfileNum;
    unsigned long long mByteStart;
    int mSize;
    int mUCSize;
    int mNumOutstandingTasks;
    int mBytesRead;
    int mTell;
    int mFail;
    float mReadStartTime;
    bool mReadAhead;
    String mFilename;
};

#endif
