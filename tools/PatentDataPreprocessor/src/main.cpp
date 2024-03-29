//
// Created by yuan on 7/19/19.
//
#include <iostream>
#include <string>
#include <stdio.h>

#include "ThreadJob.h"
#include "ThreadPool.h"

#include "StatsThread.h"
#include "XmlPathFileReader.h"
#include "PatentInfoCollector.h"
#include "XmlInfoWriter.h"
#include "PatentInfoPC.h"

#include "FormatFunctors.h"
#include "TagConstants.h"
#include "PatentTagTextCollector.h"
#include "TagNodeFilterFunctors.h"

using namespace std;


void printUsageAndExit(const char* program)
{
    printf("Usage:\n\t\t%s <path-file> <tag-abstract-output-file> "
           "<split-abstract-output-file> <num-threads>\n", program);
    exit(-1);
}


void start(int argc, char* argv[])
{
    if (argc != 5)
        printUsageAndExit(argv[0]);

    int nThreads = atoi(argv[4]);

    TagNodeFilterDict tagNodeFilterDict;

    tagNodeFilterDict.add<ClassificationNodeFilter>(tags::classification);
    tagNodeFilterDict.add<AbstractGreedyNodeFilter>(tags::abstract);

    TagTextOutputFormatterDict tagTextOutputFormatterDict;

    tagTextOutputFormatterDict.add<IdClassAbstractFileOutput>(argv[2]);
    tagTextOutputFormatterDict.add<SplitAbstractFileOutput>(argv[3]);

    CQueue<string> filenameQueue;
    unordered_map<string, CQueue<string>> outputQueueByFile;

    /* collect file paths */
    XmlPathFileReader xmlFileReader(argv[1], filenameQueue);
    xmlFileReader.runOnMain();

    /* construct outputQueues in-place */
    outputQueueByFile.emplace(piecewise_construct, make_tuple(argv[2]), make_tuple());
    outputQueueByFile.emplace(piecewise_construct, make_tuple(argv[3]), make_tuple());

    ThreadPool producers, consumers;
    for (int i = 0; i < nThreads; i++)
        producers.add<PatentTagTextCollector>(filenameQueue, outputQueueByFile,
                tagTextOutputFormatterDict, tagNodeFilterDict);

    for (auto& [filename, outputQueue] : outputQueueByFile)
        consumers.add<XmlInfoWriter>(filename, outputQueue);

    StatsThread<string, true> writeStats(outputQueueByFile[argv[2]], filenameQueue.totalPushedItems());

    producers.runAll();
    consumers.runAll();
    writeStats.run();

    producers.waitAll();
    for (auto& [filename, outputQueue] : outputQueueByFile)
        outputQueue.setQuitSignal();

    consumers.waitAll();
    writeStats.wait();
}


int main(int argc, char* argv[])
{

    start(argc, argv);

    return 0;
}
