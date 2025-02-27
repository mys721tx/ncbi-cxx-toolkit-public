/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/align_format/format_flags.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(align_format)

const string kArgOutputFormat("outfmt");
const int kDfltArgOutputFormat = 0;
string kDfltArgTabularOutputFmt =
    "qaccver saccver pident length mismatch gapopen qstart qend sstart send "
    "evalue bitscore";
const string kDfltArgTabularOutputFmtTag("std");

const size_t kNumTabularOutputFormatSpecifiers = 50;
const SFormatSpec sc_FormatSpecifiers[kNumTabularOutputFormatSpecifiers] = {
    SFormatSpec("qseqid",   
                "Query Seq-id",
                eQuerySeqId),
    SFormatSpec("qgi",
                "Query GI",
                eQueryGi),
    SFormatSpec("qacc",
                "Query accession",
                eQueryAccession),
    SFormatSpec("qaccver",
                "Query accession.version",
                eQueryAccessionVersion),
    SFormatSpec("qlen",
                "Query sequence length",
                eQueryLength),
    SFormatSpec("sseqid",
                "Subject Seq-id",
                eSubjectSeqId),
    SFormatSpec("sallseqid",
                "All subject Seq-id(s), separated by a ';'", 
                eSubjectAllSeqIds),
    SFormatSpec("sgi",
                "Subject GI", 
                eSubjectGi),
    SFormatSpec("sallgi",
                "All subject GIs", 
                eSubjectAllGis),
    SFormatSpec("sacc",
                "Subject accession", 
                eSubjectAccession),
    SFormatSpec("saccver",
                "Subject accession.version", 
                eSubjAccessionVersion),
    SFormatSpec("sallacc", 
                "All subject accessions", 
                eSubjectAllAccessions),
    SFormatSpec("slen",
                "Subject sequence length",
                eSubjectLength),
    SFormatSpec("qstart",
                "Start of alignment in query", 
                eQueryStart),
    SFormatSpec("qend",
                "End of alignment in query", 
                eQueryEnd),
    SFormatSpec("sstart", 
                "Start of alignment in subject", 
                eSubjectStart),
    SFormatSpec("send",
                "End of alignment in subject", 
                eSubjectEnd),
    SFormatSpec("qseq",
                "Aligned part of query sequence",
                eQuerySeq),
    SFormatSpec("sseq",
                "Aligned part of subject sequence", 
                eSubjectSeq),
    SFormatSpec("evalue", 
                "Expect value", 
                eEvalue),
    SFormatSpec("bitscore", 
                "Bit score", 
                eBitScore),
    SFormatSpec("score",
                "Raw score", 
                eScore),
    SFormatSpec("length", 
                "Alignment length", 
                eAlignmentLength),
    SFormatSpec("pident",
                "Percentage of identical matches", 
                ePercentIdentical),
    SFormatSpec("nident",
                "Number of identical matches", 
                eNumIdentical),
    SFormatSpec("mismatch",
                "Number of mismatches", 
                eMismatches),
    SFormatSpec("positive", 
                "Number of positive-scoring matches", 
                ePositives),
    SFormatSpec("gapopen", 
                "Number of gap openings", 
                eGapOpenings),
    SFormatSpec("gaps",
                "Total number of gaps", 
                eGaps),
    SFormatSpec("ppos",
                "Percentage of positive-scoring matches", 
                ePercentPositives),
    SFormatSpec("frames",   
                "Query and subject frames separated by a '/'", 
                eFrames),
    SFormatSpec("qframe", 
                "Query frame", 
                eQueryFrame),
    SFormatSpec("sframe",   
                "Subject frame", 
                eSubjFrame),
    SFormatSpec("btop",   
                "Blast traceback operations (BTOP)", 
                eBTOP),
    SFormatSpec("staxid",
                "Subject Taxonomy ID",
                eSubjectTaxId),
    SFormatSpec("ssciname",
                "Subject Scientific Name",
                eSubjectSciName),
    SFormatSpec("scomname",
                "Subject Common Name",
                eSubjectCommonName),
    SFormatSpec("sblastname",
                "Subject Blast Name",
                eSubjectBlastName),
    SFormatSpec("sskingdom",
                "Subject Super Kingdom",
                eSubjectSuperKingdom),
    SFormatSpec("staxids",
                "unique Subject Taxonomy ID(s), separated by a ';'\n\t\t\t (in numerical order)",
                eSubjectTaxIds),
    SFormatSpec("sscinames",
                "unique Subject Scientific Name(s), separated by a ';'",
                eSubjectSciNames),
    SFormatSpec("scomnames",
                "unique Subject Common Name(s), separated by a ';'",
                eSubjectCommonNames),
    SFormatSpec("sblastnames",
                "unique Subject Blast Name(s), separated by a ';'\n\t\t\t (in alphabetical order)",
                eSubjectBlastNames),
    SFormatSpec("sskingdoms",
                "unique Subject Super Kingdom(s), separated by a ';'\n\t\t\t (in alphabetical order) ",
                eSubjectSuperKingdoms),
    SFormatSpec("stitle",
                "Subject Title",
                eSubjectTitle),
    SFormatSpec("salltitles",
                "All Subject Title(s), separated by a '<>'",
                eSubjectAllTitles),
    SFormatSpec("sstrand",
                "Subject Strand",
                eSubjectStrand),
    SFormatSpec("qcovs",
                "Query Coverage Per Subject",
                eQueryCovSubject),
    SFormatSpec("qcovhsp",
                "Query Coverage Per HSP",
                eQueryCovSeqalign),
    SFormatSpec("qcovus",
                "Query Coverage Per Unique Subject (blastn only)",
                eQueryCovUniqSubject),
};

string DescribeTabularOutputFormatSpecifiers(bool is_igblast)
{
    // Igblast needs extra "gaps" column by default
    if (is_igblast) {
        kDfltArgTabularOutputFmt =
        "qseqid sseqid pident length mismatch gapopen gaps qstart qend sstart send "
        "evalue bitscore";
    }
    ostringstream os;
    for (size_t i = 0; i < kNumTabularOutputFormatSpecifiers; i++) {
        os << "\t" << setw(10) << sc_FormatSpecifiers[i].name << " means ";
        os << sc_FormatSpecifiers[i].description << "\n";
    }
    os << "When not provided, the default value is:\n";
    os << "'" << kDfltArgTabularOutputFmt << "', which is equivalent ";
    os << "to the keyword '" << kDfltArgTabularOutputFmtTag << "'";
    return os.str();
}

const string kArgShowGIs("show_gis");
const bool kDfltArgShowGIs = false;
const string kArgNumDescriptions("num_descriptions");
const size_t kDfltArgNumDescriptions = 500;
const string kArgNumAlignments("num_alignments");
const size_t kDfltArgNumAlignments = 250;
const string kArgProduceHtml("html");
const bool kDfltArgProduceHtml = false;
const size_t kDfltLineLength = 60;
const string kArgAlignSeqList("alignseqlist");
const string kArgMetadata("searchmetadata");
const string kArgQueryIndex("queryindex");
const string kArgSortHits("sorthits");
const string kArgSortHSPs("sorthsps");

const size_t kNumSAMOutputFormatSpecifiers = 2;
const SSAMFormatSpec sc_SAMFormatSpecifiers[kNumSAMOutputFormatSpecifiers] = {
    SSAMFormatSpec("SQ",
                   "Include Sequence Data",
                   eSAM_SeqData),
    SSAMFormatSpec("SR",
                   "Subject as Reference Seq",
                   eSAM_SubjAsRefSeq)
};

string DescribeSAMOutputFormatSpecifiers()
{
    ostringstream os;
    for (size_t i =0; i < kNumSAMOutputFormatSpecifiers; i++) {
        os << "\t" << setw(10) << sc_SAMFormatSpecifiers[i].name << " means ";
        os << sc_SAMFormatSpecifiers[i].description << "\n";
    }

    return os.str();
}

END_SCOPE(align_format)
END_NCBI_SCOPE
