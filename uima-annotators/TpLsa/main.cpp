/* 
 * File:   main.cpp
 * Author: mueller
 *
 * Created on June 27, 2014, 1:34 PM
 */

/*
 * 
 */

#include "TpCas2LsaToken.h"
#include "LsaTp.h"
#include <iostream>
#include <fstream>
#include "cmdline.h"

int main(int argc, char * argv[]) {
    srand(time(NULL));
    cmdline::parser p;
    p.add<std::string > ("filelist", 'f', "list of files to be processed.", true);
    p.add<std::string > ("command", 'c', "command letters", true);
    p.add<int>("rank", 'r', "rank      ", false, 10);
    p.add<float>("threshold", 't', "threshold of scalar product for printout", false, 0.0f);
    p.add("exclusion", 'e', "exclude files in .EXC list");
    p.add("printU", 'u', "print to file matrix U");
    p.add("printS", 's', "print to file vector S");
    p.add("printV", 'v', "print to file matrix V");
    p.set_program_name("tplsa");
    p.footer(
            "\n\n"
            "Commands available:\n\n"
            "w Process all files in filelist (files must be in TPCAS format)\n"
            "  and write a word count file (.WCS).\n"
            "f Compute float values and write a .FSV file.\n"
            "c Compute the document-term matrix A, calculate the SVD and\n"
            "  and write out U S V as well the scalarproducts of all\n"
            "  document vectors.\n"
            "s read scalarproducts and print sets for links with\n"
            "  scalarproduct of threshold t or higher\n"
            "d read scalarproducts and print doc IDs of similar papers\n"
            "a is equivalent to 'wfc'.\n"
            );
    if (argc < 2) {
        std::cerr << p.usage() << std::endl;
        return -1;
    }
    if (p.parse(argc, argv) == 0) {
        std::cerr << "Error:" << p.error() << std::endl
                << p.usage() << std::endl;
        return -1;
    }
    std::string filelist = p.get<std::string > ("filelist");
    std::string command = p.get<std::string > ("command");
    float threshold = p.get<float > ("threshold");
    int rank = p.get<int> ("rank");
    if (rank <= 0) {
        std::cerr << "rank=" << rank << std::endl
                << "rank should be positive integer" << std::endl;
        return -1;
    }
    try {
        LsaTp * plsatp = new LsaTp();
        if (command.compare("a") == 0) command = "wfc";
        plsatp->SetPrefix(filelist);
        if (command.find("w") != std::string::npos) {
            plsatp->ClearFile("WCS");
            std::ifstream ifs(filelist.c_str());
            if (!ifs) {
                throw std::string("cannot open ") + filelist;
            }
            for (std::string line; getline(ifs, line);) {
                //std::cout << "Processing " << line << std::endl;
                TpCas2LsaToken * ptk = new TpCas2LsaToken(line);
                plsatp->AddWordCount2Wcs(line, ptk->TokenCount());
                delete ptk;
            }
            ifs.close();
            plsatp->WriteDoc2Id();
            plsatp->WriteWord2Id();
        }
        if (command.find("f") != std::string::npos) {
            //std::cout << "Calculate FVS..." << std::endl;
            plsatp->CalculateFVS();
        }
        if (command.find("c") != std::string::npos) {
            if (p.exist("exclusion")) plsatp->ReadExclusionIds();
            //std::cout << "Calculate A..." << std::endl;
            plsatp->CalculateA();
            //std::cout << "Do SVD..." << std::endl;
            plsatp->DoLsa(rank);
            //std::cout << "Write out results..." << std::endl;
            plsatp->WriteScalarProducts2File(threshold, true);
            plsatp->WriteSVD(p.exist("printU"), p.exist("printS"), p.exist("printV"));
        }
        if (command.find("s") != std::string::npos)
            plsatp->ReadScalarProductsAndIdentifySets(threshold);
        if (command.find("d") != std::string::npos)
            plsatp->ReadScalarProductsAndIdentifySimilars(threshold);
    } catch (std::string & errorMessage) {
        std::cerr << errorMessage << std::endl;
        return -1;
    }
    return 0;
}

