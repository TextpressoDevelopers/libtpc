/* 
 * File:   SetsFromPairs.cpp
 * Author: mueller
 * 
 * Created on July 17, 2014, 10:03 AM
 */

#include "SetsFromPairs.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

SetsFromPairs::SetsFromPairs() {
}

SetsFromPairs::~SetsFromPairs() {
    ClearSets();
}

void SetsFromPairs::ClearSets() {
    while (!sets_.empty()) {
        delete sets_.back();
        sets_.pop_back();
    }
}

void SetsFromPairs::AddPair(int i, int j) {
    pairs_.insert(std::make_pair(i, j));
    pairs_.insert(std::make_pair(j, i));
}

std::set<int> SetsFromPairs::GetSetFromSeed(int seed) {
    std::set<int> set;
    set.clear();
    long unsigned int oldsize = 0;
    std::pair<mmiiit, mmiiit> ppp = pairs_.equal_range(seed);
    set.insert(seed);
    for (mmiiit it = ppp.first; it != ppp.second; it++) {
        set.insert((*it).second);
    }
    while (set.size() != oldsize) {
        oldsize = set.size();
        for (std::set<int>::iterator it = set.begin(); it != set.end(); it++) {
            ppp = pairs_.equal_range(*it);
            for (mmiiit it2 = ppp.first; it2 != ppp.second; it2++) {
                set.insert((*it2).second);
            }
        }
    }
    return set;
}

void SetsFromPairs::IdentifySets() {
    mmii pairs(pairs_);
    while (!pairs.empty()) {
        int seed = (*pairs.begin()).first;
        std::set<int> * set = new std::set<int>;
        *set = GetSetFromSeed(seed);
        sets_.push_back(set);
        // find pairs that are not in the set;
        mmii auxpairs;
        auxpairs.clear();
        for (mmiiit it = pairs.begin(); it != pairs.end(); it++) {
            if (set->find((*it).first) == set->end())
                if (set->find((*it).second) == set->end())
                    auxpairs.insert(std::make_pair((*it).first, (*it).second));
        }
        pairs = auxpairs;
    }
}

void SetsFromPairs::PrintSets(std::string rootname) {
    int count = 0;
    for (std::vector< std::set<int>*>::iterator vit = sets_.begin(); vit != sets_.end(); vit++) {
        std::stringstream saux;
        saux << count++;
        std::string setfile = rootname + saux.str();
        std::ofstream sffs(setfile.c_str());
        if (!sffs) {
            throw std::string("cannot open ") + setfile;
        }
        // print set
        for (std::set<int>::iterator it = (*(*vit)).begin(); it != (*(*vit)).end(); it++)
            sffs << *it << std::endl;
        sffs.close();
    }
}

