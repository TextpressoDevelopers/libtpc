cmake_minimum_required(VERSION 3.5)
project(libtpc)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake-modules/")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON) #...is required...
set(CMAKE_CXX_FLAGS "-DBOOST_NO_CXX11_SCOPED_ENUMS -w")

set(CMAKE_STATIC_LIBRARY_PREFIX "") # avoid the prefix "lib"
set(CMAKE_SHARED_LIBRARY_PREFIX "")

SET(CMAKE_INSTALL_PREFIX /usr/local)

SET(CMAKE_SKIP_BUILD_RPATH  FALSE)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

find_package(APR REQUIRED)
include_directories(${APR_INCLUDE_DIR})

find_package(CImg REQUIRED)
include_directories(${CImg_INCLUDE_DIRS})

find_package(PythonLibs REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

enable_testing()
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

file(GLOB CAS_GENERATORS_FILES "cas-generators/*.h" "cas-generators/*.cpp" "cas-generators/pdf2tpcas/*.cpp"
        "cas-generators/pdf2tpcas/*.h" "cas-generators/xml2tpcas/*.cpp" "cas-generators/xml2tpcas/*.h")

set(SOURCE_FILES Utils.h Utils.cpp lucene-custom/CaseSensitiveAnalyzer.h
        lucene-custom/CaseSensitiveAnalyzer.cpp uima-custom-analyzers/Tpcas2SingleIndex/CASUtils.h
        uima-custom-analyzers/Tpcas2SingleIndex/CASUtils.cpp DataStructures.cpp DataStructures.h)
add_library(libtextpresso SHARED ${SOURCE_FILES} IndexManager.h IndexManager.cpp CASManager.h CASManager.cpp
        cas-generators/pdf2tpcas/ElementCluster.cpp
        cas-generators/pdf2tpcas/ElementCluster.h cas-generators/pdf2tpcas/PdfInfo.cpp
        cas-generators/pdf2tpcas/PdfInfo.h cas-generators/pdf2tpcas/PdfMyFontInfo.h
        cas-generators/pdf2tpcas/PdfMyFontInfo.cpp cas-generators/Stream2Tpcas.h
        cas-generators/Stream2Tpcas.cpp cas-generators/pdf2tpcas/TextElement.cpp
        cas-generators/pdf2tpcas/TextElement.h cas-generators/xml2tpcas/pugiconfig.hpp
        cas-generators/xml2tpcas/pugixml.cpp cas-generators/xml2tpcas/pugixml.hpp
        cas-generators/xml2tpcas/PugiXml2Tpcas.cpp cas-generators/xml2tpcas/PugiXml2Tpcas.h
        cas-generators/xml2tpcas/ReadXml2Stream.cpp cas-generators/xml2tpcas/ReadXml2Stream.h
        cas-generators/Stream2Tpcas.cpp cas-generators/Stream2Tpcas.h)
target_link_libraries(libtextpresso lucene++ icuuc uima boost_iostreams boost_system boost_regex boost_filesystem
        boost_serialization xerces-c podofo z ${CImg_SYSTEM_LIBS} db_cxx db_stl ${PYTHON_LIBRARIES})

#add_executable(test_indexmanager ${SOURCE_FILES} IndexManager.h IndexManager.cpp CASManager.cpp CASManager.h
#        tests/test_indexmanager.cpp
#        cas-generators/pdf2tpcas/ElementCluster.cpp
#        cas-generators/pdf2tpcas/ElementCluster.h cas-generators/pdf2tpcas/PdfInfo.cpp
#        cas-generators/pdf2tpcas/PdfInfo.h cas-generators/pdf2tpcas/PdfMyFontInfo.h
#        cas-generators/pdf2tpcas/PdfMyFontInfo.cpp cas-generators/Stream2Tpcas.h
#        cas-generators/Stream2Tpcas.cpp cas-generators/pdf2tpcas/TextElement.cpp
#        cas-generators/pdf2tpcas/TextElement.h cas-generators/xml2tpcas/pugiconfig.hpp
#        cas-generators/xml2tpcas/pugixml.cpp cas-generators/xml2tpcas/pugixml.hpp
#        cas-generators/xml2tpcas/PugiXml2Tpcas.cpp cas-generators/xml2tpcas/PugiXml2Tpcas.h
#        cas-generators/xml2tpcas/ReadXml2Stream.cpp cas-generators/xml2tpcas/ReadXml2Stream.h
#        cas-generators/Stream2Tpcas.cpp cas-generators/Stream2Tpcas.h)
#target_link_libraries(test_indexmanager ${GTEST_BOTH_LIBRARIES} lucene++ pthread boost_system boost_iostreams boost_regex icuuc uima
#        boost_filesystem boost_serialization xerces-c db_cxx db_stl podofo z ${CImg_SYSTEM_LIBS} ${PYTHON_LIBRARIES})

#add_executable(test_casmanager ${SOURCE_FILES} CASManager.h CASManager.cpp tests/test_casmanager.cpp
#        cas-generators/pdf2tpcas/ElementCluster.cpp
#        cas-generators/pdf2tpcas/ElementCluster.h cas-generators/pdf2tpcas/PdfInfo.cpp
#        cas-generators/pdf2tpcas/PdfInfo.h cas-generators/pdf2tpcas/PdfMyFontInfo.h
#        cas-generators/pdf2tpcas/PdfMyFontInfo.cpp cas-generators/Stream2Tpcas.h
#        cas-generators/Stream2Tpcas.cpp cas-generators/pdf2tpcas/TextElement.cpp
#        cas-generators/pdf2tpcas/TextElement.h cas-generators/xml2tpcas/pugiconfig.hpp
#        cas-generators/xml2tpcas/pugixml.cpp cas-generators/xml2tpcas/pugixml.hpp
#        cas-generators/xml2tpcas/PugiXml2Tpcas.cpp cas-generators/xml2tpcas/PugiXml2Tpcas.h
#        cas-generators/xml2tpcas/ReadXml2Stream.cpp cas-generators/xml2tpcas/ReadXml2Stream.h
#        cas-generators/Stream2Tpcas.cpp cas-generators/Stream2Tpcas.h)
#target_link_libraries(test_casmanager ${GTEST_BOTH_LIBRARIES} lucene++ pthread boost_system boost_iostreams boost_regex icuuc uima
#        boost_filesystem xerces-c podofo z ${CImg_SYSTEM_LIBS} ${PYTHON_LIBRARIES})

install(TARGETS libtextpresso RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
install(FILES IndexManager.h CASManager.h DataStructures.h DESTINATION include/textpresso)

# uima annotators

add_library(Tpcas2TpCentral SHARED uima-annotators/Tpcas2TpCentral/Tpcas2TpCentral.h
        uima-annotators/Tpcas2TpCentral/Tpcas2TpCentral.cpp)
target_link_libraries(Tpcas2TpCentral ${PYTHON_LIBRARIES})

add_library(TpLexiconAnnotator SHARED uima-annotators/TpLexiconAnnotator/AnnotationCounter.h
        uima-annotators/TpLexiconAnnotator/AnnotationCounter.cpp uima-annotators/TpLexiconAnnotator/StopWords.h
        uima-annotators/TpLexiconAnnotator/StopWords.cpp
        uima-annotators/globaldefinitions.h
        uima-annotators/TpLexiconAnnotator/TpLexiconAnnotator.h
        uima-annotators/TpLexiconAnnotator/TpLexiconAnnotator.cpp uima-annotators/TpLexiconAnnotator/TpLexiconNode.h
        uima-annotators/TpLexiconAnnotator/TpLexiconNode.cpp uima-annotators/TpLexiconAnnotator/TpLexiconTrie.h
        uima-annotators/TpLexiconAnnotator/TpLexiconTrie.cpp)
target_link_libraries(TpLexiconAnnotator TpTokenizer ${PYTHON_LIBRARIES})

add_library(TpLexiconAnnotatorFromPg SHARED uima-annotators/TpLexiconAnnotatorFromPg/AnnotationCounter.h
        uima-annotators/TpLexiconAnnotatorFromPg/AnnotationCounter.cpp
        uima-annotators/TpLexiconAnnotatorFromPg/StopWords.h
        uima-annotators/TpLexiconAnnotatorFromPg/StopWords.cpp
        uima-annotators/TpLexiconAnnotatorFromPg/TpLexiconAnnotatorFromPg.h
        uima-annotators/TpLexiconAnnotatorFromPg/TpLexiconAnnotatorFromPg.cpp
        uima-annotators/TpLexiconAnnotatorFromPg/TpLexiconNode.h
        uima-annotators/TpLexiconAnnotatorFromPg/TpLexiconNode.cpp
        uima-annotators/TpLexiconAnnotatorFromPg/TpLexiconTrie.h
        uima-annotators/TpLexiconAnnotatorFromPg/TpLexiconTrie.cpp
        uima-annotators/TpLexiconAnnotatorFromPg/AllMyParents.h
        uima-annotators/TpLexiconAnnotatorFromPg/AllMyParents.cpp)
target_link_libraries(TpLexiconAnnotatorFromPg pqxx TpTokenizer ${PYTHON_LIBRARIES})

add_library(TpLsa SHARED uima-annotators/TpLsa/cmdline.h uima-annotators/TpLsa/LsaTp.h uima-annotators/TpLsa/LsaTp.cpp
        uima-annotators/TpLsa/main.cpp uima-annotators/TpLsa/redsvd.hpp uima-annotators/TpLsa/redsvdFile.hpp
        uima-annotators/TpLsa/redsvdFile.cpp uima-annotators/TpLsa/redsvdIncr.hpp uima-annotators/TpLsa/SetsFromPairs.h
        uima-annotators/TpLsa/SetsFromPairs.cpp uima-annotators/TpLsa/TpCas2LsaToken.h
        uima-annotators/TpLsa/TpCas2LsaToken.cpp uima-annotators/TpLsa/util.hpp uima-annotators/TpLsa/util.cpp)
target_link_libraries(TpLsa boost_iostreams boost_system uima xerces-c ${PYTHON_LIBRARIES})

add_library(TpTokenizer SHARED uima-annotators/TpTokenizer/AnnotationCounter.h
        uima-annotators/TpTokenizer/AnnotationCounter.cpp
        uima-annotators/TpTokenizer/TpNode.h uima-annotators/TpTokenizer/TpNode.cpp
        uima-annotators/TpTokenizer/TpTokenizer.h
        uima-annotators/TpTokenizer/TpTokenizer.cpp uima-annotators/TpTokenizer/TpTrie.h
        uima-annotators/TpTokenizer/TpTrie.cpp)
target_link_libraries(TpTokenizer boost_regex ${PYTHON_LIBRARIES})

add_library(TxTokenizer SHARED uima-annotators/TxTokenizer/AnnotationCounter.h
        uima-annotators/TxTokenizer/AnnotationCounter.cpp
        uima-annotators/TxTokenizer/TpNode.h uima-annotators/TxTokenizer/TpNode.cpp
        uima-annotators/TxTokenizer/TxTokenizer.h
        uima-annotators/TxTokenizer/TxTokenizer.cpp uima-annotators/TxTokenizer/TpTrie.h
        uima-annotators/TxTokenizer/TpTrie.cpp uima-annotators/TxTokenizer/pugiconfig.hpp
        uima-annotators/TxTokenizer/pugixml.hpp uima-annotators/TxTokenizer/pugixml.cpp)
target_link_libraries(TxTokenizer boost_regex ${PYTHON_LIBRARIES})

add_library(TdTokenizer SHARED 
        uima-annotators/TdTokenizer/AnnotationCounter.h
        uima-annotators/TdTokenizer/AnnotationCounter.cpp
        uima-annotators/TdTokenizer/TpNode.h
        uima-annotators/TdTokenizer/TpNode.cpp
        uima-annotators/TdTokenizer/TdTokenizer.h
        uima-annotators/TdTokenizer/TdTokenizer.cpp
        uima-annotators/TdTokenizer/TpTrie.h
        uima-annotators/TdTokenizer/TpTrie.cpp
        uima-annotators/TdTokenizer/textAndImageManager.h
        uima-annotators/TdTokenizer/textAndImageManager.cpp)
target_link_libraries(TdTokenizer boost_regex ${PYTHON_LIBRARIES})



add_library(Tpcas2Bib SHARED uima-custom-analyzers/Tpcas2Bib/CASUtils.h
        uima-custom-analyzers/Tpcas2Bib/CASUtils.cpp uima-custom-analyzers/Tpcas2Bib/Tpcas2Bib.h
        uima-custom-analyzers/Tpcas2Bib/Tpcas2Bib.cpp uima-custom-analyzers/Tpcas2Bib/TpNode.h
        uima-custom-analyzers/Tpcas2Bib/TpNode.cpp uima-custom-analyzers/Tpcas2Bib/TpTrie.h
        uima-custom-analyzers/Tpcas2Bib/TpTrie.cpp
        uima-custom-analyzers/Tpcas2Bib/Utils.h)
target_link_libraries(Tpcas2Bib lucene++ xerces-c icuuc boost_system uima boost_filesystem boost_regex
        ${PYTHON_LIBRARIES})

add_library(Tpcas2Bib4Nxml SHARED uima-custom-analyzers/Tpcas2Bib4Nxml/CASUtils.h
        uima-custom-analyzers/Tpcas2Bib4Nxml/CASUtils.cpp uima-custom-analyzers/Tpcas2Bib4Nxml/Tpcas2Bib4Nxml.h
        uima-custom-analyzers/Tpcas2Bib4Nxml/Tpcas2Bib4Nxml.cpp uima-custom-analyzers/Tpcas2Bib4Nxml/TpNode.h
        uima-custom-analyzers/Tpcas2Bib4Nxml/TpNode.cpp uima-custom-analyzers/Tpcas2Bib4Nxml/TpTrie.h
        uima-custom-analyzers/Tpcas2Bib4Nxml/TpTrie.cpp)
target_link_libraries(Tpcas2Bib4Nxml lucene++ xerces-c icuuc boost_system uima boost_filesystem boost_regex
        ${PYTHON_LIBRARIES})

add_library(Tpcas2Lpp SHARED uima-custom-analyzers/Tpcas2Lpp/CASUtils.h
        uima-custom-analyzers/Tpcas2Lpp/CASUtils.cpp uima-custom-analyzers/Tpcas2Lpp/Tpcas2Lpp.h
        uima-custom-analyzers/Tpcas2Lpp/Tpcas2Lpp.cpp uima-custom-analyzers/Tpcas2Lpp/Utils.h
        lucene-custom/CaseSensitiveAnalyzer.cpp lucene-custom/CaseSensitiveAnalyzer.h)
target_link_libraries(Tpcas2Lpp lucene++ xerces-c icuuc boost_system uima boost_filesystem boost_regex
        ${PYTHON_LIBRARIES})

add_library(Tpcas2SingleIndex SHARED uima-custom-analyzers/Tpcas2SingleIndex/CASUtils.h
        uima-custom-analyzers/Tpcas2SingleIndex/CASUtils.cpp uima-custom-analyzers/Tpcas2SingleIndex/Tpcas2SingleIndex.h
        uima-custom-analyzers/Tpcas2SingleIndex/Tpcas2SingleIndex.cpp uima-custom-analyzers/Tpcas2SingleIndex/TpNode.h
        uima-custom-analyzers/Tpcas2SingleIndex/TpNode.cpp uima-custom-analyzers/Tpcas2SingleIndex/TpTrie.h
        uima-custom-analyzers/Tpcas2SingleIndex/TpTrie.cpp uima-custom-analyzers/Tpcas2SingleIndex/Utils.h
        lucene-custom/CaseSensitiveAnalyzer.cpp lucene-custom/CaseSensitiveAnalyzer.h
        CASManager.cpp CASManager.h Utils.h Utils.cpp ${CAS_GENERATORS_FILES})
target_link_libraries(Tpcas2SingleIndex lucene++ xerces-c icuuc boost_system uima boost_filesystem boost_regex
        boost_iostreams ${PYTHON_LIBRARIES})

add_executable(WriteFeatureDefsFromPg uima-annotators/WriteFeatureDefsFromPg/main.cpp)
target_link_libraries(WriteFeatureDefsFromPg uima pqxx)

#### INSTALL ####

install(FILES
        uima-annotators/TpLexiconAnnotatorFromPg/resources/descriptors/TpLexiconAnnotatorFromPgFeatureSupplementals.xml
        uima-annotators/TpLexiconAnnotatorFromPg/resources/descriptors/TpLexiconAnnotatorFromPgTypeSystem.xml
        uima-annotators/TpLexiconAnnotatorFromPg/resources/descriptors/TpLexiconAnnotatorFromPgWithTmpTpontology.xml
        uima-annotators/TpLexiconAnnotatorFromPg/resources/descriptors/TpLexiconAnnotatorFromPg.xml
        uima-annotators/TpLexiconAnnotator/resources/descriptors/TpLexiconAnnotatorTypeSystem.xml
        uima-annotators/TpLexiconAnnotator/resources/descriptors/TpLexiconAnnotator.xml
        uima-annotators/TpTokenizer/resources/descriptors/TpTokenizerTypeSystem.xml
        uima-annotators/TpTokenizer/resources/descriptors/TpTokenizer.xml
        uima-annotators/TxTokenizer/resources/descriptors/TxTokenizerTypeSystem.xml
        uima-annotators/TxTokenizer/resources/descriptors/TxTokenizer.xml
        uima-annotators/TdTokenizer/resources/descriptors/TdTokenizerTypeSystem.xml
        uima-annotators/TdTokenizer/resources/descriptors/TdTokenizer.xml
        uima-annotators/shared_resources/descriptors/Tpcas2Lindex.xml
        uima-annotators/shared_resources/descriptors/Tpcas2TpCentral.xml
        DESTINATION uima_descriptors)

install(DIRECTORY tests/data DESTINATION share/textpresso)

install(TARGETS Tpcas2TpCentral Tpcas2SingleIndex TpLexiconAnnotatorFromPg TpTokenizer TxTokenizer TdTokenizer Tpcas2Bib
        Tpcas2Bib4Nxml
        LIBRARY DESTINATION lib)
