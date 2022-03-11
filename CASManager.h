/**
    Project: libtpc
    File name: TpcCASManager.h
    
    @author valerio
    @version 1.0 7/28/17.
*/

#ifndef LIBTPC_TPCCASMANAGER_H
#define LIBTPC_TPCCASMANAGER_H

#include <string>
#include <vector>
#include "uima/xmiwriter.hpp"

namespace tpc {
    namespace cas {

        static const std::string PDF2TPCAS_DESCRIPTOR("/usr/local/uima_descriptors/TpTokenizer.xml");
        static const std::string XML2TPCAS_DESCRIPTOR("/usr/local/uima_descriptors/TxTokenizer.xml");
        static const std::string TAI2TPCAS_DESCRIPTOR("/usr/local/uima_descriptors/TdTokenizer.xml");
        static const std::string TPCAS1_2_TPCAS2_DESCRIPTOR("/usr/local/uima-descriptors/TpLexiconAnnotatorFromPg.xml");

        static const std::vector<std::pair<std::string, std::string>> PMCOA_CAT_REGEX{
                {"PMCOA Biology", ".*[Bb]io.*"}, {"PMCOA Neuroscience", ".*[Nn]euro.*"}, {"PMCOA Oncology", ".*([Cc]anc|[Oo]nc).*"},
                {"PMCOA Methodology", ".*[Mm]ethod.*"}, {"PMCOA Medicine", ".*[Mm]edic.*"}, {"PMCOA Virology", ".*[Vv]ir(us|ol).*"},
                {"PMCOA Genetics", ".*[Gg]enet.*"}, {"PMCOA Animal", ".*[Aa]nimal.*"}, {"PMCOA Clinical", ".*[Cc]linic.*"},
                {"PMCOA Genomics", ".*[Gg]enom.*"}, {"PMCOA Disease", ".*[Dd]i(seas|abet).*"},
                {"PMCOA Agriculture", ".*[Aa]gricult.*"}, {"PMCOA Physiology", ".*[Pp]hysiol.*"}, {"PMCOA Psychology", ".*[Pp]sych(ol|iat).*"},
                {"PMCOA Crystallography", ".*[Cc]rystal.*"}, {"PMCOA Chemistry", ".*[Cc]hemi.*"}, {"PMCOA Health", ".*[Hh]ealth.*"}, {"PMCOA Cardiology", ".*([Cc]ardi|[Hh]eart).*"},
                {"PMCOA Pharmacology", ".*[Pp]harm.*"}, {"PMCOA Nutrition", ".*[Nn]utri.*"}, {"PMCOA Immunology", ".*[Ii]mmuno.*"}, {"PMCOA Pediatrics", ".*[Pp]a?ediatri.*"},
                {"PMCOA Review", ".*[Rr]eview.*"}, {"PMCOA Protein", ".*[Pp]rotein.*"}, {"PMCOA D. melanogaster", ".*(Drosophila( melanogaster)?|[Ff]ruit [Ff]ly|D\. melanogaster).*"}, {"PMCOA C. elegans", ".*(Caenorhabditis( elegans)?|C\. elegans).*"},
                {"PMCOA A. thaliana", ".*(Arabidopsis( thaliana)?|A\. thaliana).*"}, {"PMCOA M. musculus", ".*(Mus( musculus)?|M\. musculus|[Mm]usulus|[Mm]murine|[Mm]ouse|[Mm]ice).*"}, {"PMCOA D. rerio", ".*(Danio rerio|D\. rerio|[Zz]ebrafish).*"}, {"PMCOA S. cerevisiae", ".*(Saccharomyces( cerevisiae)?|S\. cerevisiae|[Bb]udding [Yy]east).*"},
                {"PMCOA S. pombe", ".*(Schizosaccharomyces( pombe)?|S\. pombe|([Ff]ission) [Yy]east).*"}, {"PMCOA D. discoideum", ".*(Dictyostelium( discoideum)?|D\. discoideum|[Ss]lime [Mm]old).*"}, {"PMCOA R. norvegicus", ".*(Rattus norvegicus|R\. norvegicus|Norway brown rat).*"}, {"PMCOA R. rattus", ".*(Rattus rattus|R\. rattus|[Bb]lack rat).*"},
                {"PMCOA C. intestinalis", ".*(Ciona intestinalis|C\. intestinalis|[Ss]ea [Ss]quirt).*"}, {"PMCOA X. laevis", ".*(Xenopus laevis|X\. laevis|African clawed frog).*"}, {"PMCOA X. tropicalis", ".*(Xenopus tropicalis|X\. tropicalis|Western clawed frog).*"}, {"PMCOA E. coli", ".*(Escherichia coli|E\. coli).*"},
                {"PMCOA B. subtilis", ".*(Bacillus subtilis|B\. subtilis).*"}};

        static const std::string PMCOA_UNCLASSIFIED("PMCOA Unclassified");
        static const std::string CELEGANS("C. elegans");
        static const std::string CELEGANS_SUP("C. elegans Supplementals");

        enum class FileType {
            pdf = 1, xml = 2, txt = 3, tai = 4
        };

        /*!
         * @struct BibInfo
         * @brief data structure that represents bib information of a cas file
         *
         * @var <b>author</b> the name of the author(s) of the article
         * @var <b>accession</b> unique identifier of the article in the original format
         * @var <b>type</b> article type (e.g., research article)
         * @var <b>title</b> title of the article
         * @var <b>journal</b> name of the journal where the article appears
         * @var <b>citation</b> additional information on how to cite the article
         * @var <b>year</b> year of publication
         * @var <b>abstract</b> fulltext abstract of the article
         * @var <b>subject</b> subject of the article
         */
        struct BibInfo {
            std::string author;
            std::string accession;
            std::string type;
            std::string title;
            std::string journal;
            std::string citation;
            std::string year;
            std::string abstract;
            std::string subject;
        };

        class CASManager {
        public:
            /*!
             * convert a pdf or xml article to cas1 format and save it to the specified location
             * @param file_path the path to the raw file
             * @param type the type of file
             * @param out_dir the location where to save the new cas file
             */
            static void convert_raw_file_to_cas1(const std::string &file_path, FileType type,
                                                 const std::string &out_dir, bool use_parent_dir_as_outname = false);

            /*!
             * convert a cas1 file to cas2 format and save it to the specified location
             * @param file_path the path to the cas1 file
             * @param out_dir the location where to save the new cas file
             * @return 1 if the conversion succeded, 0 otherwise
             */
            static int convert_cas1_to_cas2(const std::string& file_path, const std::string& out_dir);

            /*!
             * extract bib information from the xml fulltext of an article
             * @param xml_text a string containing the xml fulltext of an article
             * @return the bib info of the xml article
             */
            static BibInfo get_bib_info_from_xml_text(const std::string& xml_text);

            /*!
             * get the list of corpora to which the article belongs through classification performed on bibliographic
             * information
             * @param bib_info object containing the bibliographic information of the article
             */
            static std::vector<std::string> classify_article_into_corpora_from_bib_file(const BibInfo& bib_info);

        private:

            static void writeXmi(uima::CAS &outCas, int num, std::string outfn);
        };
    }
}

#endif //LIBTPC_TPCCASMANAGER_H
