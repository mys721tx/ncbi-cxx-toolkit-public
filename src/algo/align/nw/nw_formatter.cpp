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
 * Author:  Yuri Kapustin, Boris Kiryutin
 *
 * ===========================================================================
 *
 */

#include <ncbi_pch.hpp>
#include "messages.hpp"
#include <algo/align/nw/nw_formatter.hpp>
#include <algo/align/nw/align_exception.hpp>

#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <iterator>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CNWFormatter::CNWFormatter (const CNWAligner& aligner):
    m_aligner(&aligner)
{
    const char id_not_set[] = "ID_not_set";
    CRef<CSeq_id> seqid (new CSeq_id);
    seqid->SetLocal().SetStr(id_not_set);
    m_Seq1Id = m_Seq2Id = seqid;
}


void CNWFormatter::SetSeqIds(CConstRef<CSeq_id> id1, CConstRef<CSeq_id> id2)
{
    m_Seq1Id = id1;
    m_Seq2Id = id2;
}

CRef<CSeq_align> CNWFormatter::AsSeqAlign(
    TSeqPos query_start, ENa_strand query_strand,
    TSeqPos subj_start,  ENa_strand subj_strand,
    int flags) const
{

    bool trim_end_gaps = ( flags & eSAFF_TrimEndGaps ) || m_aligner->IsSmithWaterman();

    CRef<CSeq_align> seqalign (new CSeq_align);

    // the alignment is pairwise
    seqalign->SetDim(2);

    // NW is a global alignment; SW is local
    if(m_aligner->IsSmithWaterman()) {
        seqalign->SetType(CSeq_align::eType_partial);
    }
    else {
        seqalign->SetType(CSeq_align::eType_global);
    }

    // add dynprog score
    if(flags & eSAFF_DynProgScore) {
        CRef<CScore> score (new CScore);
        score->SetId().SetStr("global_score");
        score->SetValue().SetInt(m_aligner->GetScore());
        seqalign->SetScore().push_back(score);
    }

    // add identity score
    if(flags & eSAFF_Identity) {
        const string transcript = m_aligner->GetTranscriptString();
        TSeqPos matches = 0;
        ITERATE(string, ii, transcript) { 
            if(*ii == CNWAligner::eTS_Match) {
                ++matches;
            }
        }

        size_t length=0; 
        if(trim_end_gaps) {

            Int8 endi = transcript.size() - 1;
            while( endi>=0 && ( transcript[endi] != CNWAligner::eTS_Match &&  transcript[endi] != CNWAligner::eTS_Replace ) ) --endi;

            Int8 ind = 0;
            for( ;ind < endi; ++ind) {
                if(transcript[ind] == CNWAligner::eTS_Match ||  transcript[ind] == CNWAligner::eTS_Replace ) break;
        }

            if( ind <= endi ) {
                length = endi + 1 - ind;
            }

        } else {            
            length = transcript.size();
        }
            



        double idty = 0;
        if( length > 0 ) {
            idty = double(matches) / length;
        }            
        CRef<CScore> score (new CScore);
        score->SetId().SetStr("identity");
        score->SetValue().SetReal(idty);
        seqalign->SetScore().push_back(score);
    }

    CRef<CDense_seg> rds= AsDenseSeg(query_start, query_strand,
                                  subj_start, subj_strand, flags);

    if(rds) {
        seqalign->SetSegs().SetDenseg(*rds);
    } else {
        seqalign->SetSegs().SetDendiag();
    }

    return seqalign;
}

CRef<CDense_seg> CNWFormatter::AsDenseSeg(
    TSeqPos query_start, ENa_strand query_strand,
    TSeqPos subj_start,  ENa_strand subj_strand,
    int flags) const
{

    bool trim_end_gaps = ( flags & eSAFF_TrimEndGaps ) || m_aligner->IsSmithWaterman();

    const string transcript = m_aligner->GetTranscriptString();

    if(transcript.size() == 0) {
        NCBI_THROW(CAlgoAlignException, eNoSeqData, g_msg_NoAlignment);
    }

    CRef<CDense_seg> rds(new CDense_seg);
    CDense_seg& ds = *rds;

    if( m_aligner->IsSmithWaterman() ) {// check if alignment is empty

        const CNWAligner::TTranscript stranscript = m_aligner->GetTranscript();

        vector<CNWAligner::ETranscriptSymbol>::const_iterator
            ib = stranscript.begin(),
            ie = stranscript.end();
        while( ( ie != ib ) &&
               ( *ib == CNWAligner::eTS_Insert || *ib == CNWAligner::eTS_Delete || *ib == CNWAligner::eTS_Intron ||
                 *ib == CNWAligner::eTS_SlackInsert || *ib == CNWAligner::eTS_SlackDelete ) ) {
            ++ib;
        }
        if( ib == ie ) {
            rds.Reset();
            return rds;
        }
    }
        

    ds.FromTranscript(query_start, query_strand,
                      subj_start,  subj_strand,
                      transcript);
    
    CDense_seg::TIds& ids = ds.SetIds();
    if( m_Seq1Id && m_Seq2Id ) {
        
        CRef<CSeq_id> id_query (new CSeq_id);
        id_query->Assign(*m_Seq1Id);
        ids.push_back(id_query);
        
        CRef<CSeq_id> id_subj (new CSeq_id);
        id_subj->Assign(*m_Seq2Id);
        ids.push_back(id_subj);
    }

    if(trim_end_gaps) {
        ds.TrimEndGaps();
    }

    return rds;
}

static const char s_kGap [] = "<GAP>";

void CNWFormatter::SSegment::SetToGap()
{
    m_exon = false;
    m_idty = 0;
    m_len = m_box[1] - m_box[0] + 1;
    m_annot = s_kGap;
    m_details.resize(0);
    m_score = 0;   // no score for <Gap>s 
}

// try improving the segment by cutting it from the left
void CNWFormatter::SSegment::ImproveFromLeft1(const char* seq1, const char* seq2,
                                        CConstRef<CSplicedAligner> aligner)
{
    
    //legacy check
    const size_t min_query_size = 4;
    if( int(m_box[1] - m_box[0] + 1) < int(min_query_size)) {
        SetToGap();
        return;
    }

    //compute length and number of matches
    int len_total = (int)m_details.size();
    int match_total = 0;
    string::iterator irs0 = m_details.begin(),
        irs1 = m_details.end(), irs;

    for(irs = irs0; irs != irs1; ++irs) {
        if(*irs == 'M') {
            ++match_total;
        }
    }

    //count identity at the right end
    string::reverse_iterator rirs0 = m_details.rbegin(),
        rirs1 = m_details.rend(), rirs = rirs0;
    int cnt = 0, max_cnt = 20;
    int len = 0, match = 0;

    for( ; ( rirs != rirs1 ) && (cnt != max_cnt) ; ++rirs, ++cnt) {
        ++len;
        if(*rirs == 'M') {
            ++match;
        }
    }
    double ident = match/(double)len;
         
    //trimming point
    int i0_max = 0, i1_max = 0;
    string::iterator irs_max;

    //find the trimming point
    int i0 = 0, i1 = 0;
    len = 0;
    match = 0;
    double epsilon = 1e-10;
    const double dropoff_diff = .19; 

    --irs1;
    for(irs = irs0; irs != irs1; ++irs) {
        
        switch(*irs) {
            
        case 'M': {
            ++match;
            ++i0;
            ++i1;
        }
        break;
            
        case 'R': {
            ++i0;
            ++i1;
        }
        break;
      
        case 'I': {
            ++i1;
        }
        break;

        case 'D': {
            ++i0;
        }
        }
        ++len;

        //trim here if        
        if( max( ident, (match_total - match)/(double)(len_total-len) ) - match/(double)len - dropoff_diff> epsilon ){
            i0_max = i0;
            i1_max = i1;
            irs_max = irs;
            //do not count trimmed part, adjust values
            match_total -= match;
            len_total -= len;
            match = 0;
            len = 0;
        }
    }

    // work around a weird case of equally optimal
    // but detrimental for our purposes alignment
    // -check the actual sequence chars
    size_t head = 0;
    while(i0_max > 0 && i1_max > 0) {
        if( toupper(seq1[m_box[0]+i0_max-1]) != 'N' && seq1[m_box[0]+i0_max-1] == seq2[m_box[2]+i1_max-1] ) {
            --i0_max; --i1_max;
            ++head;
        }
        else {
            break;
        }
    }

    //trim

    if(i0_max == 0 && i1_max == 0) return;//no changes
    
    // if the resulting segment is still long enough
    if(m_box[1] - m_box[0] + 1 - i0_max >= min_query_size )
    {
        // resize
        m_box[0] += i0_max;
        m_box[2] += i1_max;
        const size_t L = irs_max - irs0 + 1;
        m_details.erase(0, L);
        m_details.insert(m_details.begin(), head, 'M');
        Update(aligner.GetNonNullPointer());
        
        // update the first two annotation symbols
        if(m_annot.size() > 2 && m_annot[2] == '<') {
            int  j1 = int(m_box[2]) - 2;
            char c1 = j1 >= 0? seq2[j1]: ' ';
            m_annot[0] = c1;
            int  j2 = int(m_box[2]) - 1;
            char c2 = j2 >= 0? seq2[j2]: ' ';
            m_annot[1] = c2;
        }
    } else {
        SetToGap();//just drop it
    }
}


// try improving the segment by cutting it from the left
void CNWFormatter::SSegment::ImproveFromLeft(const char* seq1, const char* seq2,
                                        CConstRef<CSplicedAligner> aligner)
{
    const size_t min_query_size = 4;
    
    int i0 = int(m_box[1] - m_box[0] + 1), i0_max = i0;
    if(i0 < int(min_query_size)) {
        SetToGap();
        return;
    }
    
    // find the top score suffix
    int i1 = int(m_box[3] - m_box[2] + 1), i1_max = i1;
    
    CNWAligner::TScore score_max = 0, s = 0;
    
    const CNWAligner::TScore wm =  1;
    const CNWAligner::TScore wms = -1;
    const CNWAligner::TScore wg =  0;
    const CNWAligner::TScore ws =  -1;
  
    string::reverse_iterator irs0 = m_details.rbegin(),
        irs1 = m_details.rend(), irs = irs0, irs_max = irs0;
    
    for( ; irs != irs1; ++irs) {
        
        switch(*irs) {
            
        case 'M': {
            s += wm;
            --i0;
            --i1;
        }
        break;
            
        case 'R': {
            s += wms;
            --i0;
            --i1;
        }
        break;
            
        case 'I': {
            s += ws;
            if(irs > irs0 && *(irs-1)!='I') s += wg;
            --i1;
        }
        break;

        case 'D': {
            s += ws;
            if(irs > irs0 && *(irs-1)!='D') s += wg;
            --i0;
        }
        }

        if(s >= score_max) {
            score_max = s;
            i0_max = i0;
            i1_max = i1;
            irs_max = irs;
        }
    }

    // work around a weird case of equally optimal
    // but detrimental for our purposes alignment
    // -check the actual sequence chars
    size_t head = 0;
    while(i0_max > 0 && i1_max > 0) {
        if( toupper (seq1[m_box[0]+i0_max-1]) != 'N' && seq1[m_box[0]+i0_max-1] == seq2[m_box[2]+i1_max-1]) {
            --i0_max; --i1_max;
            ++head;
        }
        else {
            break;
        }
    }

    if(i0_max == 0 && i1_max == 0) return;//no chages
    
    // if the resulting segment is still long enough
    if(m_box[1] - m_box[0] + 1 - i0_max >= min_query_size )
    {
        // resize
        m_box[0] += i0_max;
        m_box[2] += i1_max;
        const size_t L = m_details.size() - (irs_max - irs0 + 1);
        m_details.erase(0, L);
        m_details.insert(m_details.begin(), head, 'M');
        Update(aligner.GetNonNullPointer());
        
        // update the first two annotation symbols
        if(m_annot.size() > 2 && m_annot[2] == '<') {
            int  j1 = int(m_box[2]) - 2;
            char c1 = j1 >= 0? seq2[j1]: ' ';
            m_annot[0] = c1;
            int  j2 = int(m_box[2]) - 1;
            char c2 = j2 >= 0? seq2[j2]: ' ';
            m_annot[1] = c2;
        }
    } else {
        SetToGap();//just drop it
    }
}

size_t CNWFormatter::SSegment::GapLength()
{
    size_t gap_count = 0;
    ITERATE(string, irs, m_details) {
        switch(*irs) {            
        case 'I':
        case 'D':
            ++gap_count;
            break;
        default:
            break;
        }
    }
    return gap_count;
}


bool CNWFormatter::SSegment::IsLowComplexityExon(const char *rna_seq)
{
    map<char, size_t> count;
    for(size_t i = m_box[0]; i<=m_box[1]; ++i) {
        ++count[rna_seq[i]];
    }
    size_t gap_len = GapLength();
    for(map<char, size_t>::iterator i = count.begin(); i != count.end(); ++i) {
        if( m_len * 70 <= 100 * (i->second + gap_len) ) {
            return true;
        }
    }
    return false;
}
   

// try improving the segment by cutting it from the right
void CNWFormatter::SSegment::ImproveFromRight1(const char* seq1, const char* seq2,
                                              CConstRef<CSplicedAligner> aligner)
{
    const size_t min_query_size = 4;
    //legacy check
    if(m_box[1] - m_box[0] + 1 < min_query_size) {
        SetToGap();
        return;
    }

    //identity total
    int len_total = (int)m_details.size();
    int match_total = 0;
    string::iterator irs0 = m_details.begin(),
        irs1 = m_details.end(), irs;

    for(irs = irs0; irs != irs1; ++irs) {
        if(*irs == 'M') {
            ++match_total;
        }
    }

    //count identity at the left end
    int cnt = 0, max_cnt = 20;
    int len = 0, match = 0;
    for( irs = irs0; ( irs != irs1 ) && (cnt != max_cnt) ; ++irs, ++cnt) {
        ++len;
        if(*irs == 'M') {
            ++match;
        }
    }
    double ident = match/(double)len;

    double epsilon = 1e-10;
    const double dropoff_diff = .19; 

    int i0 = int(m_box[1] - m_box[0] + 1), i0_max = i0;
    int i1 = int(m_box[3] - m_box[2] + 1), i1_max = i1;
    match = 0;
    len = 0;    
    string::reverse_iterator rirs0 = m_details.rbegin(),
        rirs1 = m_details.rend(), rirs = rirs0, rirs_max;

    --rirs1;
    for( ; rirs != rirs1; ++rirs) {
        
        switch(*rirs) {
            
        case 'M': {
            ++match;
            --i0;
            --i1;
        }
        break;
            
        case 'R': {
            --i0;
            --i1;
        }
        break;
      
        case 'I': {
            --i1;
        }
        break;

        case 'D': {
            --i0;
        }
        }
        ++len;
        
        //trim here if        
        if( max( ident, (match_total - match)/(double)(len_total-len) ) - match/(double)len - dropoff_diff > epsilon ) {
            i0_max = i0;
            i1_max = i1;
            rirs_max = rirs;
            //do not count trimmed part, adjust values
            match_total -= match;
            len_total -= len;
            match = 0;
            len = 0;
        }
    }
    
    int dimq = int(m_box[1] - m_box[0] + 1);
    int dims = int(m_box[3] - m_box[2] + 1);
    
    // work around a weird case of equally optimal
    // but detrimental for our purposes alignment
    // -check the actual sequences
    size_t tail = 0;
    while(i0_max < dimq  && i1_max < dims ) {
        if( toupper(seq1[m_box[0]+i0_max]) != 'N' && seq1[m_box[0]+i0_max] == seq2[m_box[2]+i1_max]) {
            ++i0_max; ++i1_max;
            ++tail;
        }
        else {
            break;
        }
    }

    if( i0_max >= dimq && i1_max >= dims ) return;//no changes

    // if the resulting segment is still long enough
    if(i0_max - 1 >= int(min_query_size) ) {
        
        m_box[1] = m_box[0] + i0_max - 1;
        m_box[3] = m_box[2] + i1_max - 1;
        
        m_details.resize(m_details.size() - (rirs_max - rirs0 + 1));
        m_details.insert(m_details.end(), tail, 'M');
        Update(aligner.GetNonNullPointer());
        
        // update the last two annotation chars
        const size_t adim = m_annot.size();
        if(adim > 2 && m_annot[adim - 3] == '>') {

            const size_t len2 (aligner->GetSeqLen2());
            const char c3 (m_box[3] + 1 < len2? seq2[m_box[3] + 1]: ' ');
            const char c4 (m_box[3] + 2 < len2? seq2[m_box[3] + 2]: ' ');
            m_annot[adim-2] = c3;
            m_annot[adim-1] = c4;
        }
    } else {
        SetToGap();//just drop it
    }
}



// try improving the segment by cutting it from the right
void CNWFormatter::SSegment::ImproveFromRight(const char* seq1, const char* seq2,
                                              CConstRef<CSplicedAligner> aligner)
{
    const size_t min_query_size = 4;
    
    if(m_box[1] - m_box[0] + 1 < min_query_size) {
        SetToGap();
        return;
    }
    
    // find the top score prefix
    int i0 = -1, i0_max = i0;
    int i1 = -1, i1_max = i1;

    CNWAligner::TScore score_max = 0, s = 0;
    
    const CNWAligner::TScore wm =  1;
    const CNWAligner::TScore wms = -1;
    const CNWAligner::TScore wg =  0;
    const CNWAligner::TScore ws =  -1;
    
    string::iterator irs0 = m_details.begin(),
        irs1 = m_details.end(), irs = irs0, irs_max = irs0;
    
    for( ; irs != irs1; ++irs) {
        
        switch(*irs) {
            
        case 'M': {
            s += wm;
            ++i0;
            ++i1;
        }
        break;
            
        case 'R': {
            s += wms;
            ++i0;
            ++i1;
        }
        break;
      
        case 'I': {
            s += ws;
            if(irs > irs0 && *(irs-1) != 'I') s += wg;
            ++i1;
        }
        break;

        case 'D': {
            s += ws;
            if(irs > irs0 && *(irs-1) != 'D') s += wg;
            ++i0;
        }
    }
        
        if(s >= score_max) {
            score_max = s;
            i0_max = i0;
            i1_max = i1;
            irs_max = irs;
        }
    }
    
    int dimq = int(m_box[1] - m_box[0] + 1);
    int dims = int(m_box[3] - m_box[2] + 1);
    
    // work around a weird case of equally optimal
    // but detrimental for our purposes alignment
    // -check the actual sequences
    size_t tail = 0;
    while(i0_max < dimq - 1  && i1_max < dims - 1) {
        if( toupper(seq1[m_box[0]+i0_max+1]) != 'N' && seq1[m_box[0]+i0_max+1] == seq2[m_box[2]+i1_max+1] ) {
            ++i0_max; ++i1_max;
            ++tail;
        }
        else {
            break;
        }
    }
    
    dimq += tail;
    dims += tail;
    
    if(i0_max >= dimq - 1 && i1_max >= dims - 1) return;//no changes

    // if the resulting segment is still long enough
    if(i0_max >= int(min_query_size) ) {
        
        m_box[1] = m_box[0] + i0_max;
        m_box[3] = m_box[2] + i1_max;
        
        m_details.resize(irs_max - irs0 + 1);
        m_details.insert(m_details.end(), tail, 'M');
        Update(aligner.GetNonNullPointer());
        
        // update the last two annotation chars
        const size_t adim = m_annot.size();
        if(adim > 2 && m_annot[adim - 3] == '>') {

            const size_t len2 (aligner->GetSeqLen2());
            const char c3 (m_box[3] + 1 < len2? seq2[m_box[3] + 1]: ' ');
            const char c4 (m_box[3] + 2 < len2? seq2[m_box[3] + 2]: ' ');
            m_annot[adim-2] = c3;
            m_annot[adim-1] = c4;
        }
    } else {
        SetToGap();//just drop it
    }
}

//check if 100% extension is possible, returns the length of possible extension
int CNWFormatter::SSegment::CanExtendRight(const vector<char>& mrna, const vector<char>& genomic) const
{
    Int8 mind0 = m_box[1] + 1;
    Int8 mind = mind0;
    Int8 gind = m_box[3] + 1;
    for(; mind < (int)mrna.size() && gind < (int)genomic.size(); ++gind, ++mind) {
        if( toupper(mrna[mind]) == 'N' || mrna[mind] != genomic[gind] ) break;
    }
    return mind - mind0;
}


//check if 100% extension is possible, returns the length of possible extension
int CNWFormatter::SSegment::CanExtendLeft(const vector<char>& mrna, const vector<char>& genomic) const
{
    int mind0 = (int)m_box[0] - 1;
    int mind = mind0;
    int gind = (int)m_box[2] - 1;
    for(; mind >= 0 && gind >= 0; --mind, --gind) {
        if(  toupper(mrna[mind]) == 'N' || mrna[mind] != genomic[gind] ) break;
    }
    return mind0 - mind;
}

//do extend, 100% identity in extension is implied
void CNWFormatter::SSegment::ExtendRight(const vector<char>& mrna, const vector<char>& genomic, Int8 ext_len, const CNWAligner* aligner)
{
    if(ext_len > 0) {
        m_box[1] += ext_len;
        m_box[3] += ext_len;
        m_details.append(ext_len, 'M');
        Update(aligner);
        // fix annotation
        const size_t ann_dim = m_annot.size();
        if(ann_dim > 2 && m_annot[ann_dim - 3] == '>') {
            m_annot[ann_dim - 2] =  (m_box[3] + 1) < genomic.size() ? genomic[m_box[3] + 1] : ' ';
            m_annot[ann_dim - 1] =  (m_box[3] + 2) < genomic.size() ? genomic[m_box[3] + 2] : ' ';
        }
    }
}

//do extend, 100% identity in extension is implied
void CNWFormatter::SSegment::ExtendLeft(const vector<char>& mrna, const vector<char>& genomic, Int8 ext_len, const CNWAligner* aligner)
{
    if(ext_len > 0) {
        m_box[0] -= ext_len;
        m_box[2] -= ext_len;
        m_details.insert(m_details.begin(), ext_len, 'M');
        Update(aligner);
        //fix annotation
        if( ( m_annot.size() > 2 ) && ( m_annot[2]  == '<' ) ) {
            m_annot[1] =  m_box[2] >= 1 ? genomic[m_box[2] - 1] : ' ';
            m_annot[0] =  m_box[2] >= 2 ? genomic[m_box[2] - 2] : ' ';
        }
    }
}


void CNWFormatter::SSegment::Update(const CNWAligner* paligner)
{
    // restore length and identity
    m_len = m_details.size();

    string::const_iterator ib = m_details.begin(), ie = m_details.end();
    size_t count (0); // std::count() not supported on some platforms
    for(string::const_iterator ii = ib; ii != ie; ++ii) {
        if(*ii == 'M') ++count;
    }
    m_idty = double(count) / m_len;
    
    const size_t xcript_dim (m_details.size());
    CNWAligner::TTranscript transcript (xcript_dim);
    for(size_t i (0); i < xcript_dim; ++i) {
        transcript[i] = CNWAligner::ETranscriptSymbol(m_details[i]);
    }

    m_score = float(paligner->CNWAligner::ScoreFromTranscript(transcript)) /
        paligner->GetWm();
}


const char* CNWFormatter::SSegment::GetDonor() const 
{
    const size_t adim = m_annot.size();
    return
      (adim > 2 && m_annot[adim - 3] == '>')? (m_annot.c_str() + adim - 2): 0;
}


const char* CNWFormatter::SSegment::GetAcceptor() const 
{
    const size_t adim = m_annot.size();
    return (adim > 3 && m_annot[2] == '<')? m_annot.c_str(): 0;
}


bool CNWFormatter::SSegment::s_IsConsensusSplice(const char* donor,
                                                 const char* acceptor,
                                                 bool semi_as_cons)
{
    if(!donor || !acceptor) return false;

    bool rv;
    if(semi_as_cons) {

        if(acceptor[0] == 'A') {
            if(donor[0] == 'G' && acceptor[1] == 'G') {
                rv = donor[1] == 'T' || donor[1] == 'C';
            }
            else {
                rv = donor[0] == 'A' && donor[1] == 'T' && acceptor[1] == 'C';
            }
        }
        else {
            rv = false;
        }
    }
    else {
        rv = donor[0] == 'G' && donor[1] == 'T' 
             && acceptor[0] == 'A' && acceptor[1] == 'G';
    }

    return rv;
}
void CNWFormatter::MakeSegments(deque<SSegment>* psegments) const
{
    vector<SSegment> v;
    MakeSegments(&v);
    psegments->clear();
    copy(v.begin(), v.end(), psegments->begin());
}

void CNWFormatter::MakeSegments(vector<SSegment>* psegments) const
{
    const CNWAligner::TTranscript transcript (m_aligner->GetTranscript());
    if(transcript.size() == 0) {
        NCBI_THROW(CAlgoAlignException, eNoSeqData, g_msg_NoAlignment);
    }

    vector<SSegment>& segments(*psegments);
    segments.resize(0);

    bool esfL1, esfR1, esfL2, esfR2;
    m_aligner->GetEndSpaceFree(&esfL1, &esfR1, &esfL2, &esfR2);
    const size_t len2  (m_aligner->GetSeqLen2());
    const char* start1 (m_aligner->GetSeq1());
    const char* start2 (m_aligner->GetSeq2());
    const char* p1     (start1);
    const char* p2     (start2);
    Int8 tr_idx_hi0 (transcript.size() - 1), tr_idx_hi (tr_idx_hi0);
    Int8 tr_idx_lo0 (0), tr_idx_lo (tr_idx_lo0);

    while(transcript[tr_idx_hi] == CNWAligner::eTS_SlackInsert
          || transcript[tr_idx_hi] == CNWAligner::eTS_SlackDelete)
    {
        if(transcript[tr_idx_hi] == CNWAligner::eTS_SlackInsert) {
            ++p2;
        }
        else {
            ++p1;
        }
        --tr_idx_hi;
    }

    if(esfL1 && transcript[tr_idx_hi0] == CNWAligner::eTS_Insert) {
        while(esfL1 && transcript[tr_idx_hi] == CNWAligner::eTS_Insert) {
            --tr_idx_hi;
            ++p2;
        }
    }

    if(esfL2 && transcript[tr_idx_hi0] == CNWAligner::eTS_Delete) {
        while(esfL2 && transcript[tr_idx_hi] == CNWAligner::eTS_Delete) {
            --tr_idx_hi;
            ++p1;
        }
    }

    if(esfR1 && transcript[tr_idx_lo0] == CNWAligner::eTS_Insert) {
        while(esfR1 && transcript[tr_idx_lo] == CNWAligner::eTS_Insert) {
            ++tr_idx_lo;
        }
    }

    if(esfR2 && transcript[tr_idx_lo0] == CNWAligner::eTS_Delete) {
        while(esfR2 && transcript[tr_idx_lo] == CNWAligner::eTS_Delete) {
            ++tr_idx_lo;
        }
    }

    vector<char> trans_ex (tr_idx_hi - tr_idx_lo + 1);

    for(int tr_idx (tr_idx_hi); tr_idx >= tr_idx_lo; ) {

        const char * p1_beg (p1), * p1_x (0);
        const char * p2_beg (p2);
        size_t matches (0), exon_aln_size (0), exon_aln_size_x(0);
        int tr_idx_x (-1);

        vector<char>::iterator ii_ex (trans_ex.begin()), ii_ex_x;
        size_t cons_dels (0);
        const size_t max_cons_dels (25);
        while(tr_idx >= tr_idx_lo && transcript[tr_idx] < CNWAligner::eTS_Intron) {
                
            bool noins (transcript[tr_idx] != CNWAligner::eTS_Insert);
            bool nodel (transcript[tr_idx] != CNWAligner::eTS_Delete);
            if(noins && nodel) {
                
                if(cons_dels > max_cons_dels) {
                    break;
                }

                cons_dels = 0;

                if(toupper(*p1) != 'N' && *p1 == *p2) {
                    ++matches;
                    *ii_ex++ = 'M';
                }
                else {
                    *ii_ex++ = 'R';
                }
                ++p1;
                ++p2;
            } else if(noins) {

                if(cons_dels == 0) {
                    p1_x = p1;
                    ii_ex_x = ii_ex;
                    exon_aln_size_x = exon_aln_size;
                    tr_idx_x = tr_idx;
                }
                
                ++p1;
                *ii_ex++ = 'D';
                ++cons_dels;
            } else {

                ++p2;
                *ii_ex++ = 'I';
                cons_dels = 0;
            }
            --tr_idx;
            ++exon_aln_size;
        }

        if(cons_dels > max_cons_dels) {
            swap(p1, p1_x);
            swap(ii_ex, ii_ex_x);
            swap(exon_aln_size, exon_aln_size_x);
            swap(tr_idx, tr_idx_x);
        }

        if(exon_aln_size > 0) {

            segments.push_back(SSegment());
            SSegment& s = segments.back();

            s.m_exon = true;
            s.m_idty = float(matches) / exon_aln_size;
            s.m_len = exon_aln_size;

            size_t beg1 (p1_beg - start1), end1 (p1 - start1 - 1);
            size_t beg2 (p2_beg - start2), end2 (p2 - start2 - 1);

            s.m_box[0] = beg1;
            s.m_box[1] = end1;
            s.m_box[2] = beg2;
            s.m_box[3] = end2;

            char c1 ((p2_beg >= start2 + 2)? *(p2_beg - 2): ' ');
            char c2 ((p2_beg >= start2 + 1)? *(p2_beg - 1): ' ');
            char c3 ((p2 < start2 + len2)? *(p2): ' ');
            char c4 ((p2 < start2 + len2 - 1)? *(p2+1): ' ');

            s.m_annot.resize(10);
            s.m_annot[0] = c1;
            s.m_annot[1] = c2;
            const string s_exontag ("<exon>");
            copy(s_exontag.begin(), s_exontag.end(), s.m_annot.begin() + 2);
            s.m_annot[8] = c3;
            s.m_annot[9] = c4;
            s.m_details.resize(ii_ex - trans_ex.begin());
            copy(trans_ex.begin(), ii_ex, s.m_details.begin());
            s.Update(m_aligner);
        }

        if(cons_dels > max_cons_dels) {

            segments.push_back(SSegment());
            SSegment& s (segments.back());

            s.m_exon = false;
            s.m_idty = 0;
            s.m_len = exon_aln_size_x - exon_aln_size;

            size_t beg1 (p1 - start1),     end1 (p1_x - start1 - 1);
            size_t beg2 (0), end2 (0);

            s.m_box[0] = beg1;
            s.m_box[1] = end1;
            s.m_box[2] = beg2;
            s.m_box[3] = end2;

            s.m_annot = "<gap>";
            s.m_details.resize(ii_ex_x - ii_ex);
            copy(ii_ex, ii_ex_x, s.m_details.begin());

            swap(p1,            p1_x);
            swap(ii_ex,         ii_ex_x);
            swap(exon_aln_size, exon_aln_size_x);
            swap(tr_idx,        tr_idx_x);
        }

        if(tr_idx<tr_idx_lo || transcript[tr_idx] == CNWAligner::eTS_SlackInsert 
           || transcript[tr_idx] == CNWAligner::eTS_SlackDelete)
        {
            break;
        }

        // find next exon
        while(tr_idx >= tr_idx_lo && (transcript[tr_idx] == CNWAligner::eTS_Intron)) {
            --tr_idx;
            ++p2;
        }
    }
}


void CNWFormatter::AsText(string* output, ETextFormatType type, size_t line_width)
  const
{
    CNcbiOstrstream ss;

    const CNWAligner::TTranscript transcript = m_aligner->GetTranscript();
    if(transcript.size() == 0) {
        NCBI_THROW(CAlgoAlignException,
                   eNoSeqData,
                   g_msg_NoAlignment);
    }

    const string strid_query = m_Seq1Id->GetSeqIdString(true);
    const string strid_subj = m_Seq2Id->GetSeqIdString(true);

    switch (type) {

    case eFormatType1: {

        ss << '>' << strid_query << '\t' << strid_subj << endl;

        vector<char> v1, v2;
        unsigned i1 (0), i2 (0);
        size_t aln_size (x_ApplyTranscript(&v1, &v2));
        for (size_t i = 0;  i < aln_size; ) {

            ss << i << '\t' << i1 << ':' << i2 << endl;
            Int8 i0 = i;
            for (size_t jPos = 0;  i < aln_size  &&  jPos < line_width; ++i, ++jPos) {
                char c1 (v1[i0 + jPos]);
                ss << c1;
                if(c1 != '-' && c1 != 'x' && c1 != '+') ++i1;
            }
            ss << endl;
            
            string marker_line(line_width, ' ');
            i = i0;
            for (size_t jPos = 0;  i < aln_size  &&  jPos < line_width; ++i, ++jPos) {
                char c1 (v1[i0 + jPos]);
                char c2 (v2[i0 + jPos]);
                ss << c2;
                if(c2 != '-' && c2 != '+' && c2 != 'x')
                    i2++;
                if( c2 != '-' && c1 != '-' && c1 != '+' && c1 != 'x' && ( toupper(c2) != toupper(c1) || m_aligner->GetScoreMatrix().s[(size_t)c1][(size_t)c2] <= 0 ))
                    marker_line[jPos] = '^';
            }
            ss << endl << marker_line << endl;
        }
    }
    break;

    case eFormatType2: {

        ss << '>' << strid_query << '\t' << strid_subj << endl;

        vector<char> v1, v2;
        unsigned i1 (0), i2 (0);
        size_t aln_size (x_ApplyTranscript(&v1, &v2));
        for (size_t i = 0;  i < aln_size; ) {
            ss << i << '\t' << i1 << ':' << i2 << endl;
            Int8 i0 = i;
            for (size_t jPos = 0;  i < aln_size  &&  jPos < line_width; ++i, ++jPos) {
                char c (v1[i0 + jPos]);
                ss << c;
                if(c != '-' && c != '+' && c != 'x') ++i1;
            }
            ss << endl;
            
            string line2 (line_width, ' ');
            string line3 (line_width, ' ');
            i = i0;
            for (size_t jPos = 0;  i < aln_size  &&  jPos < line_width; ++i, ++jPos) {
                char c1 (v1[i0 + jPos]);
                char c2 (v2[i0 + jPos]);
                if(c2 != '-' && c2 != '+' && c2 != 'x') i2++;
                if( toupper(c2) == toupper(c1) && m_aligner-> GetScoreMatrix().s[(size_t)c1][(size_t)c2] > 0 )  line2[jPos] = '|';
                line3[jPos] = c2;
            }
            ss << line2 << endl << line3 << endl << endl;
        }
    }
    break;

    case eFormatAsn: {

        CRef<CSeq_align> sa = AsSeqAlign();
        CObjectOStreamAsn asn_stream (ss);
        asn_stream << *sa;
        asn_stream << Separator;
    }
    break;

    case eFormatDenseSeg: {

        CRef<CDense_seg> ds = AsDenseSeg();
        CObjectOStreamAsn asn_stream (ss);
        asn_stream << *ds;
        asn_stream << Separator;
    }
    break;

    case eFormatFastA: {
        vector<char> v1, v2;
        size_t aln_size (x_ApplyTranscript(&v1, &v2));
        
        ss << '>' << strid_query << endl;
        const vector<char>* pv = &v1;
        for(size_t i = 0; i < aln_size; ) {
            for(size_t j = 0; j < line_width && i < aln_size; ++j, ++i) {
                ss << (*pv)[i];
            }
            ss << endl;
        }

        ss << '>' << strid_subj << endl;
        pv = &v2;
        for(size_t i = 0; i < aln_size; ) {
            for(size_t j = 0; j < line_width && i < aln_size; ++j, ++i) {
                ss << (*pv)[i];
            }
            ss << endl;
        }
    }
    break;
    
    case eFormatExonTable:
    case eFormatExonTableEx:  {

        ss.precision(3);

        typedef deque<SSegment> TSegments;
        TSegments segments;
        MakeSegments(&segments);
        ITERATE(TSegments, ii, segments) {

            ss << strid_query << '\t' << strid_subj << '\t';
            ss << ii->m_idty << '\t' << ii->m_len << '\t';
            copy(ii->m_box, ii->m_box + 4, 
                 ostream_iterator<size_t>(ss,"\t"));
            ss << '\t' << ii->m_annot;
            if(type == eFormatExonTableEx) {
                ss << '\t' << ii->m_details;
            }
            ss << endl;
        }
    }
    break;

    default:
        NCBI_THROW(CAlgoAlignException, eBadParameter, "Incorrect format specified");
    }

    *output = CNcbiOstrstreamToString(ss);
}



// Transform source sequences according to the transcript.
// cut flank gaps for Smith-Waterman
// Write the results to v1 and v2 leaving source sequences intact.
// Return alignment size.
size_t CNWFormatter::x_ApplyTranscript(vector<char>* pv1, vector<char>* pv2)
    const
{
    const CNWAligner::TTranscript transcript = m_aligner->GetTranscript();

    vector<char>& v1 (*pv1);
    vector<char>& v2 (*pv2);

    v1.clear();
    v2.clear();

    if(transcript.size() == 0) {
        return 0;
    }


    vector<CNWAligner::ETranscriptSymbol>::const_reverse_iterator
        ib = transcript.rbegin(),
        ie = transcript.rend(),
        ii;

    if( m_aligner->IsSmithWaterman() ) {
        --ie;
        while( ( ie != ib ) &&
               ( *ie == CNWAligner::eTS_Insert || *ie == CNWAligner::eTS_Delete || *ie == CNWAligner::eTS_Intron ||
                 *ie == CNWAligner::eTS_SlackInsert || *ie == CNWAligner::eTS_SlackDelete ) ) {
            --ie;
        }
    }

    const char* iv1 (m_aligner->GetSeq1());
    const char* iv2 (m_aligner->GetSeq2());

    bool sw_ini_gap = false;
    if( m_aligner->IsSmithWaterman() ) {
        sw_ini_gap = true;
    }

    for (ii = ib;  ii != ie;  ii++) {

        CNWAligner::ETranscriptSymbol ts (*ii);
        char c1, c2;
        switch ( ts ) {

        case CNWAligner::eTS_Insert:
            c1 = '-';
            c2 = *iv2++;
            break;

        case CNWAligner::eTS_SlackInsert:
            c1 = 'x';
            c2 = *iv2++;
            break;

        case CNWAligner::eTS_Delete:
            c2 = '-';
            c1 = *iv1++;
            break;


        case CNWAligner::eTS_SlackDelete:
            c2 = 'x';
            c1 = *iv1++;
            break;

        case CNWAligner::eTS_Match:
        case CNWAligner::eTS_Replace:
            sw_ini_gap = false;
            c1 = *iv1++;
            c2 = *iv2++;
            break;

        case CNWAligner::eTS_Intron:
            c1 = '+';
            c2 = *iv2++;
            break;

        default:
            sw_ini_gap = false;
            c1 = c2 = '?';
            break;
        }
        if( !sw_ini_gap ) {
            v1.push_back(c1);
            v2.push_back(c2);
        }
    }

    return v1.size();
}


END_NCBI_SCOPE
