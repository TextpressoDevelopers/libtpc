/* 
 * File:   SetsFromPairs.h
 * Author: mueller
 *
 * Created on July 17, 2014, 10:03 AM
 */

#ifndef SETSFROMPAIRS_H
#define	SETSFROMPAIRS_H

#include <map>
#include <set>
#include <string>
#include <vector>

class SetsFromPairs {
    typedef std::multimap<int, int> mmii;
    typedef std::multimap<int, int>::iterator mmiiit;
public:
    SetsFromPairs();
    ~SetsFromPairs();
    void AddPair(int i, int j);
    void IdentifySets();
    void PrintSets(std::string rootname);
    void ClearSets();
    void ClearPairs() {
        pairs_.clear();
    }
    std::vector< std::set<int>*> & Sets() { return sets_; }
private:
    mmii pairs_;
    std::vector< std::set<int>*> sets_;
    std::set<int> GetSetFromSeed(int seed);
};

#endif	/* SETSFROMPAIRS_H */

