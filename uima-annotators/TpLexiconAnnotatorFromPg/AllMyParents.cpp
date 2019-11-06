/* 
 * File:   AllMyParents.cpp
 * Author: mueller
 * 
 * Created on December 16, 2015, 7:02 PM
 */

#include "AllMyParents.h"
#include "../globaldefinitions.h"
#include <iostream>

AllMyParents::AllMyParents() : cn_(PGONTOLOGY) {
    GetAllParentChildren();
    ComputeAllChildrenParent();
}

AllMyParents::AllMyParents(const AllMyParents& orig) {
}

void AllMyParents::GetAllParentChildren() {
    try {
        pqxx::work w(cn_);
        pqxx::result r;
        std::stringstream pc;
        pc << "select parent,child from ";
        pc << PCRELATIONSTABLENAME;
        r = w.exec(pc.str());
        std::string cname;
        std::string pname;
        for (pqxx::result::size_type i = 0; i != r.size(); i++)
            if (r[i]["child"].to(cname))
                if (r[i]["parent"].to(pname))
                    if (pname.compare("root") != 0) {
                        pc_.insert(std::make_pair(pname, cname));
                    }
        w.commit();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}

void AllMyParents::ComputeAllChildrenParent() {
    std::map<std::string, std::string>::iterator it;
    std::set<std::string> firstroundchildren;
    std::multimap<std::string, std::string> inverted;
    // direct parents
    for (it = pc_.begin(); it != pc_.end(); it++) {
        inverted.insert(std::make_pair((*it).second, (*it).first));
        firstroundchildren.insert((*it).second);
    }
    // now find all other parents up the tree for each child.
    std::set<std::string>::iterator frcit;
    for (frcit = firstroundchildren.begin(); frcit != firstroundchildren.end(); frcit++) {
        std::set<std::string> op = FindAllParentsOfChild(*frcit, inverted);
        for (std::set<std::string>::iterator opit = op.begin(); opit != op.end(); opit++)
            cp_.insert(std::make_pair(*frcit, *opit));
    }
}

std::set<std::string> AllMyParents::FindAllParentsOfChild(std::string c, std::multimap<std::string, std::string> & cp) {
    std::set<std::string> allparents;
    std::pair<std::multimap<std::string, std::string>::iterator,
            std::multimap<std::string, std::string>::iterator> ppp;
    ppp = cp.equal_range(c);
    for (std::multimap<std::string, std::string>::iterator it1 = ppp.first; it1 != ppp.second; ++it1) {
        std::multimap<std::string, std::string>::iterator it2(it1);
        while (it2 != cp.end()) {
            std::string newchild((*it2).second);
            if (allparents.find(newchild) != allparents.end()) break;
            allparents.insert(newchild);
            it2 = cp.find(newchild);
        }
    }
    return allparents;
}

AllMyParents::~AllMyParents() {
    cn_.disconnect();
}
