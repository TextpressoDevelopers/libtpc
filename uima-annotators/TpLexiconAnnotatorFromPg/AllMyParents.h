/* 
 * File:   AllMyParents.h
 * Author: mueller
 *
 * Created on December 16, 2015, 7:02 PM
 */

#ifndef ALLMYPARENTS_H
#define	ALLMYPARENTS_H

#include <pqxx/pqxx>
#include <set>


class AllMyParents {
public:
    AllMyParents();
    AllMyParents(const AllMyParents& orig);
    std::multimap<std::string, std::string> GetCPs () { return cp_; }
    virtual ~AllMyParents();
private:
    pqxx::connection cn_;
    std::multimap<std::string, std::string> pc_;
    std::multimap<std::string, std::string> cp_;
    void GetAllParentChildren();
    void ComputeAllChildrenParent();
    std::set<std::string> FindAllParentsOfChild(std::string c, std::multimap<std::string, std::string> & cp);
};

#endif	/* ALLMYPARENTS_H */

