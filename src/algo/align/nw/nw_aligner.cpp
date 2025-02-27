/* $Id$
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
 * Authors:  Yuri Kapustin, Boris Kiryutin, Alexander Souvorov
 *
 * File Description:  CNWAligner implementation
 *                   
 * ===========================================================================
 *
 */


#include <ncbi_pch.hpp>

#include "nw_aligner_threads.hpp"
#include "messages.hpp"

#include <corelib/ncbi_system.hpp>
#include <algo/align/nw/align_exception.hpp>
#include <algo/align/nw/nw_formatter.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>

#include <algorithm>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CNWAligner::CNWAligner()
    : m_Wm(GetDefaultWm()),
      m_Wms(GetDefaultWms()),
      m_Wg(GetDefaultWg()),
      m_Ws(GetDefaultWs()),
      m_esf_L1(false), m_esf_R1(false), m_esf_L2(false), m_esf_R2(false),
      m_SmithWaterman(false),
      m_GapPreference(eLater),
      m_abc(g_nwaligner_nucleotides),
      m_ScoreMatrixInvalid(true),
      m_prg_callback(0),
      m_terminate(false),
      m_Seq1Vec(),
      m_Seq1(0), m_SeqLen1(0),
      m_Seq2Vec(),
      m_Seq2(0), m_SeqLen2(0),
      m_PositivesAsMatches(false),
      m_score(kInfMinus),
      m_mt(false),
      m_maxthreads(1),
      m_MaxMem(GetDefaultSpaceLimit())
{
    SetScoreMatrix(0);
}


CNWAligner::CNWAligner( const char* seq1, size_t len1,
                        const char* seq2, size_t len2,
                        const SNCBIPackedScoreMatrix* scoremat )

    : m_Wm(GetDefaultWm()),
      m_Wms(GetDefaultWms()),
      m_Wg(GetDefaultWg()),
      m_Ws(GetDefaultWs()),
      m_esf_L1(false), m_esf_R1(false), m_esf_L2(false), m_esf_R2(false),
      m_SmithWaterman(false),
      m_GapPreference(eLater),
      m_abc(g_nwaligner_nucleotides),
      m_ScoreMatrixInvalid(true),
      m_prg_callback(0),
      m_terminate(false),
      m_Seq1Vec(&seq1[0], &seq1[0]+len1),
      m_Seq1(&m_Seq1Vec[0]), m_SeqLen1(len1),
      m_Seq2Vec(&seq2[0], &seq2[0]+len2),
      m_Seq2(&m_Seq2Vec[0]), m_SeqLen2(len2),
      m_PositivesAsMatches(false),
      m_score(kInfMinus),
      m_mt(false),
      m_maxthreads(1),
      m_MaxMem(GetDefaultSpaceLimit())
{
    SetScoreMatrix(scoremat);
    SetSequences(seq1, len1, seq2, len2);
}


CNWAligner::CNWAligner(const string& seq1,
                       const string& seq2,
                       const SNCBIPackedScoreMatrix* scoremat)
    : m_Wm(GetDefaultWm()),
      m_Wms(GetDefaultWms()),
      m_Wg(GetDefaultWg()),
      m_Ws(GetDefaultWs()),
      m_esf_L1(false), m_esf_R1(false), m_esf_L2(false), m_esf_R2(false),
      m_SmithWaterman(false),
      m_GapPreference(eLater),
      m_abc(g_nwaligner_nucleotides),
      m_ScoreMatrixInvalid(true),
      m_prg_callback(0),
      m_terminate(false),
      m_Seq1Vec(seq1.begin(), seq1.end()),
      m_Seq1(&m_Seq1Vec[0]), m_SeqLen1(seq1.size()),
      m_Seq2Vec(seq2.begin(), seq2.end()),
      m_Seq2(&m_Seq2Vec[0]), m_SeqLen2(seq2.size()),
      m_score(kInfMinus),
      m_mt(false),
      m_maxthreads(1),
      m_MaxMem(GetDefaultSpaceLimit())
{
    SetScoreMatrix(scoremat);
    SetSequences(seq1, seq2);
};


void CNWAligner::SetSequences(const char* seq1, size_t len1,
			      const char* seq2, size_t len2,
			      bool verify)
{
    if(!seq1 || !seq2) {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   g_msg_NullParameter);
    }

    if(verify) {
        size_t iErrPos1 = x_CheckSequence(seq1, len1);
	if(iErrPos1 < len1) {
            CNcbiOstrstream oss;
	    oss << "The first sequence is inconsistent with the current "
		<< "scoring matrix type. "
                << "Position = " << iErrPos1 
                << " Symbol = '" << seq1[iErrPos1] << "'";

	    string message = CNcbiOstrstreamToString(oss);
	    NCBI_THROW(CAlgoAlignException, eInvalidCharacter, message);
	}

	size_t iErrPos2 = x_CheckSequence(seq2, len2);
	if(iErrPos2 < len2) {
            CNcbiOstrstream oss;
	    oss << "The second sequence is inconsistent with the current "
		<< "scoring matrix type. "
                << "Position = " << iErrPos2 
                << " Symbol = '" << seq2[iErrPos2] << "'";

	    string message = CNcbiOstrstreamToString(oss);
	    NCBI_THROW(CAlgoAlignException, eInvalidCharacter, message);
	}
    }
    m_Seq1Vec.assign(&seq1[0], &seq1[0]+len1);
    m_Seq2Vec.assign(&seq2[0], &seq2[0]+len2);
    m_Seq1 = &m_Seq1Vec[0];
    m_SeqLen1 = len1;
    m_Seq2 = &m_Seq2Vec[0];
    m_SeqLen2 = len2;
    m_Transcript.clear();
}


void CNWAligner::SetSequences(const string& seq1,
                              const string& seq2,
                              bool verify)
{
    SetSequences(seq1.data(), seq1.size(), seq2.data(), seq2.size(), verify);
}


void CNWAligner::SetEndSpaceFree(bool Left1, bool Right1,
                                 bool Left2, bool Right2)
{
    m_esf_L1 = Left1;
    m_esf_R1 = Right1;
    m_esf_L2 = Left2;
    m_esf_R2 = Right2;
}

void CNWAligner::SetSmithWaterman(bool SW)
{
    m_SmithWaterman = SW;
    if (SW) {
        /// Smith-Waterman necessarily implies that all four ends are free
        m_esf_L1 = m_esf_R1 = m_esf_L2 = m_esf_R2 = true;
    }
}

void CNWAligner::SetGapPreference(EGapPreference p)
{
    m_GapPreference = p;
}

// evaluate score for each possible alignment;
// fill out backtrace matrix
// bit coding (four bits per value): D E Ec Fc
// D:  1 if diagonal; 0 - otherwise
// E:  1 if space in 1st sequence; 0 if space in 2nd sequence
// Ec: 1 if gap in 1st sequence was extended; 0 if it is was opened
// Fc: 1 if gap in 2nd sequence was extended; 0 if it is was opened
//

const unsigned char kMaskFc  = 0x01;
const unsigned char kMaskEc  = 0x02;
const unsigned char kMaskE   = 0x04;
const unsigned char kMaskD   = 0x08;

CNWAligner::TScore CNWAligner::x_Align(SAlignInOut* data)
{


    //check data integrity

    if( m_SmithWaterman && ( data->m_offset1 || m_SeqLen1 != data->m_len1 ||
                             data->m_offset2 || m_SeqLen2 != data->m_len2 ) ) {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   "Smith-Waterman not compatible with offsets provided");
    }

    if( m_SmithWaterman && ( !data->m_esf_L1 || !data->m_esf_R1 ||
                             !data->m_esf_L2 || !data->m_esf_R2 ) ) {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   "Smith-Waterman not compatible with end gap penalties");
    }

    const size_t N1 = data->m_len1 + 1;
    const size_t N2 = data->m_len2 + 1;

    vector<TScore> stl_rowV (N2), stl_rowF(N2);

    const TNCBIScore (* sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    if(m_prg_callback) {
        m_prg_info.m_iter_total = N1*N2;
        m_prg_info.m_iter_done = 0;
        if( (m_terminate = m_prg_callback(&m_prg_info)) ) {
            return 0;
        }
    }

    bool bFreeGapLeft1  = data->m_esf_L1 && data->m_offset1 == 0;
    bool bFreeGapRight1 = data->m_esf_R1 &&
                          m_SeqLen1 == data->m_offset1 + data->m_len1; 

    bool bFreeGapLeft2  = data->m_esf_L2 && data->m_offset2 == 0;
    bool bFreeGapRight2 = data->m_esf_R2 &&
                          m_SeqLen2 == data->m_offset2 + data->m_len2; 

    TScore wgleft1   = bFreeGapLeft1? 0: m_Wg;
    TScore wsleft1   = bFreeGapLeft1? 0: m_Ws;
    TScore wg1 = m_Wg, ws1 = m_Ws;

    // index calculation: [i,j] = i*n2 + j
    CBacktraceMatrix4 backtrace_matrix (N1 * N2);
    backtrace_matrix.SetAt(0, 0);

    // first row
    // note that stl_rowF[0] is not used in the main cycle,
    size_t k;
    stl_rowV[0] = wgleft1;
    for (k = 1; k < N2; ++k) {
        stl_rowV[k] = stl_rowV[k-1] + wsleft1;
        stl_rowF[k] = kInfMinus;
        backtrace_matrix.SetAt(k, kMaskE | kMaskEc);
    }
    backtrace_matrix.Purge(k);
    stl_rowV[0] = 0;
	
    if(m_prg_callback) {
        m_prg_info.m_iter_done = k;
        m_terminate = m_prg_callback(&m_prg_info);
    }

    // gap penalties
    TScore wgleft2 (bFreeGapLeft2? 0: m_Wg);
    TScore wsleft2 (bFreeGapLeft2? 0: m_Ws);

    const char * seq1 = m_Seq1 + data->m_offset1;
    const char * seq1_end = seq1 + data->m_len1;

    TScore V0 = wgleft2;
    TScore V = 0;//best score in the current cell. Will be equal to the NW score at the end
    TScore best_V = 0;//best score in the whole matrix aka score for SW 

    --k;

    for(;  seq1 != seq1_end && !m_terminate;  ++seq1) {

        backtrace_matrix.SetAt(++k, kMaskFc);

        if( seq1 + 1 == seq1_end && bFreeGapRight1) {
                wg1 = ws1 = 0;
        }

        unsigned char tracer;
        const TNCBIScore * row_sc = sm[(size_t)*seq1];

        const char * seq2 = m_Seq2 + data->m_offset2;
        const char * seq2_end = seq2 + data->m_len2;
        TScore wg2 = m_Wg, ws2 = m_Ws;

        //best ending with gap in seq1 open  seq1 X- or extended seq1 X--
        //                                   seq2 XX             seq2 XXX
        TScore  E = kInfMinus;
        //best ending with gap in seq2
        TScore F;
        //total best with 
        //best ending with match    
        TScore G;
        //just temporary
        TScore n0;
        //total best
        TScore * rowV    = &stl_rowV[0];//previos row
        V = V0 += wsleft2;       //current row
        //best ending with match
        TScore * rowF    = &stl_rowF[0];

        for (; seq2 != seq2_end;) {
            
            G = *rowV + row_sc[(size_t)*seq2++];
            *rowV = V;

            n0 = V + wg1;
            if(E >= n0) {
                E += ws1;      // continue the gap
                tracer = kMaskEc;
            }
            else {
                E = n0 + ws1;  // open a new gap
                tracer = 0;
            }

            if( bFreeGapRight2 && seq2 == seq2_end ) {
                wg2 = ws2 = 0;
            }

            F = *++rowF;
            n0 = *++rowV + wg2;
            if(F >= n0) {
                F += ws2;
                tracer |= kMaskFc;
            }
            else {
                F = n0 + ws2;
            }
            *rowF = F;
            
            //best score
            if( G < F || ( G == F && m_GapPreference == eLater) ) {
                if( E <= F ) {
                    V = F;
                } else {
                    V = E;
                    tracer |= kMaskE;
                }
            } else if( E > G || ( E == G && m_GapPreference == eLater) ) {
                V = E;
                tracer |= kMaskE;
            } else {
                V = G;
                tracer |= kMaskD;
            }
            
            if (m_SmithWaterman && V < 0 ) {
                V = 0;
            }

            backtrace_matrix.SetAt(++k, tracer);

            if (V > best_V) {
                best_V = V;
                backtrace_matrix.SetBestPos(k);
            }
        }
        *rowV = V;

        if(m_prg_callback) {
            m_prg_info.m_iter_done = k;
            if( (m_terminate = m_prg_callback(&m_prg_info)) ) {
                break;
            }
        }
    }

    backtrace_matrix.Purge(++k);
    backtrace_matrix.SetBestScore(best_V);

    /*
    //print the matrix out
    {{
    cout<<endl;
    int kk, ind1, ind2, width = 4;
    cout<<setw(width)<<" ";
    cout<<setw(width)<<"-";
    for(ind2 = 0; ind2 < N2-1; ++ind2) {
        cout<<setw(width)<<*(m_Seq2 + data->m_offset2 + ind2);
    }
    cout<<endl;
    for(kk = 0,ind1 = 0; ind1 < N1; ++ind1) {        
        if(ind1) { 
            cout<<setw(width)<<(m_Seq1 + data->m_offset1)[ind1-1];
        } else {
            cout<<setw(width)<<"-";
        }
        for(ind2 = 0; ind2 < N2; ++ind2,++kk) {
            string tstr;
            unsigned char Key (backtrace_matrix[kk]);
            if( Key & kMaskD ) tstr += "D";
            else if ( Key & kMaskE ) tstr += "E";
            else tstr += "F";
            if( Key & kMaskEc )  tstr += "-";            
            if( Key & kMaskFc )  tstr += "|";
            cout<<setw(width)<<tstr;
        }
        cout<<endl<<endl;
    }
    cout<<endl;
    }}
    //end of print the matrix out
    */

    if(!m_terminate) {
        x_SWDoBackTrace(backtrace_matrix, data);
        //check back trace
        TTranscript rv (data->m_transcript.size());
        copy(data->m_transcript.rbegin(), data->m_transcript.rend(), rv.begin());        
        if(m_SmithWaterman) {
            if( best_V != ScoreFromTranscript(rv,  data->m_offset1,  data->m_offset2) ) {
                NCBI_THROW(CAlgoAlignException, eInternal,
                           "CNWAligner: error in back trace");
            }
        } else {
            if( V != ScoreFromTranscript(rv,  data->m_offset1,  data->m_offset2) ) {
                NCBI_THROW(CAlgoAlignException, eInternal,
                           "CNWAligner: error in back trace");
            }
        }
    }

    if(m_SmithWaterman) {
        return best_V;
    }
    return V;
}


CRef<CSeq_align> CNWAligner::Run(CScope &scope, const CSeq_id &id1,
                                 const CSeq_id &id2, bool trim_end_gaps)
{
    CSeq_loc loc1, loc2;
    loc1.SetWhole().Assign(id1);
    loc2.SetWhole().Assign(id2);
    return Run(scope, loc1, loc2, trim_end_gaps);
}

CRef<CSeq_align> CNWAligner::Run(CScope &scope, const CSeq_loc &loc1,
                                 const CSeq_loc &loc2, bool trim_end_gaps)
{
    if ((!loc1.IsInt() && !loc1.IsWhole()) ||
        (!loc1.IsInt() && !loc1.IsWhole()))
    {
        NCBI_THROW(CException, eUnknown,
                   "Only whole and interval locations supported");
    }
    CSeqVector vec1(loc1, scope, CBioseq_Handle::eCoding_Iupac);
    string seq1;
    vec1.GetSeqData(0, vec1.size(), seq1);
    CSeqVector vec2(loc2, scope, CBioseq_Handle::eCoding_Iupac);
    string seq2;
    vec2.GetSeqData(0, vec2.size(), seq2);
    SetSequences(seq1,seq2);
    Run();
    CRef<CSeq_align> align(new CSeq_align);
    align->SetType(CSeq_align::eType_partial);
    align->SetSegs().SetDenseg(*GetDense_seg(
        loc1.GetStart(eExtreme_Biological), loc1.GetStrand(), *loc1.GetId(),
        loc2.GetStart(eExtreme_Biological), loc2.GetStrand(), *loc2.GetId(),
        trim_end_gaps));
    return align;
}

CNWAligner::TScore CNWAligner::Run()
{
    if(m_ScoreMatrixInvalid) {

        NCBI_THROW(CAlgoAlignException, eInvalidMatrix,
                   "CNWAligner::SetScoreMatrix(NULL) must be called "
                   "after changing match/mismatch scores "
                   "to make sure that the new parameters are engaged.");
    }

    if(!m_Seq1 || !m_Seq2) {
        NCBI_THROW(CAlgoAlignException, eNoSeqData,
                   g_msg_DataNotAvailable);
    }

    if(!x_CheckMemoryLimit()) {
        NCBI_THROW(CAlgoAlignException, eMemoryLimit, g_msg_HitSpaceLimit);
    }

    if (m_SmithWaterman && !m_guides.empty()) {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   "Smith-Waterman not compatible with provided pattern");
    }

    m_score = x_Run();

    return m_score;
}


CNWAligner::TScore CNWAligner::x_Run()
{
    try {
    
        m_terminate = false;

        if(m_guides.size() == 0) {

            SAlignInOut data (0, m_SeqLen1, m_esf_L1, m_esf_R1,
                              0, m_SeqLen2, m_esf_L2, m_esf_R2);
            m_score = x_Align(&data);
            m_Transcript = data.m_transcript;
        }
        // run the algorithm for every segment between hits
        else if(m_mt && m_maxthreads > 1) {

            size_t guides_dim = m_guides.size() / 4;

            // setup inputs
            typedef vector<SAlignInOut> TDataVector;
            TDataVector vdata;
            vector<size_t> seed_dims;
            {{
                vdata.reserve(guides_dim + 1);
                seed_dims.reserve(guides_dim + 1);
                size_t q1 = m_SeqLen1, q0, s1 = m_SeqLen2, s0;
                for(size_t istart = 4*guides_dim, i = istart; i != 0; i -= 4) {
                    
                    q0 = m_guides[i - 3] + 1;
                    s0 = m_guides[i - 1] + 1;
                    size_t dim_query = q1 - q0, dim_subj = s1 - s0;
                    
                    bool esf_L1 = false, esf_R1 = false,
                        esf_L2 = false, esf_R2 = false;
                    if(i == istart) {
                        esf_R1 = m_esf_R1;
                        esf_R2 = m_esf_R2;
                    }
                    
                    SAlignInOut data (q0, dim_query, esf_L1, esf_R1,
                                      s0, dim_subj, esf_L2, esf_R2);
                    
                    vdata.push_back(data);
                    seed_dims.push_back(m_guides[i-3] - m_guides[i-4] + 1);

                    q1 = m_guides[i - 4];
                    s1 = m_guides[i - 2];
                }
                SAlignInOut data(0, q1, m_esf_L1, false,
                                 0, s1, m_esf_L2, false);
                vdata.push_back(data);
            }}

            // rearrange vdata so that the largest chunks come first
            typedef vector<SAlignInOut*> TDataPtrVector;
            TDataPtrVector vdata_p (vdata.size());
            {{
                TDataPtrVector::iterator jj = vdata_p.begin();
                NON_CONST_ITERATE(TDataVector, ii, vdata) {
                    *jj++ = &(*ii);
                }
                stable_sort(vdata_p.begin(), vdata_p.end(),
                            SAlignInOut::PSpace);
            }}
            
            // align over the segments
            {{
                m_Transcript.clear();
                size_t idim = vdata.size();

                typedef vector<CNWAlignerThread_Align*> TThreadVector;
                TThreadVector threads;
                threads.reserve(idim);

                ITERATE(TDataPtrVector, ii, vdata_p) {
                    
                    SAlignInOut& data = **ii;

                    if(data.GetSpace() >= 10000000 &&
                       NW_RequestNewThread(m_maxthreads)) {
                        
                        CNWAlignerThread_Align* thread = 
                            new CNWAlignerThread_Align(this, &data);
                        threads.push_back(thread);
                        thread->Run();
                    }
                    else {
                        x_Align(&data);
                    }
                }

                unique_ptr<CException> e;
                ITERATE(TThreadVector, ii, threads) {

                    if(e.get() == 0) {
                        CException* pe = 0;
                        (*ii)->Join(reinterpret_cast<void**>(&pe));
                        if(pe) {
                            e.reset(new CException (*pe));
                        }
                    }
                    else {
                        (*ii)->Join(0);
                    }
                }
                if(e.get()) {
                    throw *e;
                }

                for(size_t idata = 0; idata < idim; ++idata) {

                    SAlignInOut& data = vdata[idata];
                    copy(data.m_transcript.begin(), data.m_transcript.end(),
                         back_inserter(m_Transcript));
                    if(idata + 1 < idim) {
                        for(size_t k = 0; k < seed_dims[idata]; ++k) {
                            m_Transcript.push_back(eTS_Match);
                        }
                    }
                }
                
                m_score = ScoreFromTranscript(GetTranscript(false), 0, 0);
            }}
        }
        else {            

            m_Transcript.clear();
            size_t guides_dim = m_guides.size() / 4;
            size_t q1 = m_SeqLen1, q0, s1 = m_SeqLen2, s0;
            for(size_t istart = 4*guides_dim, i = istart; i != 0; i -= 4) {

                q0 = m_guides[i - 3] + 1;
                s0 = m_guides[i - 1] + 1;
                size_t dim_query = q1 - q0, dim_subj = s1 - s0;
                
                bool esf_L1 = false, esf_R1 = false,
                     esf_L2 = false, esf_R2 = false;
                if(i == istart) {
                    esf_R1 = m_esf_R1;
                    esf_R2 = m_esf_R2;
                }

                SAlignInOut data (q0, dim_query, esf_L1, esf_R1,
                                  s0, dim_subj, esf_L2, esf_R2);

                x_Align(&data);
                copy(data.m_transcript.begin(), data.m_transcript.end(),
                     back_inserter(m_Transcript));

                size_t dim_hit = m_guides[i - 3] - m_guides[i - 4] + 1;
                for(size_t k = 0; k < dim_hit; ++k) {
                    m_Transcript.push_back(eTS_Match);
                }
                q1 = m_guides[i - 4];
                s1 = m_guides[i - 2];
            }
            SAlignInOut data(0, q1, m_esf_L1, false,
                             0, s1, m_esf_L2, false);
            x_Align(&data);
            copy(data.m_transcript.begin(), data.m_transcript.end(),
                 back_inserter(m_Transcript));

            m_score = ScoreFromTranscript(GetTranscript(false), 0, 0);
        }
    }
    
    catch(std::bad_alloc&) {
        NCBI_THROW(CAlgoAlignException, eMemoryLimit, g_msg_OutOfSpace);
    }
    
    return m_score;
}


CNWAligner::ETranscriptSymbol CNWAligner::x_GetDiagTS(size_t i1, size_t i2)
const
{
    const unsigned char c1 = m_Seq1[i1];
    const unsigned char c2 = m_Seq2[i2];
    
    ETranscriptSymbol ts;
    if(m_PositivesAsMatches) {
        ts = m_ScoreMatrix.s[c1][c2] > 0? eTS_Match: eTS_Replace;
    }
    else {
        // N vs N should be mismatch for nucleotides, 
        // and X vs X in protein. Check with matrix
        ts = (toupper(c1) == toupper(c2) && m_ScoreMatrix.s[c1][c2] > 0)? eTS_Match: eTS_Replace;
    }
    return ts;
}

// perform backtrace step, NW only
void CNWAligner::x_DoBackTrace(const CBacktraceMatrix4 & backtrace,
                               SAlignInOut* data)
{
    const size_t N1 (data->m_len1 + 1);
    const size_t N2 (data->m_len2 + 1);

    data->m_transcript.clear();
    data->m_transcript.reserve(N1 + N2);

    size_t k  (N1*N2 - 1);
    size_t i1 (data->m_offset1 + data->m_len1 - 1);
    size_t i2 (data->m_offset2 + data->m_len2 - 1);
    while (k != 0) {

        unsigned char Key (backtrace[k]);

        if (Key & kMaskD) {

            data->m_transcript.push_back(x_GetDiagTS(i1--, i2--));
            k -= N2 + 1;
        }
        else if (Key & kMaskE) {

            data->m_transcript.push_back(eTS_Insert);
            --k;
            --i2;
            while(k > 0 && (Key & kMaskEc)) {

                data->m_transcript.push_back(eTS_Insert);
                Key = backtrace[k--];
                --i2;
            }
        }
        else {

            data->m_transcript.push_back(eTS_Delete);
            k -= N2;
            --i1;
            while(k > 0 && (Key & kMaskFc)) {

                data->m_transcript.push_back(eTS_Delete);
                Key = backtrace[k];
                k -= N2;
                --i1;
            }
        }
    }
}



// perform backtrace step, NW + SW
void CNWAligner::x_SWDoBackTrace(const CBacktraceMatrix4 & backtrace,
                               SAlignInOut* data)
{
    const size_t N1 (data->m_len1 + 1);
    const size_t N2 (data->m_len2 + 1);
    const TNCBIScore (* sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    data->m_transcript.clear();
    data->m_transcript.reserve(N1 + N2);

    size_t k  (N1*N2 - 1);
    size_t i1 (data->m_offset1 + data->m_len1 - 1);
    size_t i2 (data->m_offset2 + data->m_len2 - 1);
    if (m_SmithWaterman) {
        size_t sw_k =  backtrace.BestPos();
        data->FillEdgeGaps(k - sw_k, true);
        i1 -= (k - sw_k) / (data->m_len2+1);
        i2 -= (k - sw_k) % (data->m_len2+1);
        k = backtrace.BestPos();
    }

    //score for SmithWaterman. Stop when score == 0
    TNCBIScore score = backtrace.BestScore(); 

    while (k > 0 && ( !m_SmithWaterman || score > 0 ) ) {

        unsigned char Key (backtrace[k]);

        if (Key & kMaskD) {
            score -= sm[(size_t)(m_Seq1[i1])][(size_t)(m_Seq2[i2])];
            data->m_transcript.push_back(x_GetDiagTS(i1--, i2--));
            k -= N2 + 1;
        }
        else if (Key & kMaskE) {
            score -= m_Wg + m_Ws;
            data->m_transcript.push_back(eTS_Insert);
            --k;
            --i2;
           while(k > 0 && (Key & kMaskEc)) {
               score -= m_Ws;
                data->m_transcript.push_back(eTS_Insert);
                Key = backtrace[k--];
                --i2;
            }
         }
        else {
            score -= m_Wg + m_Ws;
            data->m_transcript.push_back(eTS_Delete);
            k -= N2;
            --i1;
            while(k > 0 && (Key & kMaskFc)) {
               score -= m_Ws;
                data->m_transcript.push_back(eTS_Delete);
                Key = backtrace[k];
                k -= N2;
                --i1;
            }
        }
    }
    if( m_SmithWaterman && score != 0 ) {
        NCBI_THROW(CAlgoAlignException, eInternal,
                   "negative score in Smith-Waterman back trace");
    }
    data->FillEdgeGaps(k, false);
}


void  CNWAligner::SetPattern(const vector<size_t>& guides)
{
    size_t dim = guides.size();
    const char* err = 0;
    if(dim % 4 == 0) {
        for(size_t i = 0; i < dim; i += 4) {

            if( guides[i] > guides[i+1] || guides[i+2] > guides[i+3] ) {
                err = "Pattern hits must be specified in plus strand";
		break;
            }

            if(i > 4) {
                if(guides[i] <= guides[i-3] || guides[i+2] <= guides[i-2]){
                    err = "Pattern hits coordinates must be sorted";
                    break;
                }
            }

            size_t dim1 = guides[i + 1] - guides[i];
            size_t dim2 = guides[i + 3] - guides[i + 2];
            if( dim1 != dim2) {
                err = "Pattern hits must have equal length on both sequences";
                break;
            }

            if(guides[i+1] >= m_SeqLen1 || guides[i+3] >= m_SeqLen2) {
                err = "One or several pattern hits are out of range";
                break;
            }
        }
    }
    else {
        err = "Pattern must have a dimension multiple of four";
    }

    if(err) {
        NCBI_THROW(CAlgoAlignException, eBadParameter, err);
    }
    else {
        m_guides = guides;
    }
}


void CNWAligner::GetEndSpaceFree(bool* L1, bool* R1, bool* L2, bool* R2) const
{
    if(L1) *L1 = m_esf_L1;
    if(R1) *R1 = m_esf_R1;
    if(L2) *L2 = m_esf_L2;
    if(R2) *R2 = m_esf_R2;
}

bool CNWAligner::IsSmithWaterman() const
{
    return m_SmithWaterman;
}

CNWAligner::EGapPreference CNWAligner::GetGapPreference() const
{
    return m_GapPreference;
}

// return raw transcript
CNWAligner::TTranscript CNWAligner::GetTranscript(bool reversed) const
{
    TTranscript rv (m_Transcript.size());
    if(reversed) {
        copy(m_Transcript.begin(), m_Transcript.end(), rv.begin());
    }
    else {
        copy(m_Transcript.rbegin(), m_Transcript.rend(), rv.begin());
    }
    return rv;
}


// alignment 'restore' - set raw transcript
void CNWAligner::SetTranscript(const TTranscript& transcript)
{
    m_Transcript = transcript;
    m_score = ScoreFromTranscript(transcript);
}


// Return transcript as a readable string
string CNWAligner::GetTranscriptString(void) const
{
    const Int8 dim (m_Transcript.size());
    string s;
    s.resize(dim);
    size_t i1 (0), i2 (0), i (0);

    for (Int8 k (dim - 1); k >= 0; --k) {

        ETranscriptSymbol c0 (m_Transcript[k]);
        char c (0);
        switch ( c0 ) {

            case eTS_Match: {

                if(m_Seq1 && m_Seq2) {
                    ETranscriptSymbol ts = x_GetDiagTS(i1++, i2++);
                    c = ts == eTS_Match? 'M': 'R';
                }
                else {
                    c = 'M';
                }
            }
            break;

            case eTS_Replace: {

                if(m_Seq1 && m_Seq2) {
                    ETranscriptSymbol ts = x_GetDiagTS(i1++, i2++);
                    c = ts == eTS_Match? 'M': 'R';
                }
                else {
                    c = 'R';
                }
            }
            break;

            case eTS_Insert: {
                c = 'I';
                ++i2;
            }
            break;

            case eTS_SlackInsert: {
                c = 'i';
                ++i2;
            }
            break;

            case eTS_SlackDelete: {
                c = 'd';
                ++i1;
            }
            break;

            case eTS_Delete: {
                c = 'D';
                ++i1;
            }
            break;

            case eTS_Intron: {
                c = '+';
                ++i2;
            }
            break;

	    default: {
	      NCBI_THROW(CAlgoAlignException, eInternal,
                         g_msg_InvalidTranscriptSymbol);
	    }	  
        }

        s[i++] = c;
    }

    if(i < s.size()) {
        s.resize(i + 1);
    }

    return s;
}


void CNWAligner::SetProgressCallback(FProgressCallback prg_callback, void* data)
{
    m_prg_callback = prg_callback;
    m_prg_info.m_data = data;
}


void CNWAligner::SetWms(TScore val)
{
    m_Wms = val;
    m_ScoreMatrixInvalid = true;
}


void CNWAligner::SetWm(TScore val)
{
    m_Wm = val;
    m_ScoreMatrixInvalid = true;
}

 
void CNWAligner::SetScoreMatrix(const SNCBIPackedScoreMatrix* psm)
{
    if(psm) {

        m_abc = psm->symbols;
	NCBISM_Unpack(psm, &m_ScoreMatrix);
    }
    else { // assume IUPACna

        m_abc = g_nwaligner_nucleotides;
        const size_t dim = strlen(m_abc);
        vector<TNCBIScore> iupacna (dim*dim, m_Wms);
        iupacna[0] = iupacna[dim+1] = iupacna[2*(dim+1)] = 
            iupacna[3*(dim+1)] = m_Wm;
        SNCBIPackedScoreMatrix iupacna_psm;
        iupacna_psm.symbols = g_nwaligner_nucleotides;
        iupacna_psm.scores = &iupacna.front();
        iupacna_psm.defscore = m_Wms;
        NCBISM_Unpack(&iupacna_psm, &m_ScoreMatrix);        
    }
    m_ScoreMatrixInvalid = false;
}


// Check that all characters in sequence are valid for 
// the current sequence type.
// Return an index to the first invalid character
// or len if all characters are valid.
size_t CNWAligner::x_CheckSequence(const char* seq, size_t len) const
{
    char Flags [256];
    memset(Flags, 0, sizeof Flags);
    const size_t abc_size = strlen(m_abc);
    
    size_t k;
    for(k = 0; k < abc_size; ++k) {
        Flags[unsigned(toupper((unsigned char) m_abc[k]))] = 1;
        Flags[unsigned(tolower((unsigned char) m_abc[k]))] = 1;
        Flags[unsigned(k)] = 1;
    }

    for(k = 0; k < len; ++k) {
        if(Flags[unsigned(seq[k])] == 0)
            break;
    }

    return k;
}


CNWAligner::TScore CNWAligner::GetScore() const 
{
  if(m_Transcript.size()) {
      return m_score;
  }
  else {
    NCBI_THROW(CAlgoAlignException, eNoSeqData,
               g_msg_NoAlignment);
  }
}


bool CNWAligner::x_CheckMemoryLimit()
{
    const size_t elem_size (GetElemSize());
    const size_t gdim (m_guides.size());
    double mem (0);

    if(gdim) {

        size_t dim1 (m_guides[0]), dim2 (m_guides[2]);
        mem = double(dim1) * dim2 * elem_size;
        if(mem <= m_MaxMem) {

            for(size_t i (4); i < gdim; i += 4) {

                dim1 = m_guides[i] - m_guides[i-3] + 1;
                dim2 = m_guides[i + 2] - m_guides[i-1] + 1;
                mem = double(dim1) * dim2 * elem_size;
                if(mem > m_MaxMem) {
                    break;
                }
            }

            if(mem <= m_MaxMem) {
                dim1 = m_SeqLen1 - m_guides[gdim-3];
                dim2 = m_SeqLen2 - m_guides[gdim-1];
                mem = double(dim1) * dim2 * elem_size;
            }
        }
    }
    else {
        mem = double(m_SeqLen1 + 1) * (m_SeqLen2 + 1) * elem_size;
    }

    return mem < m_MaxMem;
}


void CNWAligner::EnableMultipleThreads(bool enable)
{
    m_maxthreads = (m_mt = enable)? CSystemInfo::GetCpuCount(): 1;
}


CNWAligner::TScore CNWAligner::ScoreFromTranscript(
                       const TTranscript& transcript,
                       size_t start1, size_t start2) const
{
    bool nucl_mode;
    if(start1 == kMax_UInt && start2 == kMax_UInt) {
        nucl_mode = true;
    }
    else if(start1 != kMax_UInt && start2 != kMax_UInt) {
        nucl_mode = false;
    }
    else {
        NCBI_THROW(CAlgoAlignException, eInternal,
                   g_msg_InconsistentArguments);
    }

    size_t dim (transcript.size());
    if(dim == 0) {
        return 0;
    }

    TScore score (0);

    const char* p1 (m_Seq1 + start1);
    const char* p2 (m_Seq2 + start2);

    int state1(0);   // 0 = normal, 1 = gap;
    int state2(0);   // 0 = normal, 1 = gap;

    size_t i (0);

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    ///SmithWaterman alterations
    if( IsSmithWaterman() ) {
        //cut beginning gaps
        for(; i<dim; ++i) {
            if( transcript[i] == eTS_Insert ) {
                ++p2;
            } else if( transcript[i] == eTS_Delete ) {
                ++p1;
            } else if( transcript[i] == eTS_Match ||
                       transcript[i] == eTS_Replace ) {
                break;
            } else {
                NCBI_THROW(CAlgoAlignException, eInternal, g_msg_InvalidTranscriptSymbol);
            }
        }
        if( i == dim ) {//alignment consists of gaps only
            return score;
        }
        //cut trailing gaps
      //for(size_t endi = dim-1; endi >=0; --endi) {   // never stops CXX-11373
        for(size_t endi = dim; endi-- > 0; ) {
              if( transcript[endi] == eTS_Match || transcript[endi] == eTS_Replace ) {
                dim = endi + 1;
                break;
            } else if( transcript[endi] != eTS_Insert  && transcript[endi] != eTS_Delete ) {
                NCBI_THROW(CAlgoAlignException, eInternal, g_msg_InvalidTranscriptSymbol);
            }
        }
    }

    for(; i < dim; ++i) {

        ETranscriptSymbol ts (transcript[i]);

        switch(ts) {

        case eTS_Match:
        case eTS_Replace: {
            if(nucl_mode) {
                score += (ts == eTS_Match)? m_Wm: m_Wms;
            }
            else {
                unsigned char c1 = *p1;
                unsigned char c2 = *p2;
                score += sm[c1][c2];
                ++p1; ++p2;
            }
            state1 = state2 = 0;
        }
        break;

        case eTS_Insert: {

            if(state1 != 1) score += m_Wg;
            state1 = 1; state2 = 0;
            score += m_Ws;
            ++p2;
        }
        break;

        case eTS_Delete: {

            if(state2 != 1) score += m_Wg;
            state1 = 0; state2 = 1;
            score += m_Ws;
            ++p1;
        }
        break;
        
        default:
            NCBI_THROW(CAlgoAlignException, eInternal, g_msg_InvalidTranscriptSymbol);
        }
    }

    if( IsSmithWaterman() ) {//end gap scores are already excluded
        return score;
    }

    if(m_esf_L1) {
        size_t g = 0;
        for(size_t i = 0; i < dim; ++i) {
            if(transcript[i] == eTS_Insert) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_L2) {
        size_t g = 0;
        for(size_t i = 0; i < dim; ++i) {
            if(transcript[i] == eTS_Delete) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_R1) {
        size_t g = 0;
        for(Int8 i = dim - 1; i >= 0; --i) {
            if(transcript[i] == eTS_Insert) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_R2) {
        size_t g = 0;
        for(Int8 i = dim - 1; i >= 0; --i) {
            if(transcript[i] == eTS_Delete) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    return score;
}


size_t CNWAligner::GetLeftSeg(size_t* q0, size_t* q1,
                              size_t* s0, size_t* s1,
                              size_t min_size) const
{
    size_t trdim = m_Transcript.size();
    size_t cur = 0, maxseg = 0;
    const char* p1 = m_Seq1;
    const char* p2 = m_Seq2;
    size_t i0 = 0, j0 = 0, imax = i0, jmax = j0;

    for(Int8 k = trdim - 1; k >= 0; --k) {

        switch(m_Transcript[k]) {

            case eTS_Insert: {
                ++p2;
                if(cur > maxseg) {
                    maxseg = cur;
                    imax = i0;
                    jmax = j0;
                    if(maxseg >= min_size) goto ret_point;
                }
                cur = 0;
            }
            break;

            case eTS_Delete: {
            ++p1;
            if(cur > maxseg) {
                maxseg = cur;
                imax = i0;
                jmax = j0;
                if(maxseg >= min_size) goto ret_point;
            }
            cur = 0;
            }
            break;

            case eTS_Match:
            case eTS_Replace: {
                if(*p1 == *p2) {
                    if(cur == 0) {
                        i0 = p1 - m_Seq1;
                        j0 = p2 - m_Seq2;
                    }
                    ++cur;
                }
                else {
                    if(cur > maxseg) {
                        maxseg = cur;
                        imax = i0;
                        jmax = j0;
                        if(maxseg >= min_size) goto ret_point;
                    }
                    cur = 0;
                }
                ++p1;
                ++p2;
            }
            break;

            default: {
                NCBI_THROW( CAlgoAlignException, eInternal,
                            g_msg_InvalidTranscriptSymbol );
            }
        }
    }

    if(cur > maxseg) {
      maxseg = cur;
      imax = i0;
      jmax = j0;
    }

 ret_point:

    *q0 = imax; *s0 = jmax;
    *q1 = *q0 + maxseg - 1;
    *s1 = *s0 + maxseg - 1;

    return maxseg;
}

/////////////////////////////////////////////////
/// naive pattern generator (a la Rabin-Karp)

struct nwaln_mrnaseg {
    nwaln_mrnaseg(size_t i1, size_t i2, unsigned char fp0):
        a(i1), b(i2), fp(fp0) {}
    size_t a, b;
    unsigned char fp;
};

struct nwaln_mrnaguide {
    nwaln_mrnaguide(size_t i1, size_t i2, size_t i3, size_t i4):
        q0(i1), q1(i2), s0(i3), s1(i4) {}
    size_t q0, q1, s0, s1;
};


unsigned char CNWAligner::x_CalcFingerPrint64(
    const char* beg, const char* end, size_t& err_index)
{
    if(beg >= end) {
        return 0xFF;
    }

    unsigned char fp = 0, code;
    for(const char* p = beg; p < end; ++p) {
        switch(*p) {
        case 'A': code = 0;    break;
        case 'G': code = 0x01; break;
        case 'T': code = 0x02; break;
        case 'C': code = 0x03; break;
        default:  err_index = p - beg; return 0x40; // incorrect char
        }
        fp = 0x3F & ((fp << 2) | code);
    }

    return fp;
}


const char* CNWAligner::x_FindFingerPrint64(
    const char* beg, const char* end,
    unsigned char fingerprint, size_t size,
    size_t& err_index)
{

    if(beg + size > end) {
        err_index = 0;
        return 0;
    }

    const char* p0 = beg;

    size_t err_idx = 0; --p0;
    unsigned char fp = 0x40;
    while(fp == 0x40 && p0 < end) {
        p0 += err_idx + 1;
        fp = x_CalcFingerPrint64(p0, p0 + size, err_idx);
    }

    if(p0 >= end) {
        return end;  // not found
    }
    
    unsigned char code;
    while(fp != fingerprint && ++p0 < end) {

        switch(*(p0 + size - 1)) {
        case 'A': code = 0;    break;
        case 'G': code = 0x01; break;
        case 'T': code = 0x02; break;
        case 'C': code = 0x03; break;
        default:  err_index = p0 + size - 1 - beg;
                  return 0;
        }
        
        fp = 0x3F & ((fp << 2) | code );
    }

    return p0;
}


size_t CNWAligner::MakePattern(const size_t guide_size,
                               const size_t guide_core)
{
    if(guide_core > guide_size) {
        NCBI_THROW(CAlgoAlignException,
                   eBadParameter,
                   g_msg_NullParameter);
    }

    vector<nwaln_mrnaseg> segs;

    size_t err_idx(0);
    for(size_t i = 0; i + guide_size <= m_SeqLen1; ) {
        const char* beg = m_Seq1 + i;
        const char* end = m_Seq1 + i + guide_size;
        unsigned char fp = x_CalcFingerPrint64(beg, end, err_idx);
        if(fp != 0x40) {
            segs.push_back(nwaln_mrnaseg(i, i + guide_size - 1, fp));
            i += guide_size;
        }
        else {
            i += err_idx + 1;
        }
    }

    vector<nwaln_mrnaguide> guides;
    size_t idx = 0;
    const char* beg = m_Seq2 + idx;
    const char* end = m_Seq2 + m_SeqLen2;
    for(size_t i = 0, seg_count = segs.size();
        beg + guide_size <= end && i < seg_count; ++i) {

        const char* p = 0;
        const char* beg0 = beg;
        while( p == 0 && beg + guide_size <= end ) {

            p = x_FindFingerPrint64( beg, end, segs[i].fp,
                                     guide_size, err_idx );
            if(p == 0) { // incorrect char
                beg += err_idx + 1; 
            }
            else if (p < end) {// fingerprints match but check actual sequences
                const char* seq1 = m_Seq1 + segs[i].a;
                const char* seq2 = p;
                size_t k;
                for(k = 0; k < guide_size; ++k) {
                    if(seq1[k] != seq2[k]) break;
                }
                if(k == guide_size) { // real match
                    size_t i1 = segs[i].a;
                    size_t i2 = segs[i].b;
                    size_t i3 = seq2 - m_Seq2;
                    size_t i4 = i3 + guide_size - 1;
                    size_t guides_dim = guides.size();
                    if( guides_dim == 0 ||
                        i1 - 1 > guides[guides_dim - 1].q1 ||
                        i3 - 1 > guides[guides_dim - 1].s1    ) {
                        guides.push_back(nwaln_mrnaguide(i1, i2, i3, i4));
                    }
                    else { // expand the last guide
                        guides[guides_dim - 1].q1 = i2;
                        guides[guides_dim - 1].s1 = i4;
                    }
                    beg0 = p + guide_size;
                }
                else {  // spurious match
                    beg = p + 1;
                    p = 0;
                }
            }
        }
        beg = beg0; // restore start pos in genomic sequence
    }

    // initialize m_guides
    size_t guides_dim = guides.size();
    m_guides.clear();
    m_guides.resize(4*guides_dim);
    const size_t offs = guide_core/2 - 1;
    for(size_t k = 0; k < guides_dim; ++k) {
        size_t q0 = (guides[k].q0 + guides[k].q1) / 2;
        size_t s0 = (guides[k].s0 + guides[k].s1) / 2;
        m_guides[4*k]         = q0 - offs;
        m_guides[4*k + 1]     = q0 + offs;
        m_guides[4*k + 2]     = s0 - offs;
        m_guides[4*k + 3]     = s0 + offs;
    }
 
    return m_guides.size();   
}

//////////////////////////////////////////////
/////////////////////////////////////////////

size_t CNWAligner::GetRightSeg(size_t* q0, size_t* q1,
                               size_t* s0, size_t* s1,
                               size_t min_size) const
{
    size_t trdim = m_Transcript.size();
    size_t cur = 0, maxseg = 0;
    const char* seq1_end = m_Seq1 + m_SeqLen1;
    const char* seq2_end = m_Seq2 + m_SeqLen2;
    const char* p1 = seq1_end - 1;
    const char* p2 = seq2_end - 1;
    size_t i0 = m_SeqLen1 - 1, j0 = m_SeqLen2 - 1,
           imax = i0, jmax = j0;

    for(size_t k = 0; k < trdim; ++k) {

        switch(m_Transcript[k]) {

            case eTS_Insert: {
                --p2;
                if(cur > maxseg) {
                    maxseg = cur;
                    imax = i0;
                    jmax = j0;
                    if(maxseg >= min_size) goto ret_point;
                }
                cur = 0;
            }
            break;

            case eTS_Delete: {
                --p1;
                if(cur > maxseg) {
                    maxseg = cur;
                    imax = i0;
                    jmax = j0;
                    if(maxseg >= min_size) goto ret_point;
		}
                cur = 0;
            }
            break;

            case eTS_Match:
            case eTS_Replace: {
                if(*p1 == *p2) {
                    if(cur == 0) {
                        i0 = p1 - m_Seq1;
                        j0 = p2 - m_Seq2;
                    }
                    ++cur;
                }
                else {
                    if(cur > maxseg) {
                        maxseg = cur;
                        imax = i0;
                        jmax = j0;
                        if(maxseg >= min_size) goto ret_point;
                    }
                    cur = 0;
                }
                --p1;
                --p2;
            }
            break;

            default: {
                NCBI_THROW( CAlgoAlignException, eInternal,
                            g_msg_InvalidTranscriptSymbol );
            }
        }
    }

    if(cur > maxseg) {
      maxseg = cur;
      imax = i0;
      jmax = j0;
    }

 ret_point:

    *q1 = imax; *s1 = jmax;
    *q0 = imax - maxseg + 1;
    *s0 = jmax - maxseg + 1;

    return maxseg;
}


size_t CNWAligner::GetLongestSeg(size_t* q0, size_t* q1,
                                 size_t* s0, size_t* s1) const
{
    size_t trdim = m_Transcript.size();
    size_t cur = 0, maxseg = 0;
    const char* p1 = m_Seq1;
    const char* p2 = m_Seq2;
    size_t i0 = 0, j0 = 0, imax = i0, jmax = j0;

    for(Int8 k = trdim-1; k >= 0; --k) {

        switch(m_Transcript[k]) {

            case eTS_Insert: {
                ++p2;
                if(cur > maxseg) {
                    maxseg = cur;
                    imax = i0;
                    jmax = j0;
                }
                cur = 0;
            }
            break;

            case eTS_Delete: {
            ++p1;
            if(cur > maxseg) {
                maxseg = cur;
                imax = i0;
                jmax = j0;
            }
            cur = 0;
            }
            break;

            case eTS_Match:
            case eTS_Replace: {
                if(*p1 == *p2) {
                    if(cur == 0) {
                        i0 = p1 - m_Seq1;
                        j0 = p2 - m_Seq2;
                    }
                    ++cur;
                }
                else {
		    if(cur > maxseg) {
                        maxseg = cur;
                        imax = i0;
                        jmax = j0;
                    }
                    cur = 0;
                }
                ++p1;
                ++p2;
            }
            break;

            default: {
                NCBI_THROW( CAlgoAlignException, eInternal,
                            g_msg_InvalidTranscriptSymbol );
            }
        }
    }
    
    if(cur > maxseg) {
        maxseg = cur;
        imax = i0;
        jmax = j0;
    }
    
    *q0 = imax; *s0 = jmax;
    *q1 = *q0 + maxseg - 1;
    *s1 = *s0 + maxseg - 1;

    return maxseg;
}


CRef<CDense_seg> CNWAligner::GetDense_seg(TSeqPos query_start,
                                          ENa_strand query_strand,
                                          TSeqPos subj_start,
                                          ENa_strand subj_strand,
                                          bool trim_end_gaps) const
{
    CNWFormatter::ESeqAlignFormatFlags  flags;
    if(trim_end_gaps) {
        flags = CNWFormatter::eSAFF_TrimEndGaps;
    } else {
        flags = CNWFormatter::eSAFF_None;
    }

    CNWFormatter fmt(*this);

    return fmt.AsDenseSeg(query_start, query_strand,
                          subj_start, subj_strand, flags);
}


CRef<CDense_seg> CNWAligner::GetDense_seg(TSeqPos query_start,
                                          ENa_strand query_strand,
                                          const CSeq_id& query_id,
                                          TSeqPos subj_start,
                                          ENa_strand subj_strand,
                                          const CSeq_id& subj_id,
                                          bool trim_end_gaps) const
{
    CNWFormatter::ESeqAlignFormatFlags  flags;
    if(trim_end_gaps) {
        flags = CNWFormatter::eSAFF_TrimEndGaps;
    } else {
        flags = CNWFormatter::eSAFF_None;
    }

    CNWFormatter fmt(*this);

    CConstRef<CSeq_id> id0(&query_id);
    CConstRef<CSeq_id> id1(&subj_id);
    fmt.SetSeqIds(id0, id1);

    return fmt.AsDenseSeg(query_start, query_strand,
                          subj_start, subj_strand, flags);
}

END_NCBI_SCOPE
