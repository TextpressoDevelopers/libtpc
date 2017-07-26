/* 
 * File:   main.cpp
 * Author: mueller
 *
 * Created on October 7, 2013, 1:33 PM
 */

#include "../globaldefinitions.h"
#include <iostream>
#include <pqxx/pqxx>
#include <boost/algorithm/string.hpp>

int main(int argc, char** argv) {
    // database connection
    pqxx::connection cn(PGONTOLOGY);
    //std::cerr << "Connected to " << cn.dbname() << std::endl;

    pqxx::work w(cn);
    pqxx::result r;
    std::stringstream pc;
    // select attributes from tpontology where attributes ~ '='
    pc << "select attributes from ";
    pc << PGONTOLOGYTABLENAME;
    pc << " where attributes ~ '='";
    r = w.exec(pc.str());
    std::vector<std::string> atts;
    if (r.size() != 0) {
        //std::cerr << r.size() << " rows retrieved." << std::endl;
        for (pqxx::result::size_type i = 0; i != r.size(); i++) {
            std::string saux;
            r[i]["attributes"].to(saux);
            //std::cerr << saux << std::endl;
            boost::to_lower(saux);
            atts.push_back(saux);
        }
    }
    w.commit();
    cn.disconnect();
    std::stringstream sout;
    std::vector<std::string> features;
    while (!atts.empty()) {
        std::vector<std::string> splits1;
        boost::split(splits1, atts.back(), boost::is_any_of(" \n\t"));
        atts.pop_back();
        while (!splits1.empty()) {
            std::vector<std::string> splits2;
            boost::split(splits2, splits1.back(), boost::is_any_of("="));
            splits1.pop_back();
            features.push_back(splits2[0]);
        }
    }
    // Print the first lines
    sout << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
    sout << "<typeSystemDescription>" << std::endl;
    sout << "  <types>" << std::endl;
    sout << "    <typeDescription>" << std::endl;
    sout << "      <name>org.apache.uima.textpresso.lexicalannotation</name>" << std::endl;
    sout << "      <description></description>" << std::endl;
    sout << "      <supertypeName>uima.tcas.Annotation</supertypeName>" << std::endl;
    sout << "      <features>" << std::endl;
    for (int i = 0; i < features.size(); i++) {
        // Trim features;
        boost::trim(features[i]);
        sout << "        <featureDescription>" << std::endl;
        sout << "          <name>" << features[i] << "</name>" << std::endl;
        sout << "          <description>(This feature is automatically generated.)</description>" << std::endl;
        sout << "          <rangeTypeName>uima.cas.String</rangeTypeName>" << std::endl;
        sout << "        </featureDescription>" << std::endl;
    }
    // print end lines.
    sout << "      </features>" << std::endl;
    sout << "    </typeDescription>" << std::endl;
    sout << "  </types>" << std::endl;
    sout << "</typeSystemDescription>" << std::endl;

    std::cout << sout.str();

    return 0;
}

