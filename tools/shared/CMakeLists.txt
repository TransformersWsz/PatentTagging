cmake_minimum_required(VERSION 3.10)
project(XmlSharedTools)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")


add_library(XmlSharedTools STATIC src/PatentTagTextCollector.cpp src/XmlPathFileReader.cpp src/XmlTagTextWalker.cpp src/FormatFunctors.cpp src/TagNodeFilterFunctors.cpp src/XmlInfoWriter.cpp src/XmlPCProcessorTester.cpp include/XmlFilterOutputIPOInterface.h src/XmlIPOTagTextPrinterTester.cpp include/XmlIPOTagTextPrinterTester.h src/XmlTagTextPrinterProcessorThread.cpp include/XmlTagTextPrinterProcessorThread.h src/XmlReaderThread.cpp include/XmlReaderThread.h src/XmlTagTextPrinterWriterThread.cpp include/XmlTagTextPrinterWriterThread.h src/XmlIOReaderThread.cpp include/XmlIOReaderThread.h include/XpathQueryString.h src/XpathQueryTextFormatter.cpp include/XpathQueryTextFormatter.h include/XpathQuery.h src/XmlTagTextPrinterProcessorXpathThread.cpp include/XmlTagTextPrinterProcessorXpathThread.h include/XmlXpathIPOInterface.h include/XmlXpathIPOTagTextPrinterTester.h include/StaticQueue.h include/ConcurrentStaticQueue.h src/FileReaderThread.cpp include/FileReaderThread.h src/XmlBufferTagTextPrinterProcessorThread.cpp include/XmlBufferTagTextPrinterProcessorThread.h include/XmlBufferXpathIPOTagTextPrinterTester.h)

target_include_directories(XmlSharedTools PRIVATE
        ${CMAKE_SOURCE_DIR}/cpptqdm
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

target_link_libraries(XmlSharedTools pugixml pthread stdc++fs)
