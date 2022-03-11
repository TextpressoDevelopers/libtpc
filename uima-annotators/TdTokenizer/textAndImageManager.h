/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   textAndImageManager.h
 * Author: mueller
 *
 * Created on January 28, 2022, 9:46 PM
 */

#ifndef TEXTANDIMAGEMANAGER_H
#define TEXTANDIMAGEMANAGER_H

#include <boost/filesystem.hpp>
#include <set>
#include <map>

using namespace boost::filesystem;

class textAndImageManager {
public:
    textAndImageManager(const path dn);
    textAndImageManager(const textAndImageManager& orig);
    virtual ~textAndImageManager();
    void loadImageFilenames();
    void loadTextFilenames();
    void loadTextfiles();

    std::set<std::string> imageFilenames() const {
        return imageFilenames_;
    };
    
    std::set<std::string> textFilenames() const {
        return textFilenames_;
    };
    
    std::map<std::string, std::string> textFile() const {
        return textFile_;
    };

//    std::map<std::string, std::string>& textFile(std::string f) {
//        return textFile_;
//    };

private:
    path directory_;
    std::set<std::string> imageFilenames_;
    std::set<std::string> textFilenames_;
    std::map<std::string, std::string> textFile_;
};

#endif /* TEXTANDIMAGEMANAGER_H */

