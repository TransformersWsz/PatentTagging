//
// Created by yuan on 7/4/19.
//

#include "PatentTagCollector.h"

#include <iostream>
#include <stdio.h>

void PatentTagCollector::internalRun(ConcurrentQueue<std::string>& filenameQueue)
{
    for (;;)
    {
        // consume object from queue
        std::cout << "thread " << thread_.get_id() << " started\n";
        auto [filename, quit] = filenameQueue.pop();

        std::cout << "got element: " << filename << "quit: " << quit << "\n";
        if (quit) break;

        pugi::xml_parse_result result = doc_.load_file(filename.c_str());
        if (!result) {
            errorFiles_.push_back(filename);
            continue;
        }
        walker_.curFilename = filename.c_str();
        walker_.isIrregular = false;
        doc_.traverse(walker_);
        if (walker_.isIrregular)
            errorFiles_.push_back(filename);
    }
    std::cout << "thread " << thread_.get_id() << " finished\n";
    for (const auto& tag : walker_.uniqueTags)
        std::cout << tag << "\n";
}

void PatentTagCollector::run(ConcurrentQueue<std::string>& filenameQueue)
{
    thread_ = std::thread(&PatentTagCollector::internalRun, this, std::ref(filenameQueue));
    threadStarted_ = true;
}
