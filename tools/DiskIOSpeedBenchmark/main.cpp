//
// Created by yuan on 8/5/19.
//

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <stdio.h>

#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <aio.h>

#include <XmlFileReader.h>
#include <PatentTagTextCollector.h>
#include "XmlPCProcessorInterface.h"
#include "StatsThread.h"


using namespace std;
namespace fs = std::filesystem;

using OutputQueueByFile = unordered_map<string, vector<string>>;

inline size_t getFileSize(const char* filename)
{
    struct stat st;
    if (stat(filename, &st) == 0)
        return st.st_size;
    return -1;
}

class DoNothingWriter : public ThreadJob<CQueue<string>&> {
    vector<string> filenames_;

    void internalRun(CQueue<string>& data) final
    {
        for (;;)
        {
            auto [filename, quit] = data.pop();

            if (quit) break;

            filenames_.push_back(filename);
        }

        cout << filenames_.size() << '\n';
    }
};


class DoNothingCollector : public ThreadJob<> {
    CQueue<string>& filenameQueue_;

    void internalRun() final
    {
        for (;;)
        {
            pugi::xml_document doc;
            auto [filename, quit] = filenameQueue_.pop();

            if (quit) break;

            pugi::xml_parse_result result = doc.load_file(filename.c_str());
        }
    }
public:
    explicit DoNothingCollector(CQueue<string>& filenameQueue) : filenameQueue_(filenameQueue) { }
};


class DoNothingReader : public ThreadJob<> {
    CQueue<string>& filenameQueue_;

    void internalRun() final
    {
        for (;;)
        {
            auto [filename, quit] = filenameQueue_.pop();
            if (quit) break;
            ifstream ifs(filename);

            stringstream buffer;
            buffer << ifs.rdbuf();

            ifs.close();
        }
    }
public:
    explicit DoNothingReader(CQueue<string>& filenameQueue) : filenameQueue_(filenameQueue) { }
};


class DoNothingSyncCReader : public ThreadJob<> {
    CQueue<string>& filenameQueue_;

    inline static constexpr size_t DEFAULT_BUFSIZE = 1024;
    size_t bufferSize_ = DEFAULT_BUFSIZE;
    char* buffer_;

    void internalRun() final
    {
        for (;;)
        {
            auto [filename, quit] = filenameQueue_.pop();
            if (quit) break;

            FILE* inputFile = fopen(filename.c_str(), "r");
            if(!inputFile) {
                cerr << "File opening failed\n";
                continue;
            }
            /* Get the number of bytes */
            fseek(inputFile, 0L, SEEK_END);
            size_t numBytes = ftell(inputFile);

            fseek(inputFile, 0L, SEEK_SET);

            if (bufferSize_ < numBytes) {
                free(buffer_);
                bufferSize_ = numBytes;
                buffer_ = (char*)malloc(bufferSize_);
            }

            fread(buffer_, sizeof(char), numBytes, inputFile);

            fclose(inputFile);
        }
    }
public:
    explicit DoNothingSyncCReader(CQueue<string>& filenameQueue) : filenameQueue_(filenameQueue)
    {
        buffer_ = (char*)malloc(bufferSize_);
    }

    ~DoNothingSyncCReader() final
    {
        free(buffer_);
    }
};


class DoNothingAsyncReader : public ThreadJob<> {
    CQueue<string>& filenameQueue_;

    inline static constexpr size_t DEFAULT_BUFSIZE = 1024;
    size_t bufferSize_ = DEFAULT_BUFSIZE;
    char* buffer_;

    void internalRun() final
    {
        for (;;)
        {
            auto [filename, quit] = filenameQueue_.pop();
            if (quit) break;

            FILE* inputFile = fopen(filename.c_str(), "r");
            if(!inputFile) {
                cerr << "File opening failed\n";
                continue;
            }

            fseek(inputFile, 0L, SEEK_END);
            size_t numBytes = ftell(inputFile);

            fseek(inputFile, 0L, SEEK_SET);

            if (bufferSize_ < numBytes) {
                free(buffer_);
                bufferSize_ = numBytes;
                buffer_ = (char*)malloc(bufferSize_);
            }

            fread(buffer_, sizeof(char), numBytes, inputFile);

            fclose(inputFile);
        }
    }
public:
    explicit DoNothingAsyncReader(CQueue<string>& filenameQueue) :
        filenameQueue_(filenameQueue)
    {
        buffer_ = (char*)malloc(bufferSize_);
    }

    ~DoNothingAsyncReader() final
    {
        free(buffer_);
    }
};

class FileSizeReaderWithFopen : public ThreadJob<> {
    CQueue<string>& filenameQueue_;


    void internalRun() final
    {
        for (;;)
        {
            auto [filename, quit] = filenameQueue_.pop();
            if (quit) break;

            FILE* inputFile = fopen(filename.c_str(), "r");
            if(!inputFile) {
                cerr << "File opening failed\n";
                continue;
            }

            fseek(inputFile, 0L, SEEK_END);
            size_t numBytes = ftell(inputFile);

            fseek(inputFile, 0L, SEEK_SET);

            fclose(inputFile);
        }
    }
public:
    explicit FileSizeReaderWithFopen(CQueue<string>& filenameQueue) :
            filenameQueue_(filenameQueue)
    {
    }

    ~FileSizeReaderWithFopen() final
    {
    }
};


class FileSizeReaderWithStat : public ThreadJob<> {
    CQueue<string>& filenameQueue_;


    void internalRun() final
    {
        for (;;)
        {
            auto [filename, quit] = filenameQueue_.pop();
            if (quit) break;

            size_t fileSize = getFileSize(filename.c_str());
        }
    }
public:
    explicit FileSizeReaderWithStat(CQueue<string>& filenameQueue) :
            filenameQueue_(filenameQueue)
    {
    }
};


class DoNothingWithRead : public ThreadJob<> {
    CQueue<string>& filenameQueue_;

    inline static constexpr size_t DEFAULT_BUFSIZE = 1024;

    size_t bufferSize_;
    char* buffer_;
    aiocb cb_;

    void resetAiocb()
    {
        memset(&cb_, 0, sizeof(aiocb));
    }

    void internalRun() final
    {
        for (;;)
        {
            auto [filename, quit] = filenameQueue_.pop();
            if (quit) break;

            size_t fileSize = getFileSize(filename.c_str());

            if (bufferSize_ < fileSize) {
                bufferSize_ = fileSize * 2;
                delete[] buffer_;
                buffer_ = new char[bufferSize_];
            }

            int fd = open(filename.c_str(), O_RDONLY);
            if (fd == -1) {
                fprintf(stderr, "Cannot open [%s]\n", filename.c_str());
                continue;
            }

            resetAiocb();
            cb_.aio_nbytes = fileSize;
            cb_.aio_fildes = fd;
            cb_.aio_offset = 0;
            cb_.aio_buf = buffer_;

            if (aio_read(&cb_) == -1) {
                fprintf(stderr, "failed to create aio_read request for [%s]\n", filename.c_str());
                continue;
            }

            close(fd);
        }
    }
public:
    explicit DoNothingWithRead(CQueue<string>& filenameQueue) :
        filenameQueue_(filenameQueue), bufferSize_(DEFAULT_BUFSIZE),
        buffer_(new char[bufferSize_]) { resetAiocb(); }

    ~DoNothingWithRead() final
    {
        delete[] buffer_;
    }
};


class DiskIOSpeedBenchmark : public XmlPCProcessorInterface {
    string pathFilename1_;
    string pathFilename2_;

    CQueue<string> filenameQueue1_, filenameQueue2_;

    void prepareNodeFilters() final { }
    void prepareOutputFormatters() final { }
    void initializeData() final
    {
        XmlFileReader xmlFileReader(pathFilename1_, filenameQueue1_);
        XmlFileReader xmlFileReader2(pathFilename2_, filenameQueue2_);
        xmlFileReader.runOnMain();
        xmlFileReader2.runOnMain();
    }

    void initializeThreads() final
    {
        for (int i = 0; i < nProducers_; i++) {
            producers_.add<DoNothingReader>(filenameQueue1_);
            producers_.add<DoNothingReader>(filenameQueue2_);
        }
    }

    void executeThreads() final
    {
        producers_.runAll();
        producers_.waitAll();
    }

public:
    DiskIOSpeedBenchmark(string_view pathFilename1, string_view pathFilename2, int nFileReaders) :
        pathFilename1_(pathFilename1), pathFilename2_(pathFilename2)
    { nProducers_ = nFileReaders; }
};

void printUsageAndExit(const char* program)
{
    printf("Usage:\n\t\t%s <path-file-1> <path-file-2> <num-threads>\n", program);
    exit(-1);
}

/*
int main(int argc, char* argv[])
{
    if (argc != 4)
        printUsageAndExit(argv[0]);

    DiskIOSpeedBenchmark diskIoSpeedBenchmark(argv[1], argv[2], atoi(argv[3]));

    diskIoSpeedBenchmark.process();

    return 0;
}
*/

class PathFinder : public ThreadJob<CQueue<string>&> {

    vector<fs::path> pathRoots_;
    size_t batchSize_;

    void internalRun(CQueue<string>& filepathQueue) final
    {
        vector<string> fileBatch;
        fileBatch.reserve(batchSize_);

        for (const fs::path& root : pathRoots_) {
            if (!fs::exists(root))
                fprintf(stderr, "path [%s] does not exist\n", root.c_str());
            for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root)) {
                if (fs::is_regular_file(entry.path())) {
                    fileBatch.push_back(entry.path());
                }

                if (fileBatch.size() == batchSize_) {
                    filepathQueue.push(fileBatch);

                    fileBatch.clear();
                }
            }
            if (!fileBatch.empty()) {
                filepathQueue.push(fileBatch);
                fileBatch.clear();
            }
        }
        filepathQueue.setQuitSignal();
    }
public:
    PathFinder(vector<fs::path>&& pathRoots, CQueue<string>& filepathQueue,
            size_t batchSize = 128) :
        ThreadJob(filepathQueue), pathRoots_(pathRoots), batchSize_(batchSize) { }
};

class DiskIOBenchmarkWithPathFinder : public XmlPCProcessorInterface {
    string pathRoot_;

    CQueue<string> filenameQueue_;

    void prepareNodeFilters() final { }
    void prepareOutputFormatters() final { }
    void initializeData() final
    {
        PathFinder pathFinder(
                {pathRoot_ + "/2015", pathRoot_ + "/2016"}, filenameQueue_);
        pathFinder.runOnMain();
    }

    void initializeThreads() final
    {
        for (int i = 0; i < nProducers_; i++) {
            producers_.add<DoNothingReader>(filenameQueue_);
        }
    }

    void executeThreads() final
    {
        producers_.runAll();
        producers_.waitAll();
    }

public:
    DiskIOBenchmarkWithPathFinder(string_view pathRoot, int nFileReaders) :
            pathRoot_(pathRoot)
    { nProducers_ = nFileReaders; }
};


class DiskIOBenchmarkWithCRead : public XmlPCProcessorInterface {
    string pathFilename_;

    CQueue<string> filenameQueue_;

    void prepareNodeFilters() final { }
    void prepareOutputFormatters() final { }
    void initializeData() final
    {
        XmlFileReader xmlFileReader(pathFilename_, filenameQueue_);

        xmlFileReader.runOnMain();
    }

    void initializeThreads() final
    {
        for (int i = 0; i < nProducers_; i++) {
            producers_.add<DoNothingWithRead>(filenameQueue_);
        }
    }

    void executeThreads() final
    {
        StatsThread<string, false> statsThread(filenameQueue_);
        producers_.runAll();
        statsThread.run();

        producers_.waitAll();
        statsThread.wait();
    }

public:
    DiskIOBenchmarkWithCRead(string_view pathFilename, int nFileReaders) :
            pathFilename_(pathFilename)
    { nProducers_ = nFileReaders; }
};


void printUsageAndExit2(const char* program)
{
    printf("Usage:\n\t\t%s <root-path> <num-threads>\n", program);
    exit(-1);
}

void singleLargeFileFreadBenchmark(const char* filename)
{
    FILE* inputFile = fopen(filename, "r");
    if(!inputFile) {
        cerr << "File opening failed\n";
        return;
    }
    /* Get the number of bytes */
    fseek(inputFile, 0L, SEEK_END);
    size_t numBytes = ftell(inputFile);

    fseek(inputFile, 0L, SEEK_SET);

    char* buffer = (char*)malloc(numBytes);

    fread(buffer, sizeof(char), numBytes, inputFile);

    fclose(inputFile);
}


void singleLargeFileReadBenchmark(const char* filename)
{
    size_t size = getFileSize(filename);

    cout << size << '\n';

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Cannot open [%s]\n", filename);
        exit(-1);
    }

    char* buffer = new char[size];

    size_t sizeRead;
    for (sizeRead = read(fd, buffer, size); sizeRead != 0; sizeRead = read(fd, buffer, size))
        cout << sizeRead << '\n';

    cout << sizeRead << '\n';

    close(fd);

    delete[] buffer;
}

void singleLargeFileAioReadBenchmark(const char* filename)
{
    size_t size = getFileSize(filename);

    cout << size << '\n';

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Cannot open [%s]\n", filename);
        exit(-1);
    }

    char* buffer = new char[size];

    aiocb cb;
    memset(&cb, 0, sizeof(aiocb));
    cb.aio_nbytes = size;
    cb.aio_fildes = fd;
    cb.aio_offset = 0;
    cb.aio_buf = buffer;

    if (aio_read(&cb) == -1) {
        fprintf(stderr, "aio_read failed on [%s]\n", filename);
        exit(-1);
    }

    while (aio_error(&cb) == EINPROGRESS);

//        cout << "waiting...\n";

    if (size_t sizeRead = aio_return(&cb); sizeRead != (size_t)-1)
        printf("Success: read %zu\n", sizeRead);
    else
        cout << "Failed\n";

    close(fd);

    delete[] buffer;
}


int main(int argc, char* argv[])
{
/*
    if (argc != 3)
        printUsageAndExit2(argv[0]);

    DiskIOBenchmarkWithCRead diskIoBenchmarkWithCRead(argv[1], atoi(argv[2]));

    diskIoBenchmarkWithCRead.process();
*/
    singleLargeFileAioReadBenchmark(argv[1]);

    return 0;
}