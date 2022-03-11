/* 
 * File:   textAndImageManager.cpp
 * Author: mueller
 * 
 * Created on January 28, 2022, 9:46 PM
 */

#include "textAndImageManager.h"
#include <iostream>
//#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string.hpp>

using namespace boost::filesystem;

textAndImageManager::textAndImageManager(const path fn) : directory_(fn) {
}

textAndImageManager::textAndImageManager(const textAndImageManager& orig) {
}

textAndImageManager::~textAndImageManager() {
}

void textAndImageManager::loadImageFilenames() {
    std::vector<std::string> dirpieces;
    boost::split(dirpieces,directory_.string(), boost::is_any_of("/"));
    std::string d;
    if (dirpieces.size() > 1) 
        d = dirpieces[dirpieces.size()-2] + "/" + dirpieces[dirpieces.size()-1];
    for (directory_iterator dit(directory_); dit != directory_iterator(); ++dit) {
        std::string f(dit->path().filename().string());
        if (f.find(".jpg") != std::string::npos) imageFilenames_.insert(d + "/images/" + f);
        if (f.find(".jpeg") != std::string::npos) imageFilenames_.insert(d + "/images/" + f);
        if (f.find(".gif") != std::string::npos) imageFilenames_.insert(d + "/images/" + f);
        if (f.find(".png") != std::string::npos) imageFilenames_.insert(d + "/images/" + f);
        if (f.find(".tif") != std::string::npos) imageFilenames_.insert(d + "/images/" + f);
    }
}

void textAndImageManager::loadTextFilenames() {
    for (directory_iterator dit(directory_); dit != directory_iterator(); ++dit) {
        std::string f(dit->path().filename().string());
        if (f.find(".txt") != std::string::npos) textFilenames_.insert(directory_.string() + "/" + f);
    }
}

void textAndImageManager::loadTextfiles() {
    for (auto x : textFilenames_) {
        ifstream f(x.c_str());
        std::string in;
        while (getline(f, in)) textFile_[x] += in + "\n";
    }
}