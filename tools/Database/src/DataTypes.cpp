//
// Created by yuan on 8/13/19.
//

#include "DataTypes.h"

#include <Utility.h>
#include <TagConstants.h>

#include <fstream>

using namespace std;

void DataRecordFile::clear()
{
    /* atomically obtains file id */
    binId_ = nextBinId_.fetch_add(1);

    memset(buf_, 0, MAX_FILE_SIZE);
    curBuf_ = buf_ + FILE_HEAD_SIZE;
    nBytesWritten_ = FILE_HEAD_SIZE;
    nRecordsWritten_ = 0;

    for (IndexValue* iv : indexSubTable_)
        delete iv;
    indexSubTable_.clear();
}

void DataRecordFile::writeToFile(const char* filename)
{
    FILE* filePtr = fopen(filename, "wb");
    if (!filePtr) {
        fprintf(stderr, "failed to open file [%s]\n", filename);
        PERROR("fopen()");
    }

    fwrite(buf_, nBytesWritten_, 1, filePtr);

    fclose(filePtr);
}

bool DataRecordFile::appendRecord(const vector<string>& dataText,
        const vector<string>& indexText,
        const uint32_t* recordSize)
{
    uint32_t rsz;
    if (recordSize == nullptr) {
        rsz = sizeof(uint32_t) + sizeof(uint32_t) * dataText.size();
        for (const auto& t : dataText)
            rsz += t.length();
    }
    else
        rsz = *recordSize;

    if (bytesWritten() + rsz > capacity())
        return false;

    /* create new IndexValue */
    auto iv = new IndexValue;

    // TODO: hard coded check
    if (indexText.size() != HARD_CODED_INDEX_FIELDS)
        PERROR("hard coded IndexValue doesn't match");
    if (dataText.size() != HARD_CODED_DATA_FIELDS)
        PERROR("hard coded IndexValue doesn't match");

    // TODO: change this, hard-coded for now
    iv->pid = indexText[0];
    iv->aid = indexText[1];
    iv->appDate = indexText[2];
    iv->ipc = indexText[3];
    iv->binId = binId_;
    iv->offset = curBuf_ - buf_;
    iv->ti = sizeof(uint32_t);
    iv->ai = iv->ti + dataText[0].length() + sizeof(uint32_t);
    iv->ci = iv->ai + dataText[1].length() + sizeof(uint32_t);
    iv->di = iv->ci + dataText[2].length() + sizeof(uint32_t);

    /* append to Data File (buf_)*/
    *(uint32_t*)curBuf_ = rsz;
    incrementBy(sizeof(uint32_t));
    for (const auto& t : dataText) {
        *(uint32_t*) curBuf_ = t.length();
        incrementBy(sizeof(uint32_t));
        memcpy(curBuf_, t.c_str(), t.length());
        incrementBy(t.length());
    }
    /* update records written */
    nRecordsWritten_++;

    /* append to index subtable */
    indexSubTable_.push_back(iv);

    return true;
}

void DataRecordFile::readFromFile(const char* filename)
{
    clear();

    FILE* filePtr = fopen(filename, "rb");
    if (!filePtr) {
        fprintf(stderr, "failed to open file [%s]\n", filename);
        PERROR("fopen()");
    }

    /* interpret how many bytes are in the file */
    if (fread(buf_, sizeof(decltype(nBytesWritten_)), 1, filePtr) != 1)
        PERROR("fread() file size");

    /* read the */
    if (fread(buf_ + sizeof(decltype(nBytesWritten_)),
            nBytesWritten_ - sizeof(decltype(nBytesWritten_)), 1, filePtr) != 1)
        PERROR("fread()");

    printf("read %lu bytes\n", nBytesWritten_);

    fclose(filePtr);
}


void DataRecordFile::writeSubIndexTableToFile(const char* filename)
{
    ofstream ofs(filename);
    for (IndexValue* iv : indexSubTable_) {
        ofs << *iv << '\n';
    }
}

void DataRecordFile::writeSubIndexTableToStream(std::ostream& os)
{
    for (IndexValue* iv : indexSubTable_) {
        os << *iv << '\n';
    }
}

bool DataRecordFile::GetDataRecordAtOffset(uint64_t offset, DataRecord* dataRecord) const
{
    if (offset > nBytesWritten_) {
        fprintf(stderr, "%s: offset %lu exceeds file [prefix_%u.bin] with %lu B\n",
                __FUNCTION__, offset, binId_, nBytesWritten_);
        return false;
    }

/* to check if an increment of the pointer exceeds the boundary of the file */
#define INCR_AND_CHECK(sz)                                                          \
    do {                                                                            \
        curBuf += sz;                                                               \
        if ((uint64_t)(curBuf - buf_) > nBytesWritten_) {                           \
        fprintf(stderr, "%s: offset %lu exceeds file [prefix_%u.bin] with %lu B\n", \
                    __FUNCTION__, curBuf - buf_, binId_, nBytesWritten_);           \
            return false;                                                           \
        }                                                                           \
    } while (0)

    const char* curBuf = buf_ + offset;
    auto* size = (uint32_t*)curBuf;
    INCR_AND_CHECK(sizeof(uint32_t));

#define GET_SIZE_AND_STR(sz, str)           \
    uint32_t* sz;                           \
    const char* str;                        \
    do {                                    \
        sz = (uint32_t*)curBuf;             \
        INCR_AND_CHECK(sizeof(uint32_t));   \
        str = curBuf;                       \
        INCR_AND_CHECK(*sz);                \
    } while(0)

    GET_SIZE_AND_STR(ts, title);
    GET_SIZE_AND_STR(as, abstract);
    GET_SIZE_AND_STR(cs, claim);
    GET_SIZE_AND_STR(ds, description);

    *dataRecord = DataRecord(*size, *ts, *as, *cs, *ds, title, abstract, claim, description);

#undef GET_SIZE_AND_STR
#undef INCR_AND_CHECK

    return true;
}


void IndexTable::readFromFile(const char* filename)
{
    if (!valList_.empty())
        PERROR("valList must be empty before reading from file");

    ifstream ifs(filename);
    if (!ifs.is_open()) {
        fprintf(stderr, "file [%s] failed to open\n", filename);
        PERROR("ifstream open");
    }

    string line;
    getline(ifs, line); // discard the first line as it is a title

    for (; getline(ifs, line);) {
        istringstream ss(line);
        auto* iv = new IndexValue;
        ss >> *iv;

        /* keep track of all IndexValues to be released later */
        valList_.push_back(iv);
        /* hash IndexValues by their binID for easier access to data files */
        indexByBinId_[iv->binId].push_back(iv);

        insertValueToTables(iv);
    }
}

void IndexTable::writeToFile(const char* filename)
{
    ofstream ofs((string(filename)));
    if (!ofs.is_open()) {
        fprintf(stderr, "file [%s] failed to open\n", filename);
        PERROR("ofstream file open");
    }

    for (IndexValue* val : valList_)
        ofs << *val << '\n';

    ofs.close();
}

void IndexTable::reserve(size_t n)
{
    valList_.reserve(n);
    for (auto& [_, table] : indexTables_)
        table.reserve(n);
}

ostream& operator<<(std::ostream& os, const IndexValue& ie)
{
    os << ie.stringify();

    return os;
}

istream& operator>>(std::istream& is, IndexValue& ie)
{
    // WARNING: It is presumed there is a SINGLE tab delimiting here!!!
    string field;

    getline(is, ie.pid, '\t');
    getline(is, ie.aid, '\t');
    getline(is, ie.appDate, '\t');
    getline(is, ie.ipc, '\t');

/* helper macro for repetitive code */
#define GET_UINT32(name)            \
    do {                            \
        getline(is, field, '\t');   \
        ie.name = stoi(field);      \
    } while(0)

    GET_UINT32(binId);
    GET_UINT32(offset);
    GET_UINT32(ti);
    GET_UINT32(ai);
    GET_UINT32(ci);
    GET_UINT32(di);

    return is;
}

#undef GET_UINT32

std::ostream& operator<<(std::ostream& os, const DataRecord& dataRecord)
{
    /* empty if any of them is null */
    if (!dataRecord.title || !dataRecord.abstract ||
            !dataRecord.claim || !dataRecord.description) {
        os << "empty data record";
        return os;
    }

    os << "title: " << *dataRecord.title << '\n' <<
        "abstract: " << *dataRecord.abstract << '\n' <<
        "claim: " << *dataRecord.claim << '\n' <<
        "description: " << *dataRecord.description;

    return os;
}
