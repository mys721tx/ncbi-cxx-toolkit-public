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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  Implemented methods to identify file formats.
 *
 */

#include <ncbi_pch.hpp>
#include <util/format_guess.hpp>
#include <util/util_exception.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/stream_utils.hpp>


BEGIN_NCBI_SCOPE


// Must list all *supported* EFormats except eUnknown and eFormat_max. 
// Will cause assertion if violated!

static const CFormatGuess::EFormat sm_CheckOrder[] = 
{
    CFormatGuess::eBam, // must precede eGZip!
    CFormatGuess::eZip,
    CFormatGuess::eZstd,
    CFormatGuess::eGZip,
    CFormatGuess::eBZip2,
    CFormatGuess::eLzo,
    CFormatGuess::eSra,
    CFormatGuess::ePsl, // must be checked before eRmo!
    CFormatGuess::eRmo,
    CFormatGuess::eVcf,
    CFormatGuess::eGvf,
    CFormatGuess::eGff3,
    CFormatGuess::eGtf,
    CFormatGuess::eGffAugustus,
    CFormatGuess::eGff2,
    CFormatGuess::eGlimmer3,
    CFormatGuess::eAgp,
    CFormatGuess::eXml,
    CFormatGuess::eWiggle,
    CFormatGuess::eNewick,
    CFormatGuess::eBed,
    CFormatGuess::eBed15,
    CFormatGuess::eHgvs,
    CFormatGuess::eDistanceMatrix,
    CFormatGuess::eFlatFileSequence,
    CFormatGuess::eFlatFileUniProt,
    CFormatGuess::eFlatFileEna,
    CFormatGuess::eFlatFileGenbank,
    CFormatGuess::eFiveColFeatureTable,
    CFormatGuess::eSnpMarkers,
    CFormatGuess::eFasta,
    CFormatGuess::eTextASN,
    CFormatGuess::eAlignment,    
    CFormatGuess::eTaxplot,
    CFormatGuess::eTable,
    CFormatGuess::eBinaryASN,
    CFormatGuess::ePhrapAce,
    CFormatGuess::eUCSCRegion,
    CFormatGuess::eJSON,
};
constexpr size_t sm_CheckOrder_Size = sizeof(sm_CheckOrder) / sizeof(sm_CheckOrder[0]);


// This array must stay in sync with enum CFormatGuess::EFormat, 
// but that's not supposed to change in the middle anyway, 
// so the explicit size should suffice to avoid accidental skew.

typedef SStaticPair<CFormatGuess::EFormat, const char*> TFormatNamesItem;
typedef CStaticPairArrayMap<CFormatGuess::EFormat, const char*> TFormatNamesMap;

static const TFormatNamesItem s_format_to_name_table [] =
{
    { CFormatGuess::eUnknown,              "unknown"            },
    { CFormatGuess::eBinaryASN,            "binary ASN.1"       },
    { CFormatGuess::eRmo,                  "RepeatMasker"       },
    { CFormatGuess::eGtf_POISENED,         "GFF/GTF Poisoned"   },
    { CFormatGuess::eGlimmer3,             "Glimmer3"           },
    { CFormatGuess::eAgp,                  "AGP"                },
    { CFormatGuess::eXml,                  "XML"                },
    { CFormatGuess::eWiggle,               "WIGGLE"             },
    { CFormatGuess::eBed,                  "BED"                },
    { CFormatGuess::eBed15,                "BED15"              },
    { CFormatGuess::eNewick,               "Newick"             },
    { CFormatGuess::eAlignment,            "alignment"          },
    { CFormatGuess::eDistanceMatrix,       "distance matrix"    },
    { CFormatGuess::eFlatFileSequence,     "flat-file sequence" },
    { CFormatGuess::eFiveColFeatureTable,  "five-column feature table" },
    { CFormatGuess::eSnpMarkers,           "SNP Markers"        },
    { CFormatGuess::eFasta,                "FASTA"              },
    { CFormatGuess::eTextASN,              "text ASN.1"         },
    { CFormatGuess::eTaxplot,              "Taxplot"            },
    { CFormatGuess::ePhrapAce,             "Phrap ACE"          },
    { CFormatGuess::eTable,                "table"              },
    { CFormatGuess::eGtf,                  "GTF"                },
    { CFormatGuess::eGff3,                 "GFF3"               },
    { CFormatGuess::eGff2,                 "GFF2"               },
    { CFormatGuess::eHgvs,                 "HGVS"               },
    { CFormatGuess::eGvf,                  "GVF"                },
    { CFormatGuess::eZip,                  "zip"                },
    { CFormatGuess::eGZip,                 "gzip"               },
    { CFormatGuess::eBZip2,                "bzip2"              },
    { CFormatGuess::eLzo,                  "lzo"                },
    { CFormatGuess::eSra,                  "SRA"                },
    { CFormatGuess::eBam,                  "BAM"                },
    { CFormatGuess::eVcf,                  "VCF"                },
    { CFormatGuess::eUCSCRegion,           "UCSC Region"        },
    { CFormatGuess::eGffAugustus,          "GFF Augustus"       },
    { CFormatGuess::eJSON,                 "JSON"               },
    { CFormatGuess::ePsl,                  "PSL"                },
    { CFormatGuess::eAltGraphX,            "altGraphX"          },
    { CFormatGuess::eBed5FloatScore,       "BED5 float score"   },
    { CFormatGuess::eBedGraph,             "BED graph"          },
    { CFormatGuess::eBedRnaElements,       "BED Rna elements"   },
    { CFormatGuess::eBigBarChart,          "bigBarChart"        },
    { CFormatGuess::eBigBed,               "BigBED"             },
    { CFormatGuess::eBigPsl,               "BigPSL"             },
    { CFormatGuess::eBigChain,             "BigChain"           },
    { CFormatGuess::eBigMaf,               "BigMaf"             },
    { CFormatGuess::eBigWig,               "BigWig"             },
    { CFormatGuess::eBroadPeak,            "BroadPeak"          },
    { CFormatGuess::eChain,                "Chain"              },
    { CFormatGuess::eClonePos,             "ClonePos"           },
    { CFormatGuess::eColoredExon,          "ColoredExon"        },
    { CFormatGuess::eCtgPos,               "CtgPos"             },
    { CFormatGuess::eDownloadsOnly,        "DowloadsOnly"       },
    { CFormatGuess::eEncodeFiveC,          "EncodeFiveC"        },
    { CFormatGuess::eExpRatio,             "ExpRatio"           },
    { CFormatGuess::eFactorSource,         "FactorSource"       },
    { CFormatGuess::eGenePred,             "GenePred"           },
    { CFormatGuess::eLd2,                  "Ld2"                },
    { CFormatGuess::eNarrowPeak,           "NarrowPeak"         },
    { CFormatGuess::eNetAlign,             "NetAlign"           },
    { CFormatGuess::ePeptideMapping,       "PeptideMapping"     },
    { CFormatGuess::eRmsk,                 "Rmsk"               },
    { CFormatGuess::eSnake,                "Snake"              },
    { CFormatGuess::eVcfTabix,             "VcfTabix"           },
    { CFormatGuess::eWigMaf,               "WigMaf"             },
    { CFormatGuess::eFlatFileGenbank,      "Genbank FlatFile"   },
    { CFormatGuess::eFlatFileEna,          "ENA FlatFile"       },
    { CFormatGuess::eFlatFileUniProt,      "UniProt FlatFile"   },
    { CFormatGuess::eZstd,                 "zstd"               },
};
DEFINE_STATIC_ARRAY_MAP(TFormatNamesMap, sm_FormatNames, s_format_to_name_table);


    
enum ESymbolType {
    fDNA_Main_Alphabet  = 1<<0, ///< Just ACGTUN-.
    fDNA_Ambig_Alphabet = 1<<1, ///< Anything else representable in ncbi4na.
    fProtein_Alphabet   = 1<<2, ///< Allows BZX*-, but not JOU.
    fLineEnd            = 1<<3,
    fAlpha              = 1<<4,
    fDigit              = 1<<5,
    fSpace              = 1<<6,
    fInvalid            = 1<<7
};

enum EConfidence {
    eNo = 0,
    eMaybe,
    eYes
};



//  ============================================================================
//  Helper routine--- file scope only:
//  ============================================================================

static unsigned char symbol_type_table[256];

//  ----------------------------------------------------------------------------
static bool s_IsTokenPosInt(
    const string& strToken )
    //  ----------------------------------------------------------------------------
{
    size_t tokenSize = strToken.size();
    if (tokenSize == 0) {
        return false;
    }
    if (tokenSize == 1  &&  strToken[0] == '0') {
        return true;
    }
    if (strToken[0] < '1'  ||  '9' < strToken[0]) {
        return false;
    }
    for (size_t i=1; i<tokenSize; ++i) {
        if (strToken[i] < '0'  ||  '9' < strToken[i]) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
static bool s_IsTokenInteger(
    const string& strToken )
//  ----------------------------------------------------------------------------
{
    if ( ! strToken.empty() && (strToken[0] == '-'  ||  strToken[0] == '+')) {
        return s_IsTokenPosInt( strToken.substr( 1 ) );
    }
    return s_IsTokenPosInt( strToken );
}

//  ----------------------------------------------------------------------------
static bool s_IsTokenDouble(
    const string& strToken )
{
    string token( strToken );
    NStr::ReplaceInPlace( token, ".", "1", 0, 1 );
    if ( token.size() > 1 && token[0] == '-' ) {
        token[0] = '1';
    }
    if (token.size() > 1 && token[0] == '0') {
        token[0] = '1';
    }
    return s_IsTokenPosInt(token);
}

//  ----------------------------------------------------------------------------
static void init_symbol_type_table(void)
{
    if ( symbol_type_table[0] == 0 ) {
        for ( const char* s = "ACGNTU"; *s; ++s ) {
            int c = *s;
            symbol_type_table[c] |= fDNA_Main_Alphabet;
            c = tolower(c);
            symbol_type_table[c] |= fDNA_Main_Alphabet;
        }
        for ( const char* s = "BDHKMRSVWY"; *s; ++s ) {
            int c = *s;
            symbol_type_table[c] |= fDNA_Ambig_Alphabet;
            c = tolower(c);
            symbol_type_table[c] |= fDNA_Ambig_Alphabet;
        }
        for ( const char* s = "ACDEFGHIKLMNPQRSTVWYBZX"; *s; ++s ) {
            int c = *s;
            symbol_type_table[c] |= fProtein_Alphabet;
            c = tolower(c);
            symbol_type_table[c] |= fProtein_Alphabet;
        }
        symbol_type_table[(int)'-'] |= fDNA_Main_Alphabet | fProtein_Alphabet;
        symbol_type_table[(int)'*'] |= fProtein_Alphabet;
        for ( const char* s = "\r\n"; *s; ++s ) {
            int c = *s;
            symbol_type_table[c] |= fLineEnd;
        }
        for ( int c = 1; c < 256; ++c ) {
            if ( isalpha((unsigned char)c) )
                symbol_type_table[c] |= fAlpha;
            if ( isdigit((unsigned char)c) )
                symbol_type_table[c] |= fDigit;
            if ( isspace((unsigned char)c) )
                symbol_type_table[c] |= fSpace;
        }
        symbol_type_table[0] |= fInvalid;
    }
}


const char*
CFormatGuess::GetFormatName(EFormat format)
{
    auto formatIt = sm_FormatNames.find(format);
    if (formatIt == sm_FormatNames.end()) {
        NCBI_THROW(CUtilException, eWrongData,
                   "CFormatGuess::GetFormatName: out-of-range format value "
                   + NStr::IntToString(format));
    }
    return formatIt->second;
}


//  ============================================================================
//  Old style class interface:
//  ============================================================================

//  ----------------------------------------------------------------------------
CFormatGuess::ESequenceType
CFormatGuess::SequenceType(const char* str, unsigned length,
                           ESTStrictness strictness)
{
    if (length == 0)
        length = (unsigned)::strlen(str);

    init_symbol_type_table();
    unsigned int main_nuc_content = 0, ambig_content = 0, bad_nuc_content = 0,
        amino_acid_content = 0, exotic_aa_content = 0, bad_aa_content = 0;

    for (unsigned i = 0; i < length; ++i) {
        unsigned char c = str[i];
        unsigned char type = symbol_type_table[c];
        if ( type & fDNA_Main_Alphabet ) {
            ++main_nuc_content;
        } else if ( type & fDNA_Ambig_Alphabet ) {
            ++ambig_content;
        } else if ( !(type & (fSpace | fDigit)) ) {
            ++bad_nuc_content;
        }

        if ( type & fProtein_Alphabet ) {
            ++amino_acid_content;
        } else if ( type & fAlpha ) {
            ++exotic_aa_content;
        } else if ( !(type & (fSpace | fDigit)) ) {
            ++bad_aa_content;
        }
    }

    switch (strictness) {
    case eST_Lax:
    {
        double dna_content = (double)main_nuc_content / (double)length;
        double prot_content = (double)amino_acid_content / (double)length;

        if (dna_content > 0.7) {
            return eNucleotide;
        }
        if (prot_content > 0.7) {
            return eProtein;
        }
    }

    case eST_Default:
        if (bad_nuc_content + ambig_content <= main_nuc_content / 9
            ||  (bad_nuc_content + ambig_content <= main_nuc_content / 3  &&
                 bad_nuc_content <= (main_nuc_content + ambig_content) / 19)) {
            // >=90% main alph. (ACGTUN-) or >=75% main and >=95% 4na-encodable
            return eNucleotide;
        } else if (bad_aa_content + exotic_aa_content
                   <= amino_acid_content / 9) {
            // >=90% relatively standard protein residues.  (JOU don't count.)
            return eProtein;
        }

    case eST_Strict: // Must be 100% encodable
        if (bad_nuc_content == 0  &&  ambig_content <= main_nuc_content / 3) {
            return eNucleotide;
        } else if (bad_aa_content == 0
                   &&  exotic_aa_content <= amino_acid_content / 9) {
            return eProtein;
        }
    }

    return eUndefined;
}


//  ----------------------------------------------------------------------------
CFormatGuess::EFormat CFormatGuess::Format(const string& path, EOnError /*onerror*/)
{
    CNcbiIfstream input(path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    return Format(input);
}

//  ----------------------------------------------------------------------------
CFormatGuess::EFormat CFormatGuess::Format(CNcbiIstream& input, EOnError onerror)
{
    CFormatGuess FG( input );
    return FG.GuessFormat( onerror );
}


//  ============================================================================
//  New style object interface:
//  ============================================================================

//  ----------------------------------------------------------------------------
CFormatGuess::CFormatGuess()
    : m_Stream(* new CNcbiIfstream)
    , m_bOwnsStream(true)
    , m_iTestBufferSize(0)
{
    Initialize();
}

//  ----------------------------------------------------------------------------
CFormatGuess::CFormatGuess(
    const string& FileName )
    : m_Stream( * new CNcbiIfstream( FileName.c_str(), ios::binary ) )
    , m_bOwnsStream( true )
{
    Initialize();
}

//  ----------------------------------------------------------------------------
CFormatGuess::CFormatGuess(
    CNcbiIstream& Stream )
    : m_Stream( Stream )
    , m_bOwnsStream( false )
{
    Initialize();
}

//  ----------------------------------------------------------------------------
CFormatGuess::~CFormatGuess()
{
    delete[] m_pTestBuffer;
    if ( m_bOwnsStream ) {
        delete &m_Stream;
    }
}

//  ----------------------------------------------------------------------------
bool 
CFormatGuess::IsSupportedFormat(EFormat format) 
{
    for (size_t i = 0; i < sm_CheckOrder_Size; ++i) {
        if (sm_CheckOrder[i] == format) {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
CFormatGuess::EFormat
CFormatGuess::GuessFormat( EMode )
{
    return GuessFormat(eDefault);
}

//  ----------------------------------------------------------------------------
CFormatGuess::EFormat
CFormatGuess::GuessFormat( EOnError onerror )
{
    //sqd-4036:
    // make sure we got something to work with
    //
    if (!x_TestInput(m_Stream, onerror)) {
        return eUnknown;
    }
    if (!EnsureTestBuffer()) {
        //one condition that won't allow us to get a good test buffer is an ascii
        // file without any line breaks. so before giving up, let's specifically
        // try any formats that would allow for that:
        if(TestFormatNewick(eQuick)) {
            return CFormatGuess::eNewick;
        }
        return CFormatGuess::eUnknown;
    }

    EMode mode = eQuick;

    // First, try to use hints
    if ( !m_Hints.IsEmpty() ) {
        for (size_t f = 0; f < sm_CheckOrder_Size; ++f) {
            EFormat fmt = EFormat( sm_CheckOrder[f] );
            if (m_Hints.IsPreferred(fmt)  &&  x_TestFormat(fmt, mode)) {
                return fmt;
            }
        }
    }

    // Check other formats, skip the ones that are disabled through hints
    for (size_t f = 0; f < sm_CheckOrder_Size; ++f) {
        EFormat fmt = EFormat( sm_CheckOrder[f] );
        if ( ! m_Hints.IsDisabled(fmt)  &&  x_TestFormat(fmt, mode) ) {
            return fmt;
        }
    }
    return eUnknown;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormat( EFormat format, EMode )
{
    return TestFormat( format, eDefault);
}

//  ----------------------------------------------------------------------------
bool CFormatGuess::TestFormat(
    EFormat format,
    EOnError onerror )
{
    if (format != eUnknown && !x_TestInput(m_Stream, onerror)) {
        return false;
    }
    EMode mode = eQuick;
    return x_TestFormat(format, mode);
}

//  ----------------------------------------------------------------------------
bool CFormatGuess::x_TestFormat(EFormat format, EMode mode)
{
    // First check if the format is disabled
    if ( m_Hints.IsDisabled(format) ) {
        return false;
    }

    switch( format ) {

    case eBinaryASN:
        return TestFormatBinaryAsn( mode );
    case eRmo:
        return TestFormatRepeatMasker( mode );
    case eGtf:
        return TestFormatGtf( mode );
    case eGvf:
        return TestFormatGvf( mode );
    case eGff3:
        return TestFormatGff3( mode );
    case eGff2:
        return TestFormatGff2( mode );
    case eGlimmer3:
        return TestFormatGlimmer3( mode );
    case eAgp:
        return TestFormatAgp( mode );
    case eXml:
        return TestFormatXml( mode );
    case eNewick:
        return TestFormatNewick( mode );
    case eWiggle:
        return TestFormatWiggle( mode );
    case eBed:
        return TestFormatBed( mode );
    case eBed15:
        return TestFormatBed15( mode );
    case eAlignment:
        return TestFormatAlignment( mode );
    case eDistanceMatrix:
        return TestFormatDistanceMatrix( mode );
    case eFlatFileSequence:
        return TestFormatFlatFileSequence( mode );
    case eFiveColFeatureTable:
        return TestFormatFiveColFeatureTable( mode );
    case eSnpMarkers:
        return TestFormatSnpMarkers( mode );
    case eFasta:
        return TestFormatFasta( mode );
    case eTextASN:
        return TestFormatTextAsn( mode );
    case eTaxplot:
        return TestFormatTaxplot( mode );
    case ePhrapAce:
        return TestFormatPhrapAce( mode );
    case eTable:
        return TestFormatTable( mode );
    case eHgvs:
        return TestFormatHgvs( mode );
    case eZip:
        return TestFormatZip( mode );
    case eGZip:
        return TestFormatGZip( mode );
    case eZstd:
        return TestFormatZstd( mode );
    case eBZip2:
        return TestFormatBZip2( mode );
    case eLzo:
        return TestFormatLzo( mode );
    case eSra:
        return TestFormatSra( mode );
    case eBam:
        return TestFormatBam( mode );
    case ePsl:
        return TestFormatPsl( mode );
    case eVcf:
        return TestFormatVcf( mode );
    case eUCSCRegion:
        return false;
    case eGffAugustus:
        return TestFormatAugustus( mode );
    case eJSON:
        return TestFormatJson( mode );
    case eFlatFileGenbank:
        return TestFormatFlatFileGenbank( mode );
    case eFlatFileEna:
        return TestFormatFlatFileEna( mode );
    case eFlatFileUniProt:
        return TestFormatFlatFileUniProt( mode );
    default:
        NCBI_THROW( CCoreException, eInvalidArg,
            "CFormatGuess::x_TestFormat(): Unsupported format ID (" +
            NStr::NumericToString((int)format) + ")." );
    }
}

//  ----------------------------------------------------------------------------
void
CFormatGuess::Initialize()
{
    NCBI_ASSERT(eFormat_max == sm_FormatNames.size(),
        "sm_FormatNames does not list all possible formats");
    m_pTestBuffer = 0;

    m_bStatsAreValid = false;
    m_bSplitDone = false;
    m_iStatsCountData = 0;
    m_iStatsCountAlNumChars = 0;
    m_iStatsCountDnaChars = 0;
    m_iStatsCountAaChars = 0;
    m_iStatsCountBraces = 0;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::EnsureTestBuffer()
//  ----------------------------------------------------------------------------
{
    if ( m_pTestBuffer ) {
        return true;
    }
    if ( ! m_Stream.good() ) {
        return false;
    }

    // Fix to the all-comment problem.
    // Read a test buffer,
    // Test it for being all comment
    // If its all comment, read a twice as long buffer
    // Stop when its no longer all comment, end of the stream,
    //   or Multiplier hits 1024 

    const streamsize k_TestBufferGranularity = 8096;
    
    int Multiplier = 1;

    while(true) {
        m_iTestBufferSize = Multiplier * k_TestBufferGranularity;
        m_pTestBuffer = new char[ m_iTestBufferSize ];
        m_Stream.read( m_pTestBuffer, m_iTestBufferSize );
        m_iTestDataSize = m_Stream.gcount();
        if (m_iTestDataSize == 0) {
            delete[] m_pTestBuffer;
            m_pTestBuffer = 0;
            m_iTestBufferSize = 0;
            return false; //empty file
        } 
        m_Stream.clear();  // in case we reached eof
        CStreamUtils::Stepback( m_Stream, m_pTestBuffer, m_iTestDataSize );
        
        if (IsAllComment()) {
            if (Multiplier >= 1024)  {
                // this is how far we will go and no further.
                // if it's indeed all comments then none of the format specific 
                //  tests will assert.
                // if something was misidentified as a comment then the relevant 
                //  format specific test may still have a good sample to work with.
                // so it does not hurt to at least try.
                return true;
            }
            Multiplier *= 2;
            delete [] m_pTestBuffer;
            m_pTestBuffer = NULL;
            if (m_iTestDataSize < m_iTestBufferSize)  {
                return false;
            }
            continue;
        } else {
            break;
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::EnsureStats()
//  ----------------------------------------------------------------------------
{
    if ( m_bStatsAreValid ) {
        return true;
    }
    if ( ! EnsureTestBuffer() ) {
        return false;
    }

    string strBuffer(m_pTestBuffer, m_iTestDataSize);
    CNcbiIstrstream TestBuffer(strBuffer);
    string strLine;

    init_symbol_type_table();
    // Things we keep track of:
    //   m_iStatsCountAlNumChars: number of characters that are letters or
    //     digits
    //   m_iStatsCountData: number of characters not part of a line starting
    //     with '>', ignoring whitespace
    //   m_iStatsCountDnaChars: number of characters counted in m_iStatsCountData
    //     from the DNA alphabet
    //   m_iStatsCountAaChars: number of characters counted in m_iStatsCountData
    //     from the AA alphabet
    //  m_iStatsCountBraces: Opening { and closing } braces
    //
    while ( ! TestBuffer.fail() ) {
        NcbiGetline( TestBuffer, strLine, "\r\n" );
// code in CFormatGuess::Format counts line ends
// so, we will count them here as well
        if (!strLine.empty()) {
            strLine += '\n';
        }
        size_t size = strLine.size();
        bool is_header = size > 0 && strLine[0] == '>';
        for ( size_t i=0; i < size; ++i ) {
            unsigned char c = strLine[i];
            unsigned char type = symbol_type_table[c];

            if ( type & (fAlpha | fDigit | fSpace) ) {
                ++m_iStatsCountAlNumChars;
            }
            else if (c == '{'  ||  c == '}') {
                ++m_iStatsCountBraces;
            }
            if ( !is_header ) {
                if ( !(type & fSpace) ) {
                    ++m_iStatsCountData;
                }

                if ( type & fDNA_Main_Alphabet ) {
                    ++m_iStatsCountDnaChars;
                }
                if ( type & fProtein_Alphabet ) {
                    ++m_iStatsCountAaChars;
                }
            }
        }
    }
    m_bStatsAreValid = true;
    return true;
}

//  ----------------------------------------------------------------------------
bool CFormatGuess::x_TestInput( CNcbiIstream& input, EOnError onerror )
{
    if (!input) {
        if (onerror == eThrowOnBadSource) {
            NCBI_THROW(CUtilException,eNoInput,"Unreadable input stream");
        }
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CFormatGuess::TestFormatRepeatMasker(
    EMode /* not used */ )
{
    if ( ! EnsureStats() || ! EnsureSplitLines() ) {
        return false;
    }
    return IsInputRepeatMaskerWithHeader() ||
        IsInputRepeatMaskerWithoutHeader();
}


//  ----------------------------------------------------------------------------

static bool s_LooksLikeNucSeqData(const string& line, size_t minLength=10) {
    if (line.size()<minLength) {
        return false;
    }

    int nucCount=0;
    for (auto c : line) {
        if (isalpha(c)) {
            auto index = static_cast<int>(c);
            if (symbol_type_table[index] & fDNA_Main_Alphabet) {
                ++nucCount;
            }
            continue;
        }

        if (!isspace(c)) {
            return false;
        }
    }

    return (nucCount/line.size() > 0.9);
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatPhrapAce(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    if (memchr(m_pTestBuffer, 0, m_iTestDataSize)) { // Cannot contain NuLL bytes
        return false;                                // RW-1102
    }

    bool foundId = false;
    for (const auto& line : m_TestLines) {
        if (foundId) {
            if (s_LooksLikeNucSeqData(line)) {
                return true;
            }
        }
        else if (IsLinePhrapId(line)) {
            foundId = true;
        }
    }
    return false;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatGtf(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    unsigned int uGtfLineCount = 0;
    list<string>::iterator it = m_TestLines.begin();

    for ( ;  it != m_TestLines.end();  ++it) {
        //
        //  Make sure to ignore any UCSC track and browser lines prior to the
        //  start of data
        //
        if ( it->empty() || (*it)[0] == '#' ) {
            continue;
        }
        if ( !uGtfLineCount && NStr::StartsWith( *it, "browser " ) ) {
            continue;
        }
        if ( !uGtfLineCount && NStr::StartsWith( *it, "track " ) ) {
            continue;
        }
        if ( ! IsLineGtf( *it ) ) {
            return false;
        }
        ++uGtfLineCount;
    }
    return (uGtfLineCount != 0);
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatGvf(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    unsigned int uGvfLineCount = 0;
    list<string>::iterator it = m_TestLines.begin();

    for ( ;  it != m_TestLines.end();  ++it) {
        //
        //  Make sure to ignore any UCSC track and browser lines prior to the
        //  start of data
        //
        if ( it->empty() || (*it)[0] == '#' ) {
            if (NStr::StartsWith(*it, "##gvf-version")) {
                return true;
            }
            continue;
        }
        if ( !uGvfLineCount && NStr::StartsWith( *it, "browser " ) ) {
            continue;
        }
        if ( !uGvfLineCount && NStr::StartsWith( *it, "track " ) ) {
            continue;
        }
        if ( ! IsLineGvf( *it ) ) {
            return false;
        }
        ++uGvfLineCount;
    }
    return (uGvfLineCount != 0);
}


//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatGff3(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    unsigned int uGffLineCount = 0;
    list<string>::iterator it = m_TestLines.begin();

    for ( ;  it != m_TestLines.end();  ++it) {
        //
        //  Make sure to ignore any UCSC track and browser lines prior to the
        //  start of data
        //
        if (!uGffLineCount && NStr::StartsWith(*it, "##gff-version")) {
            return NStr::StartsWith(*it, "##gff-version 3");
        }
        if ( it->empty() || (*it)[0] == '#' ) {
            continue;
        }
        if ( !uGffLineCount && NStr::StartsWith( *it, "browser " ) ) {
            continue;
        }
        if ( !uGffLineCount && NStr::StartsWith( *it, "track " ) ) {
            continue;
        }
        if ( ! IsLineGff3( *it ) ) {
            return false;
        }
        ++uGffLineCount;
    }
    return (uGffLineCount != 0);
}


//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatAugustus(
    EMode /*not used*/)
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    unsigned int uGffLineCount = 0;
    list<string>::iterator it = m_TestLines.begin();

    for ( ;  it != m_TestLines.end();  ++it) {
        //
        //  Make sure to ignore any UCSC track and browser lines prior to the
        //  start of data
        //
        if (!uGffLineCount && NStr::StartsWith(*it, "##gff-version 3")) {
            return false;
        }
        if ( it->empty() || (*it)[0] == '#' ) {
            continue;
        }
        if ( !uGffLineCount && NStr::StartsWith( *it, "browser " ) ) {
            return false;
        }
        if ( !uGffLineCount && NStr::StartsWith( *it, "track " ) ) {
            return false;
        }
        if ( !IsLineAugustus( *it ) ) {
            return false;
        }
        ++uGffLineCount;
    }
    return (uGffLineCount != 0);
}


//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatGff2(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    unsigned int uGffLineCount = 0;
    list<string>::iterator it = m_TestLines.begin();

    for ( ;  it != m_TestLines.end();  ++it) {
        //
        //  Make sure to ignore any UCSC track and browser lines prior to the
        //  start of data
        //
        if ( it->empty() || (*it)[0] == '#' ) {
            continue;
        }
        if ( !uGffLineCount && NStr::StartsWith( *it, "browser " ) ) {
            continue;
        }
        if ( !uGffLineCount && NStr::StartsWith( *it, "track " ) ) {
            continue;
        }
        if ( ! IsLineGff2( *it ) ) {
            return false;
        }
        ++uGffLineCount;
    }
    return (uGffLineCount != 0);
}


//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatGlimmer3(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    /// first line should be a FASTA defline
    list<string>::iterator it = m_TestLines.begin();
    if (it->empty()  ||  (*it)[0] != '>') {
        return false;
    }
    
    /// there should be additional data lines, and they should be easily parseable, 
    ///  with five columns
    ++it;
    if (it == m_TestLines.end()) {
        return false;
    }
    for ( /**/;  it != m_TestLines.end();  ++it) {
        if ( !IsLineGlimmer3( *it ) ) {
            return false;
        }
    }
    return true;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatAgp(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }
    ITERATE( list<string>, it, m_TestLines ) {
        try {
            if ( !IsLineAgp( *it ) ) {
                return false;
            }
        } catch(...) {
            return false;
        }
    }
    return true;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatNewick(
    EMode /* not used */ )
{
//  -----------------------------------------------------------------------------
    // newick trees can be found in nexus files. check for that first as a special case
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        const int BUFFSIZE = 8096;
        if (m_pTestBuffer) {
            delete [] m_pTestBuffer;
        }
        m_pTestBuffer = new char[BUFFSIZE+1];
        m_Stream.read( m_pTestBuffer, BUFFSIZE );
        m_iTestDataSize = m_Stream.gcount();
        m_pTestBuffer[m_iTestDataSize] = 0;
        m_Stream.clear();  // in case we reached eof
        CStreamUtils::Stepback( m_Stream, m_pTestBuffer, m_iTestDataSize );
        m_TestLines.push_back(m_pTestBuffer);
    }

    // Note: We can live with false negatives. Avoid false positives
    // at all cost.

    bool is_nexus = false;
    bool has_trees = false;
    const size_t check_size = 12;

    ITERATE( list<string>, it, m_TestLines ) {
        if ( NPOS != it->find( "#NEXUS" ) ) {
            is_nexus = true;
        }
    }

    // Trees can be anywhere in a nexus file.  If nexus is true, 
    // try to read the whole file to see if there is a tree.
    if (is_nexus) {
        // Read in file one chunk at a time.  Readline would be better
        // but is not avialable for this stream.  Since the text we 
        // are looking for "begin trees;" may span two chunks, we
        // copy the last 12 characters of the previous chunk to
        // the front of the new one.
        const size_t read_size = 16384;
        char test_buf[read_size + check_size + 1];
        memset(test_buf, ' ', check_size); // "previous chunk" initially blank.

        size_t max_reads = 32768;  // max read to locate tree: 512 MB
        for (size_t i = 0; i < max_reads; ++i) {
            m_Stream.read(test_buf+check_size, read_size);
            size_t num_read = m_Stream.gcount();
            if (num_read > 0) {
                test_buf[num_read + check_size] = 0; // null terminator
                if (NPOS != NStr::FindNoCase(CTempString(test_buf), "begin trees;")) {
                    has_trees = true;
                    m_Stream.clear();  // in case we reached eof
                    break;
                }
                // copy end of buffer to beginning in case string
                // spans two buffers:
                strncpy(test_buf, test_buf + num_read, check_size);
            }

            if (m_Stream.eof() || m_Stream.fail()) {
                m_Stream.clear();  // clear eof
                break;
            }
        }
    }

    // In a nexus file with a tree, we will just read in the tree (ignoring for now
    // the alignment)
    if (is_nexus ) {
        if (has_trees)
            return true;
        return false;
    }

    //  special newick consideration:
    //  newick files may come with all data cramped into a single run-on line,
    //  that single oversized line may not have a line terminator
    const size_t maxSampleSize = 8*1024-1;
    size_t sampleSize = 0;
    char* pSample = new char[maxSampleSize+1];
    AutoArray<char> autoDelete(pSample);

    m_Stream.read(pSample, maxSampleSize);
    sampleSize = (size_t)m_Stream.gcount();
    m_Stream.clear();  // in case we reached eof
    CStreamUtils::Stepback(m_Stream, pSample, sampleSize);
    if (0 == sampleSize) {
        return false;
    }

    pSample[sampleSize] = 0;
    if (!IsSampleNewick(pSample)) { // tolerant of embedded line breaks
        return false;
    }
    return true;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatBinaryAsn(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }

    //
    //  Criterion: Presence of any non-printing characters
    //
    EConfidence conf = eNo;
    for (int i = 0;  i < m_iTestDataSize;  ++i) {
        if ( !isgraph((unsigned char) m_pTestBuffer[i])  &&
             !isspace((unsigned char) m_pTestBuffer[i]) )
        {
            if (m_pTestBuffer[i] == '\1') {
                conf = eMaybe;
            } else {
                return true;
            }
        }
    }
    return (conf == eYes);
}


//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatDistanceMatrix(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    //
    // criteria are odd:
    //
    list<string>::const_iterator iter = m_TestLines.begin();
    list<string> toks;

    /// first line: one token, one number
    NStr::Split(*iter++, "\t ", toks, NStr::fSplit_Tokenize);
    if (toks.size() != 1  ||
        toks.front().find_first_not_of("0123456789") != string::npos) {
        return false;
    }

    // now, for remaining ones, we expect an alphanumeric item first,
    // followed by a set of floating-point values.  Unless we are at the last
    // line, the number of values should increase monotonically
    for (size_t i = 1;  iter != m_TestLines.end();  ++i, ++iter) {
        toks.clear();
        NStr::Split(*iter, "\t ", toks, NStr::fSplit_Tokenize);
        if (toks.size() != i) {
            /// we can ignore the last line ; it may be truncated
            list<string>::const_iterator it = iter;
            ++it;
            if (it != m_TestLines.end()) {
                return false;
            }
        }

        list<string>::const_iterator it = toks.begin();
        for (++it;  it != toks.end();  ++it) {
            if ( ! s_IsTokenDouble( *it ) ) {
                return false;
            }
        }
    }

    return true;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatFlatFileSequence(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    ITERATE (list<string>, it, m_TestLines) {
        if ( !IsLineFlatFileSequence( *it ) ) {
            return false;
        }
    }
    return true;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatFiveColFeatureTable(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    ITERATE( list<string>, it, m_TestLines ) {
        if (it->empty()) {
            continue;
        }

        if (it->find(">Feature ") != 0 && it->find(">Features ") != 0) {
            return false;
        }
        break;
    }

    return true;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatXml(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }

    string input( m_pTestBuffer, (size_t)m_iTestDataSize );
    NStr::TruncateSpacesInPlace( input, NStr::eTrunc_Begin );

    //
    //  Test 1: If it starts with typical XML decorations such as "<?xml..."
    //  then respect that:
    //
    if ( NStr::StartsWith( input, "<?XML", NStr::eNocase ) ) {
        return true;
    }
    if ( NStr::StartsWith( input, "<!DOCTYPE", NStr::eNocase ) ) {
        return true;
    }

    //
    //  Test 2: In the absence of XML specific declarations, check whether the
    //  input starts with the opening tag of a well known set of doc types:
    //
    static const char* known_types[] = {
        "<Blast4-request>"
    };
    for ( size_t i=0; i < ArraySize(known_types); ++i ) {
        if ( NStr::StartsWith( input, known_types[i], NStr::eCase ) ) {
            return true;
        }
    }

    return false;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatAlignment(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }


    if (TestFormatCLUSTAL()) {
        return true;
    }

    // Alignment files come in all different shapes and broken formats,
    // and some of them are hard to recognize as such, in particular
    // if they have been hacked up in a text editor.

    // This functions only concerns itself with the ones that are
    // easy to recognize.

    // Note: We can live with false negatives. Avoid false positives
    // at all cost.

    ITERATE( list<string>, it, m_TestLines ) {
        if ( NPOS != it->find( "#NEXUS" ) ) {
            return true;
        }
    }
    return false;
}

//  -----------------------------------------------------------------------------
bool CFormatGuess::x_LooksLikeCLUSTALConservedInfo(const string& line) const
{

    for (auto c : line) {
        if ( isspace(c)) {
            continue;
        }

        if (c != ':' &&
            c != '*' &&
            c != '.') {
            return false;
        }
    }
    return true;
}

//  -----------------------------------------------------------------------------
bool CFormatGuess::x_TryProcessCLUSTALSeqData(const string& line, string& id, size_t& seg_length) const
{
    vector<string> toks;
    NStr::Split(line, " \t", toks, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    const size_t num_toks = toks.size();

    if (num_toks != 2 &&
        num_toks != 3) {
        return false;
    }

    const string& seqdata = toks[1];


    unsigned int cumulated_res = 0;
    if (num_toks == 3) {
        cumulated_res = NStr::StringToUInt(toks[2], NStr::fConvErr_NoThrow);
        if (cumulated_res == 0) {
            return false;
        }
    }

    // Check sequence data
    ESequenceType seqtype = 
        SequenceType(seqdata.c_str(), static_cast<unsigned int>(seqdata.size()), eST_Strict);

    if (seqtype == eUndefined) {
        return false;
    }
    
    if (num_toks == 3) {
        size_t num_gaps = count(seqdata.begin(), seqdata.end(), '-');
        if (((seqdata.size() - num_gaps) > cumulated_res)) {
            return false;
        }
    }


    id = toks[0];
    seg_length = seqdata.size();

    return true;
} 


//  -----------------------------------------------------------------------------

namespace { // anonymous namespace

struct SClustalBlockInfo
{
    bool m_InBlock;
    unsigned int m_Size;
    set<string> m_Ids;


    void Reset(void) {
        m_InBlock = false;
        m_Size = 0;
        m_Ids.clear();
    }

    SClustalBlockInfo() { Reset(); }
};

}

//  -----------------------------------------------------------------------------
bool 
CFormatGuess::TestFormatCLUSTAL()
{

    if (!EnsureTestBuffer()) {
        return false;
    }

    string strBuffer(m_pTestBuffer, m_iTestDataSize);
    CNcbiIstrstream TestBuffer(strBuffer);
    string strLine;

    SClustalBlockInfo block_info;

    bool has_valid_block = false;
    size_t seg_length = 0;
    size_t seg_length_prev = 0;


    const bool buffer_full = m_iTestDataSize == m_iTestBufferSize;

    while ( !TestBuffer.eof() ) {
        NcbiGetline(TestBuffer, strLine, "\r\n");

        if (buffer_full && 
            TestBuffer.eof()) { // Skip last line if buffer is full 
            break;              // to avoid misidentification due to line truncation
        }

        if (TestBuffer.fail()) {
            break;
        }

        if (NStr::StartsWith(strLine, "CLUSTAL")) {
            continue;    
        }

        if (NStr::IsBlank(strLine)) {
            if (block_info.m_InBlock) {
                if (block_info.m_Size < 2) {
                    return false;
                }
                block_info.Reset();
            }
            continue;
        }

        if (x_LooksLikeCLUSTALConservedInfo(strLine)) {
            if (! block_info.m_InBlock || block_info.m_Size<2) {
                return false;
            }
            block_info.Reset();
            continue;
        }

        string seq_id;
        if (!x_TryProcessCLUSTALSeqData(strLine, seq_id, seg_length)) {
            return false;
        }

        if (seg_length > 60) {
            return false;
        }
        if (block_info.m_InBlock) {
            if(seg_length != seg_length_prev) {
                return false;
            }
            has_valid_block = true;
        }

        if (block_info.m_Ids.find(seq_id) != block_info.m_Ids.end()) {
            return false;
        }
        block_info.m_Ids.insert(seq_id);

        seg_length_prev = seg_length;
        block_info.m_InBlock = true;
        ++(block_info.m_Size);
    }

    return has_valid_block;
}


//  -----------------------------------------------------------------------------
 bool 
 CFormatGuess::x_TestTableDelimiter(const string& delims)
 {
    list<string>::const_iterator iter = m_TestLines.begin();
    list<string> toks;

    // Skip initial lines since not all headers start with comments like # or ;:
    // Don't skip though if file is very short - add up to 3, 1 for each line 
    // over 5:
    for (size_t i=5; i<7; ++i)
        if (m_TestLines.size() > i) 
            ++iter;

    /// determine the number of observed columns
    size_t ncols = 0;
    for ( ;  iter != m_TestLines.end();  ++iter) {
        if (iter->empty()  ||  (*iter)[0] == '#'  ||  (*iter)[0] == ';') {
            continue;
        }

        toks.clear();
        NStr::Split(*iter, delims, toks, NStr::fSplit_Tokenize);
        ncols = toks.size();
        break;
    }
    if ( ncols < 2 ) {
        return false;
    }

    size_t nlines = 1;
    // verify that columns all have the same size
    // we can add an exception for the last line
    for ( ;  iter != m_TestLines.end();  ++iter) {
        if (iter->empty()  ||  (*iter)[0] == '#'  ||  (*iter)[0] == ';') {
            continue;
        } 

        toks.clear();
        NStr::Split(*iter, delims, toks, NStr::fSplit_Tokenize);
        if (toks.size() != ncols) {
            list<string>::const_iterator it = iter;
            ++it;
            if (it != m_TestLines.end() || (m_iTestDataSize < m_iTestBufferSize) ) {
                return false;
            }
        } else {
            ++nlines;
        }
        // Tokens should only contain printable characters
        for (const auto& token : toks) {
            auto it = find_if(token.begin(), token.end(), 
                    [](unsigned char c){ return !isprint(c); });
            if (it != token.end()) {
                return false;
            }
        }
    }
    return ( nlines >= 3 );
 }

bool
CFormatGuess::TestFormatTable(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }
    if ( ! IsAsciiText()) {//gp-13007:  "table" means "ascii table"
        return false;
    }

    //
    //  NOTE 1:
    //  There is a bunch of file formats that are a special type of table and
    //  that we want to identify (like Repeat Masker output). So not to shade
    //  out those more special formats, this test should be performed only after
    //  all the more specialized table formats have been tested.
    //

    //
    //  NOTE 2:
    //  The original criterion for this test was "the same number of observed
    //  columns in every line".
    //  In order to weed out false positives the following *additional*
    //  conditions have been imposed:
    //  - there are at least two observed columns
    //  - the sample contains at least two non-comment lines.
    //

    //' ' ' \t' '\t' ',' '|'
    if (x_TestTableDelimiter(" "))
        return true;
    else if (x_TestTableDelimiter(" \t"))
        return true;
    else if (x_TestTableDelimiter("\t"))
        return true;
    else if (x_TestTableDelimiter(","))
        return true;
    else if (x_TestTableDelimiter("|"))
        return true;

    return false;
}

//  -----------------------------------------------------------------------------
void SkipCommentAndBlank(CTempString &text)
{
    const CTempString COMMENT_SYMBOLS(";#!");
    const CTempString NEW_LINE_SYMBOLS("\r\n");
    while (true) {
        text = NStr::TruncateSpaces_Unsafe(text, NStr::eTrunc_Begin);
        if ( COMMENT_SYMBOLS.find(text[0]) != CTempString::npos ) {
            CTempString::size_type pos = text.find_first_of(NEW_LINE_SYMBOLS, 1);
            text = text.substr(pos);
        } else {
            break;
        }
    }
}

bool
CFormatGuess::TestFormatFasta( EMode /* not used */ )
{
    if ( ! EnsureStats() ) {
        return false;
    }

    // reject obvious misfits:
    CTempString header(m_pTestBuffer, m_iTestDataSize);
    SkipCommentAndBlank(header);
    if ( m_iTestDataSize == 0 || header.length() == 0 || header[0] != '>' ) {
        return false;
    }
    if ( m_iStatsCountData == 0 ) {
        if (0.75 > double(m_iStatsCountAlNumChars)/double(m_iTestDataSize) ) {
            return false;
        }
        return ( NStr::Find( m_pTestBuffer, "|" ) <= 10 );
    }

    // remaining decision based on text stats:
    double dAlNumFraction = (double)m_iStatsCountAlNumChars / (double)m_iTestDataSize;
    double dDnaFraction   = (double)m_iStatsCountDnaChars / (double)m_iStatsCountData;
    double dAaFraction    = (double)m_iStatsCountAaChars / (double)m_iStatsCountData;

    // want at least 80% text-ish overall:
    if ( dAlNumFraction < 0.8 ) {
        return false;
    }

    // want more than 91 percent of either DNA content or AA content in what we
    // presume is data:
    if ( dDnaFraction > 0.91 || dAaFraction > 0.91 ) {
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatTextAsn(
    EMode /* not used */ )
{
    if ( ! EnsureStats() ) {
        return false;
    }

    // reject obvious misfits:
    if ( m_iTestDataSize == 0 || m_pTestBuffer[0] == '>' ) {
        return false;
    }

    // criteria:
    // at least 80% text-ish,
    // 1st field of the first non-blank not comment line must start with letter.
    // "::=" as the 2nd field of the first non-blank non comment line.
    //
    double dAlNumFraction = (double)(m_iStatsCountAlNumChars+m_iStatsCountBraces) /
                            (double)m_iTestDataSize;
    if ( dAlNumFraction < 0.80 ) {
        return false;
    }

    string strBuffer(m_pTestBuffer, m_iTestDataSize);
    CNcbiIstrstream TestBuffer(strBuffer);
    string strLine;

    while ( ! TestBuffer.fail() ) {
        vector<string> Fields;
        NcbiGetline(TestBuffer, strLine, "\n\r");
        NStr::Split(strLine, " \t", Fields, NStr::fSplit_Tokenize);
        if ( IsAsnComment( Fields  ) ) {
            continue;
        }
        return ( Fields.size() >= 2 && Fields[1] == "::=" && isalpha(Fields[0][0]));
    }
    return false;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatTaxplot(
    EMode /* not used */ )
{
    return false;
}

//  -----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatSnpMarkers(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }
    ITERATE( list<string>, it, m_TestLines ) {
        string str = *it;
        int rsid, chr, pos, numMatched;
        numMatched = sscanf( it->c_str(), "rs%d\t%d\t%d", &rsid, &chr, &pos);
        if ( numMatched == 3) {
            return true;
        }
    }
    return false;  
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatBed(
    EMode /* not used */ )
{
    if ( ! EnsureStats() || ! EnsureSplitLines() ) {
        return false;
    }

    bool bTrackLineFound( false );    
    bool bHasStartAndStop ( false );
    size_t columncount = 0;
    ITERATE( list<string>, it, m_TestLines ) {
        string str = NStr::TruncateSpaces( *it );
        if ( str.empty() ) {
            continue;
        }
        
        // 'chr 8' fixup, the bedreader does this too
        if (str.find("chr ") == 0 || 
            str.find("Chr ") == 0 || 
            str.find("CHR ") == 0)
            str.erase(3, 1);

        //
        //  while occurrence of the following decorations _is_ a good sign, they could
        //  also be indicator for a variety of other UCSC data formats
        //
        if ( NStr::StartsWith( str, "track" ) ) {
            bTrackLineFound = true;
            continue;
        }
        if ( NStr::StartsWith( str, "browser" ) ) {
            continue;
        }
        if ( NStr::StartsWith( str, "#" ) ) {
            continue;
        }

        vector<string> columns;
        NStr::Split(str, " \t", columns, NStr::fSplit_Tokenize);
        if (columns.size() < 3 || columns.size() > 12) {
            return false;
        }
        if ( columns.size() != columncount ) {
            if ( columncount == 0 ) {
                columncount = columns.size();
            }
            else {
                return false;
            }
        }
        if(columns.size() >= 3) {
            if (s_IsTokenPosInt(columns[1]) &&
                s_IsTokenPosInt(columns[2])) {
                bHasStartAndStop = true;
            }
        }
    }

    return (bHasStartAndStop || bTrackLineFound);
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatBed15(
    EMode /* not used */ )
{
    if ( ! EnsureStats() || ! EnsureSplitLines() ) {
        return false;
    }

    bool LineFound = false;
    size_t columncount = 15;
    ITERATE( list<string>, it, m_TestLines ) {
        if ( NStr::TruncateSpaces( *it ).empty() ) {
            continue;
        }
        //
        //  while occurrence of the following decorations _is_ a good sign, they could
        //  also be indicator for a variety of other UCSC data formats
        //
        if ( NStr::StartsWith( *it, "track" ) ) {
            continue;
        }
        if ( NStr::StartsWith( *it, "browser" ) ) {
            continue;
        }
        if ( NStr::StartsWith( *it, "#" ) ) {
            continue;
        }

        vector<string> columns;
        NStr::Split(*it, " \t", columns, NStr::fSplit_Tokenize);
        if ( columns.size() != columncount ) {
            return false;
        } else {
            if (!s_IsTokenPosInt(columns[1]) ||   //chr start
                !s_IsTokenPosInt(columns[2]) ||   //chr end
                !s_IsTokenPosInt(columns[4]) ||   //score
                !s_IsTokenPosInt(columns[6]) ||   //thick draw start
                !s_IsTokenPosInt(columns[7]))     //thick draw end
                    return false;
            string strand = NStr::TruncateSpaces(columns[5]);
            
            if (strand != "+" && strand != "-")
                return false;

            LineFound = true;
        }
    }
    return LineFound;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatWiggle(
    EMode /* not used */ )
{
    if ( ! EnsureStats() || ! EnsureSplitLines() ) {
        return false;
    }
    ITERATE( list<string>, it, m_TestLines ) {
        if ( NStr::StartsWith( *it, "track" ) ) {
            if ( NStr::Find( *it, "type=wiggle_0" ) != NPOS ) {
                return true;
            }
            if ( NStr::Find( *it, "type=bedGraph" ) != NPOS ) {
                return true;
            }
        }
        if ( NStr::StartsWith(*it, "fixedStep") ) { /* MSS-140 */
            if ( NStr::Find(*it, "chrom=")  &&  NStr::Find(*it, "start=") ) {
                return true;
            } 
        }
        if ( NStr::StartsWith(*it, "variableStep") ) { /* MSS-140 */
            if ( NStr::Find(*it, "chrom=") ) {
                return true;
            }
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatHgvs(
    EMode /* not used */ )
{
    if ( ! EnsureStats() || ! EnsureSplitLines() ) {
        const int BUFFSIZE = 1024;
        if (m_pTestBuffer) {
            delete [] m_pTestBuffer;
        }
        m_pTestBuffer = new char[BUFFSIZE+1];
        m_Stream.read( m_pTestBuffer, BUFFSIZE );
        m_iTestDataSize = m_Stream.gcount();
        m_pTestBuffer[m_iTestDataSize] = 0;
        m_Stream.clear();  // in case we reached eof
        CStreamUtils::Stepback( m_Stream, m_pTestBuffer, m_iTestDataSize );
        m_TestLines.push_back(m_pTestBuffer);
    }

    unsigned int uHgvsLineCount = 0;
    list<string>::iterator it = m_TestLines.begin();

    for ( ;  it != m_TestLines.end();  ++it) {
        if ( it->empty() || (*it)[0] == '#' ) {
            continue;
        }
        if ( ! IsLineHgvs( *it ) ) {
            return false;
        }
        ++uHgvsLineCount;
    }
    return (uHgvsLineCount != 0);
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatZip(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    // check if the first two bytes match with the zip magic number: 0x504B,
    // or PK and the next two bytes match with any of 0x0102, 0x0304, 0x0506
    // and 0x0708.
    if ( m_iTestDataSize < 4) {
        return false;
    }
    if (m_pTestBuffer[0] == 'P'  &&  m_pTestBuffer[1] == 'K'  &&
        ((m_pTestBuffer[2] == (char)1  &&  m_pTestBuffer[3] == (char)2)  ||
         (m_pTestBuffer[2] == (char)3  &&  m_pTestBuffer[3] == (char)4)  ||
         (m_pTestBuffer[2] == (char)5  &&  m_pTestBuffer[3] == (char)6) ||
         (m_pTestBuffer[2] == (char)7  &&  m_pTestBuffer[3] == (char)8) ) ) {
        return true;
    }
    return false;
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatGZip(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    // check if the first two bytes match the gzip magic number: 0x1F8B
    if ( m_iTestDataSize < 2) {
        return false;
    }
    if (m_pTestBuffer[0] == (char)31  &&  m_pTestBuffer[1] == (char)139) {
        return true;
    }
    return false;
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatZstd(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }
    // check if the first 4 bytes match with the zstd magic number: 0xFD2FB528
    if ( m_iTestDataSize < 4) {
        return false;
    }
    if (m_pTestBuffer[0] == (char)0x28  &&  
        m_pTestBuffer[1] == (char)0xB5  &&
        m_pTestBuffer[2] == (char)0x2F  &&
        m_pTestBuffer[3] == (char)0xFD  ) {
        return true;
    }
    return false;
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatBZip2(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }

    // check if the first two bytes match with the bzip2 magic number: 0x425A,
    // or 'BZ' and the next two bytes match with 0x68(h) and 0x31-39(1-9)
    if ( m_iTestDataSize < 4) {
        return false;
    }

    if (m_pTestBuffer[0] == 'B'  &&  m_pTestBuffer[1] == 'Z'  &&
        m_pTestBuffer[2] == 'h'  &&  m_pTestBuffer[3] >= '1'  &&
        m_pTestBuffer[3] <= '9') {
        return true;
    }

    return false;
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::TestFormatLzo(
    EMode /* not used */ )
{
    if ( ! EnsureTestBuffer() ) {
        return false;
    }

    if (m_iTestDataSize >= 3  &&  m_pTestBuffer[0] == 'L'  &&
        m_pTestBuffer[1] == 'Z'  &&  m_pTestBuffer[2] == 'O') {
        if (m_iTestDataSize == 3  ||
            (m_iTestDataSize > 3  &&  m_pTestBuffer[3] == '\0')) {
            return true;
        }
    }

    if (m_iTestDataSize >= 4  &&  m_pTestBuffer[1] == 'L'  &&
        m_pTestBuffer[2] == 'Z'  &&  m_pTestBuffer[3] == 'O') {
        if (m_iTestDataSize == 4  ||
            (m_iTestDataSize > 4  &&  m_pTestBuffer[4] == '\0')) {
            return true;
        }
    }

    return false;
}


bool CFormatGuess::TestFormatSra(EMode /* not used */ )
{
    if ( !EnsureTestBuffer()  ||  m_iTestDataSize < 16
        ||  CTempString(m_pTestBuffer, 8) != "NCBI.sra") {
        return false;
    }

    if (m_pTestBuffer[8] == '\x05'  &&  m_pTestBuffer[9] == '\x03'
        &&  m_pTestBuffer[10] == '\x19'  &&  m_pTestBuffer[11] == '\x88') {
        return true;
    } else if (m_pTestBuffer[8] == '\x88'  &&  m_pTestBuffer[9] == '\x19'
        &&  m_pTestBuffer[10] == '\x03'  &&  m_pTestBuffer[11] == '\x05') {
        return true;
    } else {
        return false;
    }
}

bool CFormatGuess::TestFormatBam(EMode mode)
{
    //rw-9:
    // the original heuristic to "guess" at the content of a gzip archive
    // broke down and we found a whole class of false positives for the
    // BAM format- on our very own FTP site no less!
    //To really be sure we are dealing indeed with BAM we would have to
    // decompress the beginning of the archive and peek inside- however, gzip
    // decompression is not available in this module.

    //If reliable BAM detection is needed, use objtools/readers/format_guess_ex
    // instead. It's a drop in replacement, and it's not any slower for any file
    // format that can be detected reliably here (because it calls this code
    // before doing any of the more fancy stuff). And because of the fancy
    // stuff it does, it will even classify some files this code can't (though
    // possibly at considerable extra expense).
    return false;
}


bool CFormatGuess::TestFormatPsl(EMode mode)
{
    // for the most part, following https://genome.ucsc.edu/FAQ/FAQformat.html#format2.
    // note that UCSC downloads often include one extra column, right at the start
    // of each line. If that's the case then all records have that extra column. Since 
    // UCSC downloads are common we will also accept as PSL anything that follows the 
    // spec after the first column of every line has been tossed.
    // Note that I have also seen "#" columns but only at the very beginning of a file.
    //
    if ( ! EnsureTestBuffer() || ! EnsureSplitLines() ) {
        return false;
    }

    bool ignoreFirstColumn = false;
    unsigned int uPslLineCount = 0;
    list<string>::iterator it = m_TestLines.begin();
    while (it != m_TestLines.end()  &&  NStr::StartsWith(*it, "#")) {
        it++;
    }
    if (it == m_TestLines.end()) {
        return false;
    }
    if (!IsLinePsl(*it, ignoreFirstColumn)) {
        ignoreFirstColumn = true;
        if (!IsLinePsl(*it, ignoreFirstColumn)) {
            return false;
        }
    }
    uPslLineCount++;
    it++;
    for ( ;  it != m_TestLines.end();  ++it) {
        if ( ! IsLinePsl(*it, ignoreFirstColumn) ) {
            return false;
        }
        uPslLineCount++;
    }
    return (uPslLineCount != 0);
}

//  ----------------------------------------------------------------------------
bool
GenbankGetKeywordLine(
    list<string>::iterator& lineIt,
    list<string>::iterator endIt,
    string& keyword,
    string& data)
//  ----------------------------------------------------------------------------
{
    if (lineIt == endIt) {
        return false;
    }
    if (lineIt->size() > 79) {
        return false;
    }

    vector<int> validIndents = {0, 2, 3, 5, 12, 21};
    auto firstNotBlank = lineIt->find_first_not_of(" ");
    while (firstNotBlank != 0) {
        if (std::find(validIndents.begin(), validIndents.end(), firstNotBlank) == 
                validIndents.end()) {
            auto firstNotBlankOrDigit = lineIt->find_first_not_of(" 1234567890");
            if (firstNotBlankOrDigit != 10) {
                return false;
            }
        }
        lineIt++;
        if (lineIt == endIt) {
            return false;
        }
        firstNotBlank = lineIt->find_first_not_of(" ");
    }
    try {
        NStr::SplitInTwo(
            *lineIt, " ", keyword, data, NStr::fSplit_MergeDelimiters);
    }
    catch (CException&) {
        return false;
    }
    lineIt++;
    return true;
}

//  ----------------------------------------------------------------------------
bool CFormatGuess::TestFormatFlatFileGenbank(
    EMode /*unused*/)
{
    // see ftp://ftp.ncbi.nih.gov/genbank/gbrel.txt

    if ( ! EnsureStats() || ! EnsureSplitLines() ) {
        return false;
    }

    // smell test:
    // note: sample size at least 8000 characters, line length soft limited to
    //  80 characters
    if (m_TestLines.size() < 9) { // number of required records 
        return false;
    }
    
    string keyword, data, lookingFor;
    auto recordIt = m_TestLines.begin();
    auto endIt = m_TestLines.end();
    NStr::SplitInTwo(
        *recordIt, " ", keyword, data, NStr::fSplit_MergeDelimiters);

    lookingFor = "LOCUS"; // excactly one
    if (keyword != lookingFor) {
        return false;
    }
    recordIt++;
    if (!GenbankGetKeywordLine(recordIt, endIt, keyword, data)) {
        return false;
    }

    lookingFor = "DEFINITION"; // one or more
    if (keyword != lookingFor) {
        return false;
    }
    while (keyword == lookingFor) {
        if (!GenbankGetKeywordLine(recordIt, endIt, keyword, data)) {
            return false;
        }
    }

    lookingFor = "ACCESSION"; // one or more
    if (keyword != lookingFor) {
        return false;
    }
    while (keyword == lookingFor) {
        if (!GenbankGetKeywordLine(recordIt, endIt, keyword, data)) {
            return false;
        }
    }

    bool nidSeen = false;
    lookingFor = "NID"; // zero or one, can come before or after VERSION
    if (keyword == lookingFor) {
        nidSeen = true;
        if (!GenbankGetKeywordLine(recordIt, endIt, keyword, data)) {
            return false;
        }
    }

    lookingFor = "VERSION"; // exactly one
    if (keyword != lookingFor) {
        return false;
    }
    if (!GenbankGetKeywordLine(recordIt, endIt, keyword, data)) {
        return false;
    }

    if (!nidSeen) {
        lookingFor = "NID"; // zero or one
        if (keyword == lookingFor) {
            if (!GenbankGetKeywordLine(recordIt, endIt, keyword, data)) {
                return false;
            }
        }
    }

    lookingFor = "PROJECT"; // zero or more
    while (keyword == lookingFor) {
        if (!GenbankGetKeywordLine(recordIt, endIt, keyword, data)) {
            return false;
        }
    }
    
    lookingFor = "DBLINK"; // zero or more
    while (keyword == lookingFor) {
        if (!GenbankGetKeywordLine(recordIt, endIt, keyword, data)) {
            return false;
        }
    }

    lookingFor = "KEYWORDS"; // one or more
    if (keyword != lookingFor) {
        return false;
    }

    // I am convinced now. There may be flaws farther down but this input 
    //  definitely wants to be a Genbank flat file.
    return true;
}

//  ----------------------------------------------------------------------------
bool
EnaGetLineData(
    list<string>::iterator& lineIt,
    list<string>::iterator endIt,
    string& lineCode,
    string& lineData)
//  ----------------------------------------------------------------------------
{
    while (lineIt != endIt  &&  NStr::StartsWith(*lineIt, "XX")) {
        lineIt++;
    }
    if (lineIt == endIt) {
        return false;
    }
    try {
        NStr::SplitInTwo(
            *lineIt, " ", lineCode, lineData, NStr::fSplit_MergeDelimiters);
    }
    catch(CException&) {
        lineCode = *lineIt;
        lineData = "";
    }
    lineIt++;
    return true;
}
    
//  ----------------------------------------------------------------------------
bool CFormatGuess::TestFormatFlatFileEna(
    EMode /*unused*/)
{
    // see: ftp://ftp.ebi.ac.uk/pub/databases/ena/sequence/release/doc/usrman.txt

    if ( ! EnsureStats() || ! EnsureSplitLines() ) {
        return false;
    }

    // smell test:
    // note: sample size at least 8000 characters, line length soft limited to
    //  78 characters
    if (m_TestLines.size() < 19) { // number of required records 
        return false;
    }
    
    string lineCode, lineData, lookingFor;
    auto recordIt = m_TestLines.begin();
    auto endIt = m_TestLines.end();
    NStr::SplitInTwo(
        *recordIt, " ", lineCode, lineData, NStr::fSplit_MergeDelimiters);

    lookingFor = "ID"; // excactly one
    if (lineCode != lookingFor) {
        return false;
    }
    recordIt++;

    lookingFor = "AC"; // one or more
    if (!EnaGetLineData(recordIt, endIt, lineCode, lineData)) {
        return false;
    }
    if (lineCode != lookingFor) {
        return false;
    }
    while (lineCode == lookingFor) {
        if (!EnaGetLineData(recordIt, endIt, lineCode, lineData)) {
            return false;
        }
    }

    lookingFor = "PR"; // zero or more
    while (lineCode == lookingFor) {
        if (!EnaGetLineData(recordIt, endIt, lineCode, lineData)) {
            return false;
        }
    }

    lookingFor = "DT"; // two (first hard difference from UniProt)
    for (int i = 0; i < 2; ++i) {
        if (lineCode != lookingFor) {
            return false;
        }
        if (!EnaGetLineData(recordIt, endIt, lineCode, lineData)) {
            return false;
        }
    }

    lookingFor = "DE"; // one or more
    if (lineCode != lookingFor) {
        return false;
    }
    while (lineCode == lookingFor) {
        if (!EnaGetLineData(recordIt, endIt, lineCode, lineData)) {
            return true;
        }
    }

    lookingFor = "KW"; // one or more
    if (lineCode != lookingFor) {
        return false;
    }
    while (lineCode == lookingFor) {
        if (!EnaGetLineData(recordIt, endIt, lineCode, lineData)) {
            return true;
        }
    }

    lookingFor = "OS"; // one or more
    if (lineCode != lookingFor) {
        return false;
    }
    while (lineCode == lookingFor) {
        if (!EnaGetLineData(recordIt, endIt, lineCode, lineData)) {
            return true;
        }
    }

    lookingFor = "OC"; // one or more
    if (lineCode != lookingFor) {
        return false;
    }
    while (lineCode == lookingFor) {
        if (!EnaGetLineData(recordIt, endIt, lineCode, lineData)) {
            return true;
        }
    }

    //  once here it's Ena or someone is messing with me
    return true;
}

//  ----------------------------------------------------------------------------
bool
UniProtGetLineData(
    list<string>::iterator& lineIt,
    list<string>::iterator endIt,
    string& lineCode,
    string& lineData)
//  ----------------------------------------------------------------------------
{
    if (lineIt == endIt) {
        return false;
    }
    try {
        NStr::SplitInTwo(
            *lineIt, " ", lineCode, lineData, NStr::fSplit_MergeDelimiters);
    }
    catch(CException&) {
        lineCode = *lineIt;
        lineData = "";
    }
    lineIt++;
    return true;
}
    
//  ----------------------------------------------------------------------------
bool CFormatGuess::TestFormatFlatFileUniProt(
    EMode /*unused*/)
{
    // see: https://web.expasy.org/docs/userman.html#genstruc

    if ( ! EnsureStats() || ! EnsureSplitLines() ) {
        return false;
    }

    // smell test:
    // note: sample size at least 8000 characters, line length soft limited to
    //  75 characters
    if (m_TestLines.size() < 15) { // number of required records 
        return false;
    }

    // note:
    // we are only trying to assert that the input is *meant* to be uniprot. 
    // we should not be in the business of validation - this should happen 
    // downstream, with better error messages than we could possibly provide here.
    string lineCode, lineData, lookingFor;
    auto recordIt = m_TestLines.begin();
    auto endIt = m_TestLines.end();
    NStr::SplitInTwo(
        *recordIt, " ", lineCode, lineData, NStr::fSplit_MergeDelimiters);

    lookingFor = "ID"; // excatly one
    if (lineCode != lookingFor) {
        return false;
    }
    recordIt++;

    lookingFor = "AC"; // one or more
    if (!UniProtGetLineData(recordIt, endIt, lineCode, lineData)) {
        return false;
    }
    if (lineCode != lookingFor) {
        return false;
    }
    while (lineCode == lookingFor) {
        if (!UniProtGetLineData(recordIt, endIt, lineCode, lineData)) {
            return false;
        }
    }

    lookingFor = "DT"; // three (first hard difference from UniProt)
    for (int i = 0; i < 3; ++i) {
        if (lineCode != lookingFor) {
            return false;
        }
        if (!UniProtGetLineData(recordIt, endIt, lineCode, lineData)) {
            return false;
        }
    }


    lookingFor = "DE"; // one or more
    if (lineCode != lookingFor) {
        return false;
    }
    while (lineCode == lookingFor) {
        if (!UniProtGetLineData(recordIt, endIt, lineCode, lineData)) {
            return true;
        }
    }

    // optional "GN" line or first "OS" line
    if (lineCode != "GN"  &&  lineCode != "OS") {
        return false;
    }
    
    //  once here it's UniProt or someone is messing with me
    return true;
}

//  ----------------------------------------------------------------------------
bool CFormatGuess::TestFormatVcf(
    EMode)
{
    // Currently, only look for the header line identifying the VCF version.
    // Waive requirement this be the first line, but still expect it to by
    // in the initial sample.
    if ( ! EnsureStats() || ! EnsureSplitLines() ) {
        return false;
    }

    ITERATE( list<string>, it, m_TestLines ) {
        if (NStr::StartsWith(*it, "##fileformat=VCFv")) {
            return true;
        }
    }
    return false;
}
//  ----------------------------------------------------------------------------


//  ----------------------------------------------------------------------------
void CFormatGuess::x_StripJsonStrings(string& testString) const
{
    list<size_t> limits;
    x_FindJsonStringLimits(testString, limits);

    // If no strings found
    if ( limits.empty() ) {
        return;
    }

    if (limits.size()%2 == 1) { 
        // Perhaps testString ends on an open string 
        // Tack on an additional set of quotes at the end
        testString += "\"";
        limits.push_back(testString.size()-1);       
    }
    // The length of the limits container is now even

    // Iterate over string start and stop sites
    // Strip strings and copy what remains to complement
    string complement = "";

    auto it = limits.begin();
    size_t comp_interval_start = 0;
    while (it != limits.end()) {
        const size_t string_start = *it++;
        if (string_start > comp_interval_start) {
            const size_t comp_interval_length = string_start-comp_interval_start;
            complement += testString.substr(comp_interval_start, comp_interval_length);
        }

        const size_t string_stop = *it++;
        comp_interval_start = string_stop+1;
    }

    if (comp_interval_start < testString.size()) {
        complement += testString.substr(comp_interval_start);
    }

    testString = complement;
    return;
}
//  ----------------------------------------------------------------------------


//  ----------------------------------------------------------------------------
void CFormatGuess::x_FindJsonStringLimits(const string& input, list<size_t>& limits) const
{
    limits.clear();
    const string& double_quotes = R"(")";

    bool is_start = true;
    size_t pos = NStr::Find(input, double_quotes);
    // List all string start and stop positions
    while ( pos != NPOS ) {
        limits.push_back(pos);
        if (is_start) {
            pos = x_FindNextJsonStringStop(input, pos+1);
        } else {
            pos = NStr::Find(input, double_quotes, pos+1);
        }
        is_start = !is_start;
    }
}
//  ----------------------------------------------------------------------------


//  ----------------------------------------------------------------------------
size_t s_GetPrecedingFslashCount(const string& input, const size_t pos)
{
    if (pos == 0 ||
        pos >= input.size() ||
        NStr::IsBlank(input) ) 
    {
        return 0;
    }

    int current_pos = static_cast<int>(pos)-1; 
    size_t num_fslash = 0;
    while  ( current_pos >= 0 && input[current_pos] == '\\' ) {
        ++num_fslash;
        --current_pos;
    }
    return num_fslash;
}
//  ----------------------------------------------------------------------------


//  ----------------------------------------------------------------------------
size_t CFormatGuess::x_FindNextJsonStringStop(const string& input, const size_t from_pos) const
{
    const string& double_quotes = R"(")";
    size_t pos = NStr::Find(input, double_quotes, from_pos);

    // Double quotes immediately preceded by an odd number of forward 
    // slashes, for example, /", ///", are escaped
    while (pos != NPOS) {
        const size_t num_fslash = s_GetPrecedingFslashCount(input, pos);
        // If the number of forward slashes is even,
        // return the position of the double quotes
        if (num_fslash%2 == 0) {
            break;
        }
        pos = NStr::Find(input, double_quotes, pos+1);
    }   
    return pos;
}
//  ----------------------------------------------------------------------------


//  ----------------------------------------------------------------------------
bool CFormatGuess::x_CheckStripJsonNumbers(string& testString) const
{
    if (NStr::IsBlank(testString)) {
        return true;
    }

    list<string> subStrings;
    // Split on white space
    NStr::Split(testString, " \r\t\n", subStrings, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    for (auto it = subStrings.cbegin(); it != subStrings.cend(); ++it) {
        const string subString = *it;

        if (!x_IsNumber(subString)) { // The last substring might be a truncated number or keyword
           ++it;
           if (it == subStrings.cend()) {
               testString = subString;
               return true;
           }
           return false;
        }
    }

    testString.clear();
    return true;
}
//  ----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
bool CFormatGuess::x_IsTruncatedJsonNumber(const string& testString) const
{
    // Truncation of a JSON number may result strings of the following type:
    //  1.1e
    //  1.1E
    //  1.7E-
    //  +
    //  -
    // NStr::StringToDouble cannot handle such truncations, but we can "fix"
    // the truncation by appending zero ("0") to the truncated string

    const string extendedString = testString + "0"; 

    return x_IsNumber(extendedString);
}
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
bool CFormatGuess::x_IsNumber(const string& testString) const 
{
    try {
        NStr::StringToDouble(testString);
    } 
    catch (...) {
        return false;
    }
    return true;
}
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
bool CFormatGuess::x_IsTruncatedJsonKeyword(const string& testString) const
{
    const size_t stringSize = testString.size();
    // nul, tru, fals
    if (stringSize > 4) {
        return false;
    }

    const string nullString("null");
    const string trueString("true");
    const string falseString("false");

    if (testString == nullString.substr(0, stringSize) ||
        testString == trueString.substr(0, stringSize) ||
        testString == falseString.substr(0, stringSize)) {
        return true;
    }

    return false;
}
// -----------------------------------------------------------------------------


//  ----------------------------------------------------------------------------
bool CFormatGuess::x_IsBlankOrNumbers(const string& testString) const 
{
    if (NStr::IsBlank(testString)) {
        return true;
    }

    list<string> numStrings;
    // Split on white space
    NStr::Split(testString, " \r\t\n", numStrings, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    for (auto numString : numStrings) {
        if (!x_IsNumber(numString)) {
            return false;
        }
    }

    return true;
}
//  ----------------------------------------------------------------------------


//  ----------------------------------------------------------------------------
bool CFormatGuess::x_CheckStripJsonPunctuation(string& testString) const 
{
    // Parentheses are prohibited
    if (testString.find_first_of("()") != string::npos) {
        return false;
    }

    const size_t punctuation_threshold = 4;

    // Reject if the number of punctuation characters falls below some threshold value.
    // In this case, the threshold is hardcoded to 4.
    if (x_StripJsonPunctuation(testString) < punctuation_threshold) {
        return false;
    }

    return true;
}
//  ----------------------------------------------------------------------------


//  ----------------------------------------------------------------------------
size_t CFormatGuess::x_StripJsonPunctuation(string& testString) const 
{
    size_t initial_len = testString.size();

    NStr::ReplaceInPlace(testString, "{", "");
    NStr::ReplaceInPlace(testString, "}", "");
    NStr::ReplaceInPlace(testString, "[", "");
    NStr::ReplaceInPlace(testString, "]", "");
    NStr::ReplaceInPlace(testString, ":", "");
    NStr::ReplaceInPlace(testString, ",", "");

    return testString.size() - initial_len;
}
//  ----------------------------------------------------------------------------


//  ----------------------------------------------------------------------------
void CFormatGuess::x_StripJsonKeywords(string& testString) const 
{
    NStr::ReplaceInPlace(testString, "true", "");
    NStr::ReplaceInPlace(testString, "false", "");
    NStr::ReplaceInPlace(testString, "null", "");
}
//  ----------------------------------------------------------------------------


//  ----------------------------------------------------------------------------
bool CFormatGuess::x_CheckJsonStart(const string& testString) const 
{
    if (NStr::StartsWith(testString, "{")) {
        // Next character must begin a string
        const auto next_pos = testString.find_first_not_of("( \t\r\n",1); 
        if (next_pos != NPOS && testString[next_pos] == '\"') {
            return true;
        }
    } 
    else
    if (NStr::StartsWith(testString, "[")) {
        return true;
    }

    return false;
}
//  ----------------------------------------------------------------------------


//  ----------------------------------------------------------------------------
bool CFormatGuess::TestFormatJson(
        EMode)
{

    // Convert the test-buffer character array to a string
    string testString(m_pTestBuffer, m_iTestDataSize);

    if ( NStr::IsBlank(testString) ) {
        return false;
    }

    NStr::TruncateSpacesInPlace(testString, NStr::eTrunc_Begin);

    if (!x_CheckJsonStart(testString)) {
        return false;
    }

    x_StripJsonStrings(testString);

    if ( !x_CheckStripJsonPunctuation(testString) ) {
        return false;
    }

    x_StripJsonKeywords(testString);

    if (!x_CheckStripJsonNumbers(testString)) {
        return false;
    }

    if ( NStr::IsBlank(testString) ) {
        return true;
    }

    // What remains is either a truncated number
    // or a truncated keyword
    return x_IsTruncatedJsonNumber(testString) | 
           x_IsTruncatedJsonKeyword(testString);
}
//  ----------------------------------------------------------------------------


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsInputRepeatMaskerWithHeader()
{
    //
    //  Repeatmasker files consist of columnar data with a couple of lines
    //  of column labels prepended to it (but sometimes someone strips those
    //  labels).
    //  This function tries to identify repeatmasker data by those column
    //  label lines. They should be the first non-blanks in the file.
    //
    string labels_1st_line[] = { "SW", "perc", "query", "position", "matching", "" };
    string labels_2nd_line[] = { "score", "div.", "del.", "ins.", "sequence", "" };

    //
    //  Purge junk lines:
    //
    list<string>::iterator it = m_TestLines.begin();
    for  ( ; it != m_TestLines.end(); ++it ) {
        NStr::TruncateSpacesInPlace( *it );
        if ( *it != "" ) {
            break;
        }
    }

    if ( it == m_TestLines.end() ) {
        return false;
    }

    //
    //  Verify first line of labels:
    //
    size_t current_offset = 0;
    for ( size_t i=0; labels_1st_line[i] != ""; ++i ) {
        current_offset = NStr::FindCase( *it, labels_1st_line[i], current_offset );
        if ( current_offset == NPOS ) {
            return false;
        }
    }

    //
    //  Verify second line of labels:
    //
    ++it;
    if ( it == m_TestLines.end() ) {
        return false;
    }
    current_offset = 0;
    for ( size_t j=0; labels_2nd_line[j] != ""; ++j ) {
        current_offset = NStr::FindCase( *it, labels_2nd_line[j], current_offset );
        if ( current_offset == NPOS ) {
            return false;
        }
    }

    //
    //  Should have at least one extra line:
    //
    ++it;
    if ( it == m_TestLines.end() ) {
        return false;
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsInputRepeatMaskerWithoutHeader()
{
    //
    //  Repeatmasker files consist of columnar data with a couple of lines
    //  of column labels prepended to it (but sometimes someone strips those
    //  labels).
    //  This function assumes the column labels have been stripped and attempts
    //  to identify RMO by checking the data itself.
    //

    //
    //  We declare the data as RMO if we are able to parse every record in the
    //  sample we got:
    //
    ITERATE( list<string>, it, m_TestLines ) {
        string str = NStr::TruncateSpaces( *it );
        if ( str == "" ) {
            continue;
        }
        if ( ! IsLineRmo( str ) ) {
            return false;
        }
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::IsSampleNewick(
    const string& cline )
//  ----------------------------------------------------------------------------
{
    //  NOTE:
    //  See http://evolution.genetics.washington.edu/phylip/newick_doc.html
    //
    //  Note that Newick tree tend to be written out as a single long line. Thus,
    //  we are most likely only seeing the first part of a tree.
    //

    //  NOTE:
    //  MSS-112 introduced the concept of multitree files is which after the ";" 
    //  another tree may start. The new logic accepts files as Newick if they 
    //  are Newick up to and including the first semicolon. It does not look
    //  beyond.

    string line = NStr::TruncateSpaces( cline );
    if ( line.empty()  ||  line[0] != '(') {
        return false;
    }
    {{
        //  Strip out comments:
        string trimmed;
        bool in_comment = false;
        for ( size_t ii=0; line.c_str()[ii] != 0; ++ii ) {
            if ( ! in_comment ) {
                if ( line.c_str()[ii] != '[' ) {
                    trimmed += line.c_str()[ii];
                }
                else {
                    in_comment = true;
                }
            }
            else /* in_comment */ {
                if ( line.c_str()[ii] == ']' ) {
                    in_comment = false;
                }
            }
        }
        line = trimmed;
    }}
    {{
        //  Compress quoted labels:
        string trimmed;
        bool in_quote = false;
        for ( size_t ii=0; line.c_str()[ii] != 0; ++ii ) {
            if ( ! in_quote ) {
                if ( line.c_str()[ii] != '\'' ) {
                    trimmed += line.c_str()[ii];
                }
                else {
                    in_quote = true;
                    trimmed += 'A';
                }
            }
            else { /* in_quote */
                if ( line.c_str()[ii] == '\'' ) {
                    in_quote = false;
                }
            }
        }
        line = trimmed;
    }}
    {{
        //  Strip distance markers:
        string trimmed;
        size_t ii=0;
        while ( line.c_str()[ii] != 0 ) {
            if ( line.c_str()[ii] != ':' ) {
                trimmed += line.c_str()[ii++];
            }
            else {
                ii++;
                if ( line.c_str()[ii] == '-'  || line.c_str()[ii] == '+' ) {
                    ii++;
                }
                while ( '0' <= line.c_str()[ii] && line.c_str()[ii] <= '9' ) {
                    ii++;
                }
                if ( line.c_str()[ii] == '.' ) {
                    ii++;
                    while ( '0' <= line.c_str()[ii] && line.c_str()[ii] <= '9' ) {
                        ii++;
                    }
                }
            }
        }
        line = trimmed;
    }}
    {{
        //  Rough lexical analysis of what's left. Bail immediately on fault:
        if (line.empty()  ||  line[0] != '(') {
            return false;
        }
        size_t paren_count = 1;
        for ( size_t ii=1; line.c_str()[ii] != 0; ++ii ) {
            switch ( line.c_str()[ii] ) {
                default: 
                    break;
                case '(':
                    ++paren_count;
                    break;
                case ')':
                    if ( paren_count == 0 ) {
                        return false;
                    }
                    --paren_count;
                    break;
                case ',':
                    if ( paren_count == 0 ) {
                        return false;
                    }
                    break;
                case ';':
//                    if ( line[ii+1] != 0 ) {
//                        return false;
//                    }
                    break;
            }
        }
    }}
    return true; 
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineFlatFileSequence(
    const string& line )
{
    // blocks of ten residues (or permitted punctuation characters)
    // with a count at the start or end; require at least four
    // (normally six)
    SIZE_TYPE pos = line.find_first_not_of("0123456789 \t");
    if (pos == NPOS  ||  pos + 45 >= line.size()) {
        return false;
    }

    for (SIZE_TYPE i = 0;  i < 45;  ++i) {
        char c = line[pos + i];
        if (i % 11 == 10) {
            if ( !isspace(c) ) {
                return false;
            }
        } else {
            if ( !isalpha(c)  &&  c != '-'  &&  c != '*') {
                return false;
            }
        }
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLabelNewick(
    const string& label )
{
    //  Starts with a string of anything other than "[]:", optionally followed by
    //  a single ':', followed by a number, optionally followed by a dot and
    //  another number.
    if ( NPOS != label.find_first_of( "[]" ) ) {
        return false;
    }
    size_t colon = label.find( ':' );
    if ( NPOS == colon ) {
        return true;
    }
    size_t dot = label.find_first_not_of( "0123456789", colon + 1 );
    if ( NPOS == dot ) {
        return true;
    }
    if ( label[ dot ] != '.' ) {
        return false;
    }
    size_t end = label.find_first_not_of( "0123456789", dot + 1 );
    return ( NPOS == end );
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineAgp( 
    const string& strLine )
{
    //
    //  Note: The reader allows for line and endline comments starting with a '#'.
    //  So we accept them here, too.
    //
    string line( strLine );
    size_t uCommentStart = NStr::Find( line, "#" );

    if ( NPOS != uCommentStart ) {
        line = line.substr( 0, uCommentStart );
    }
    NStr::TruncateSpacesInPlace( line );
    if ( line.empty() ) {
        return true;
    }

    vector<string> tokens;
    if ( NStr::Split(line, " \t", tokens, NStr::fSplit_Tokenize).size() < 8 ) {
        return false;
    }

    if ( tokens[1].size() > 1 && tokens[1][0] == '-' ) {
        tokens[1][0] = '1';
    }
    if ( -1 == NStr::StringToNonNegativeInt( tokens[1] ) ) {
        return false;
    }

    if ( tokens[2].size() > 1 && tokens[2][0] == '-' ) {
        tokens[2][0] = '1';
    }
    if ( -1 == NStr::StringToNonNegativeInt( tokens[2] ) ) {
        return false;
    }

    if ( tokens[3].size() > 1 && tokens[3][0] == '-' ) {
        tokens[3][0] = '1';
    }
    if ( -1 == NStr::StringToNonNegativeInt( tokens[3] ) ) {
        return false;
    }

    if ( tokens[4].size() != 1 || NPOS == tokens[4].find_first_of( "ADFGPNOW" ) ) {
        return false;
    }
    if ( tokens[4] == "N" ) {
        if ( -1 == NStr::StringToNonNegativeInt( tokens[5] ) ) {
            return false;
        }
    }
    else {
        if ( -1 == NStr::StringToNonNegativeInt( tokens[6] ) ) {
            return false;
        }
        if ( -1 == NStr::StringToNonNegativeInt( tokens[7] ) ) {
            return false;
        }            
        if ( tokens.size() != 9 ) {
            return false;
        }
        if ( tokens[8].size() != 1 || NPOS == tokens[8].find_first_of( "+-" ) ) {
            return false;
        }
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineGlimmer3(
    const string& line )
{
    list<string> toks;
    NStr::Split(line, "\t ", toks, NStr::fSplit_Tokenize);
    if (toks.size() != 5) {
        return false;
    }

    list<string>::iterator i = toks.begin();

    /// first column: skip (ascii identifier)
    ++i;

    /// second, third columns: both ints
    if ( ! s_IsTokenInteger( *i++ ) ) {
        return false;
    }
    if ( ! s_IsTokenInteger( *i++ ) ) {
        return false;
    }

    /// fourth column: int in the range of -3...3
    if ( ! s_IsTokenInteger( *i ) ) {
        return false;
    }
    int frame = NStr::StringToInt( *i++ );
    if (frame < -3  ||  frame > 3) {
        return false;
    }

    /// fifth column: score; double
    if ( ! s_IsTokenDouble( *i ) ) {
        return false;
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineGtf(
    const string& line )
{
    vector<string> tokens;
    if ( NStr::Split(line, " \t", tokens, NStr::fSplit_Tokenize).size() < 8 ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[3] ) ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[4] ) ) {
        return false;
    }
    if ( ! s_IsTokenDouble( tokens[5] ) ) {
        return false;
    }
    if ( tokens[6].size() != 1 || NPOS == tokens[6].find_first_of( ".+-" ) ) {
        return false;
    }
    if ( tokens[7].size() != 1 || NPOS == tokens[7].find_first_of( ".0123" ) ) {
        return false;
    }
    if ( tokens.size() < 9 || 
         (NPOS == tokens[8].find( "gene_id" ) && NPOS == tokens[8].find( "transcript_id" ) ) ) {
        return false;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineGvf(
    const string& line )
//  ----------------------------------------------------------------------------
{
    vector<string> tokens;
    if ( NStr::Split(line, " \t", tokens, NStr::fSplit_Tokenize).size() < 8 ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[3] ) ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[4] ) ) {
        return false;
    }

    //make sure that "type" is a GVF admissible value:
    {{
        bool typeOk = false;
        list<string> terms;
        terms.push_back("snv");
        terms.push_back("cnv");
        terms.push_back("copy_number_variation");
        terms.push_back("gain");
        terms.push_back("copy_number_gain");
        terms.push_back("loss");
        terms.push_back("copy_number_loss");
        terms.push_back("loss_of_heterozygosity");
        terms.push_back("complex");
        terms.push_back("complex_substitution");
        terms.push_back("complex_sequence_alteration");
        terms.push_back("indel");
        terms.push_back("insertion");
        terms.push_back("inversion");
        terms.push_back("substitution");
        terms.push_back("deletion");
        terms.push_back("duplication");
        terms.push_back("translocation");
        terms.push_back("upd");
        terms.push_back("uniparental_disomy");
        terms.push_back("maternal_uniparental_disomy");
        terms.push_back("paternal_uniparental_disomy");
        terms.push_back("tandom_duplication");
        terms.push_back("structural_variation");
        terms.push_back("sequence_alteration");
        ITERATE(list<string>, termiter, terms) {
            if(NStr::EqualNocase(*termiter, tokens[2])) {
                typeOk = true;
                break;
            }
        }
        if (!typeOk) {
            return false;
        }
    }}
    if ( ! s_IsTokenDouble( tokens[5] ) ) {
        return false;
    }
    if ( tokens[6].size() != 1 || NPOS == tokens[6].find_first_of( ".+-" ) ) {
        return false;
    }
    if ( tokens[7].size() != 1 || NPOS == tokens[7].find_first_of( ".0123" ) ) {
        return false;
    }

    //make sure all the mandatory attributes are present:
    string attrs = tokens[8];
    if (string::npos == attrs.find("ID="))
        return false;
    if (string::npos == attrs.find("Variant_seq=")) {
        return false;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineGff3(
    const string& line )
{
    vector<string> tokens;
    if ( NStr::Split(line, " \t", tokens, NStr::fSplit_Tokenize).size() < 8 ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[3] ) ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[4] ) ) {
        return false;
    }
    if ( ! s_IsTokenDouble( tokens[5] ) ) {
        return false;
    }
    if ( tokens[6].size() != 1 || NPOS == tokens[6].find_first_of( ".+-?" ) ) {
        return false;
    }
    if ( tokens[7].size() != 1 || NPOS == tokens[7].find_first_of( ".0123" ) ) {
        return false;
    }
    if ( tokens.size() < 9 || tokens[8].empty()) {
        return false;
    }
    if ( tokens.size() >= 9 && tokens[8].size() > 1) {
        const string& col9 = tokens[8];
        if ( NPOS == NStr::Find(col9, "ID") &&
             NPOS == NStr::Find(col9, "Parent") &&
             NPOS == NStr::Find(col9, "Target") &&
             NPOS == NStr::Find(col9, "Name") &&
             NPOS == NStr::Find(col9, "Alias") &&
             NPOS == NStr::Find(col9, "Note") &&
             NPOS == NStr::Find(col9, "Dbxref") &&
             NPOS == NStr::Find(col9, "Xref") ) {
            return false;
        }
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineAugustus(
    const string& line )
{
    vector<string> tokens;
    string remaining(line), head, tail;

    //column 0: ID, string
    if (!NStr::SplitInTwo(remaining, " \t", head, tail)) {
        return false;
    }
    remaining = tail;

    //column 1: method, most likely "AUGUSTUS" but don't want to rely on this
    if (!NStr::SplitInTwo(remaining, " \t", head, tail)) {
        return false;
    }
    remaining = tail;

    //column 2: feature type, controlled vocabulary
    if (!NStr::SplitInTwo(remaining, " \t", head, tail)) {
        return false;
    }
    remaining = tail;
    string featureType = head;

    //column 3: start, integer
    if (!NStr::SplitInTwo(remaining, " \t", head, tail)  ||  !s_IsTokenPosInt(head)) {
        return false;
    }
    remaining = tail;

    //column 4: stop, integer
    if (!NStr::SplitInTwo(remaining, " \t", head, tail)  ||  !s_IsTokenPosInt(head)) {
        return false;
    }
    remaining = tail;

    //column 5: score, double
    if (!NStr::SplitInTwo(remaining, " \t", head, tail)  ||  !s_IsTokenDouble(head)) {
        return false;
    }
    remaining = tail;

    //column 6: strand, one in "+-.?"
    const string legalStrands{"+-.?"};
    if (!NStr::SplitInTwo(remaining, " \t", head, tail)  ||  head.size() != 1  ||  
            string::npos == legalStrands.find(head)) {
        return false;
    }
    remaining = tail;

    //column 7: phase, one in ".0123"
    const string legalPhases{".0123"};
    if (!NStr::SplitInTwo(remaining, " \t", head, tail)  ||  head.size() != 1  ||  
            string::npos == legalPhases.find(head)) {
        return false;
    }
    remaining = tail;

    //everything else: attributes, format depends on featureType
    if (remaining.empty()) {
        return false;
    }

    if (featureType == "gene") {
        if (NPOS != NStr::Find(remaining, ";")) {
            return false;
        }
        if (NPOS != NStr::Find(remaining, " ")) {
            return false;
        }
        return true;
    }
    if (featureType == "transcript") {
        if (NPOS != NStr::Find(remaining, ";")) {
            return false;
        }
        if (NPOS != NStr::Find(remaining, " ")) {
            return false;
        }
        return true;
    }
    if (NPOS == NStr::Find(remaining, "transcript_id")) {
        return false;
    }
    if (NPOS == NStr::Find(remaining, "gene_id")) {
        return false;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineGff2(
    const string& line )
{
    vector<string> tokens;
    const size_t num_cols = NStr::Split(line, " \t", tokens, NStr::fSplit_Tokenize).size();
    if ( num_cols < 8 ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[3] ) ) {
        return false;
    }
    if ( ! s_IsTokenPosInt( tokens[4] ) ) {
        return false;
    }
    if ( ! s_IsTokenDouble( tokens[5] ) ) {
        return false;
    }
    if ( tokens[6].size() != 1 || NPOS == tokens[6].find_first_of( ".+-" ) ) {
        return false;
    }
    if ( tokens[7].size() != 1 || NPOS == tokens[7].find_first_of( ".0123" ) ) {
        return false;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLinePhrapId(
    const string& line )
{
    vector<string> values;
    if ( NStr::Split(line, " \t", values, NStr::fSplit_Tokenize).empty() ) {
        return false;
    }

    //
    //  Old style: "^DNA \\w+ "
    //
    if ( values[0] == "DNA" ) {
        return true;
    }

    //
    //  New style: "^AS [0-9]+ [0-9]+"
    //
    if ( values[0] == "AS" ) {
        return ( 0 <= NStr::StringToNonNegativeInt( values[1] ) &&
          0 <= NStr::StringToNonNegativeInt( values[2] ) );
    }

    return false;
}


//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineRmo(
    const string& line )
{
    const size_t MIN_VALUES_PER_RECORD = 14;

    //
    //  Make sure there is enough stuff on that line:
    //
    list<string> values;
    if ( NStr::Split(line, " \t", values, NStr::fSplit_Tokenize).size() < MIN_VALUES_PER_RECORD ) {
        return false;
    }

    //
    //  Look at specific values and make sure they are of the correct type:
    //

    //  1: positive integer:
    list<string>::iterator it = values.begin();
    if ( ! s_IsTokenPosInt( *it ) ) {
        return false;
    }

    //  2: float:
    ++it;
    if ( ! s_IsTokenDouble( *it ) ) {
        return false;
    }

    //  3: float:
    ++it;
    if ( ! s_IsTokenDouble( *it ) ) {
        return false;
    }

    //  4: float:
    ++it;
    if ( ! s_IsTokenDouble( *it ) ) {
        return false;
    }

    //  5: string, not checked
    ++it;

    //  6: positive integer:
    ++it;
    if ( ! s_IsTokenPosInt( *it ) ) {
        return false;
    }

    //  7: positive integer:
    ++it;
    if ( ! s_IsTokenPosInt( *it ) ) {
        return false;
    }

    //  8: positive integer, likely in paretheses, not checked:
    ++it;

    //  9: '+' or 'C':
    ++it;
    if ( *it != "+" && *it != "C" ) {
        return false;
    }

    //  and that's enough for now. But there are at least two more fields 
    //  with values that look testable.

    return true;
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::IsLinePsl(
    const string& line,
    bool ignoreFirstLine)
//  ----------------------------------------------------------------------------
{
    vector<string> tokens;
    int firstColumn = (ignoreFirstLine ? 1 : 0);
    NStr::Split(line, " \t", tokens, NStr::fSplit_Tokenize);
    if (tokens.size() - firstColumn != 21) {
        return false;
    }
    // first 8 columns are positive integers:
    for (auto column = firstColumn; column < firstColumn + 8; ++column) {
        if (!s_IsTokenPosInt(tokens[column]) ) {
            return false;
        }
    }
    // next is one or two "+" or "-":
    const string& token = tokens[firstColumn + 8];
    if (token.empty()  ||  token.size() > 2) {
        return false;
    }
    if (token.find_first_not_of("-+") != string::npos) {
        return false;
    }
    // columns 11 - 13 are positive integers:
    for (auto column = firstColumn + 10; column < firstColumn + 13; ++column) {
        if (!s_IsTokenPosInt(tokens[column]) ) {
            return false;
        }
    }
    // columns 15 - 18 are positive integers:
    for (auto column = firstColumn + 14; column < firstColumn + 18; ++column) {
        if (!s_IsTokenPosInt(tokens[column]) ) {
            return false;
        }
    }
    
    // the following is disabled because we want to allow incorrect but recognizable
    //  PSL to pass as PSL.
    //  This will hopefully give the user a better error message as to what is wrong
    //  with the data than we can do within the constraints of the format guesser.
#if 0
    int blockCount = NStr::StringToInt(tokens[firstColumn + 17]);
    // columns 19 - 21 are comma separated lists of positive integers, list size
    //  must be equal to blockCount
    for (auto column = firstColumn + 18; column < firstColumn + 21; ++column) {
        vector<string> hopefullyInts;
        NStr::Split(tokens[column], ",", hopefullyInts, NStr::fSplit_Tokenize);
        if (hopefullyInts.size() != blockCount) {
            return false;
        }
        for (auto hopefulInt: hopefullyInts) {
            if (!s_IsTokenPosInt(hopefulInt) ) {
                return false;
            }
        }
    }
#endif
    return true;
}


//  ----------------------------------------------------------------------------
bool
CFormatGuess::IsAsnComment(
    const vector<string>& Fields )
{
    if ( Fields.size() == 0 ) {
        return true;
    }
    return ( NStr::StartsWith( Fields[0], "--" ) );
}

//  ----------------------------------------------------------------------------

bool
CFormatGuess::EnsureSplitLines()
//  ----------------------------------------------------------------------------
{
    if ( m_bSplitDone ) {
        return !m_TestLines.empty();
    }
    m_bSplitDone = true;

    //
    //  Make sure the given data is ASCII before checking potential line breaks:
    //
    const size_t MIN_HIGH_RATIO = 20;
    size_t high_count = 0;
    for ( streamsize i=0; i < m_iTestDataSize; ++i ) {
        if ( 0x80 & m_pTestBuffer[i] ) {
            ++high_count;
        }
    }
    if ( 0 < high_count && m_iTestDataSize / high_count < MIN_HIGH_RATIO ) {
        return false;
    }

    //
    //  Let's expect at least one line break in the given data:
    //
    string data( m_pTestBuffer, (size_t)m_iTestDataSize );
    m_TestLines.clear();

    if ( string::npos != data.find("\r\n") ) {
        NStr::Split(data, "\r\n", m_TestLines, NStr::fSplit_Tokenize);
    }
    else if ( string::npos != data.find("\n") ) {
        NStr::Split(data, "\n", m_TestLines, NStr::fSplit_Tokenize);
    }
    else if ( string::npos != data.find("\r") ) {
        NStr::Split(data, "\r", m_TestLines, NStr::fSplit_Tokenize);
    }
    else if ( m_iTestDataSize == m_iTestBufferSize) {
        //most likely single truncated line
        return false;
    }
    else {
        //test buffer contains the entire file
        m_TestLines.push_back(data);
    }

    if ( m_iTestDataSize == m_iTestBufferSize   &&  m_TestLines.size() > 1 ) {
        //multiple lines, last likely truncated
        m_TestLines.pop_back();
    }
    return !m_TestLines.empty();
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::IsAsciiText()
{
    const double REQUIRED_ASCII_RATIO = 0.9;

    // first stab - are we text?  comments are only valid if we are text
    size_t count = 0;
    size_t count_print = 0;
    for (int i = 0;  i < m_iTestDataSize;  ++i, ++count) {
        if (isprint((unsigned char) m_pTestBuffer[i])) {
            ++count_print;
        }
    }
    if (count_print < (double)count * REQUIRED_ASCII_RATIO) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CFormatGuess::IsAllComment()
{
    if (!IsAsciiText()) {
        return false;
    }

    m_bSplitDone = false;
    m_TestLines.clear();
    EnsureSplitLines();

    ITERATE(list<string>, it, m_TestLines) {
        if(it->empty()) {
            continue;
        }
        if (NStr::StartsWith(*it, "#")) {
            continue;
        }
        if(NStr::StartsWith(*it, "--")) {
            continue;
        }
        return false;
    }
    
    return true;
}

//  ----------------------------------------------------------------------------
bool CFormatGuess::IsLineHgvs(
    const string& line )
{
    // This simple check can mistake Newwick, so Newwick is checked first
    //  /[:alnum:]+:(g|c|r|p|m|mt|n)\.[:alnum:]+/  as in NC_000001.9:g.1234567C>T
    int State = 0;
    ITERATE(string, Iter, line) {
        char Char = *Iter;
        char Next = '\0';
        string::const_iterator NextI = Iter;
        ++NextI;
        if(NextI != line.end())
            Next = *NextI;
        
        if(State == 0) {
            if( isalnum(Char) )
                State++;
        } else if(State == 1) {
            if(Char == ':')
                State++;
        } else if(State == 2) {
            if (Char == 'g' ||
                Char == 'c' ||
                Char == 'r' ||
                Char == 'p' ||
                Char == 'n' ||
                Char == 'm' ) {
                State++;
                if (Char=='m' && Next == 't') {
                    ++Iter;
                }
            } else {
                return false;
            }
        } else if(State == 3) {
            if(Char == '.') 
                State++;
            else
                return false;
        } else if(State == 4) {
            if( isalnum(Char) )
                State++;
        }
    }
    
    return (State == 5);    
}



END_NCBI_SCOPE
