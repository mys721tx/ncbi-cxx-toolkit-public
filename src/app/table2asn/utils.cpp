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
* Authors:  
*
* File Description:
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include "utils.hpp"
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

void g_LogGeneralParsingError(EDiagSev sev, const string& idString, const string& msg, objects::ILineErrorListener& listener)
{
    listener.PutError(*unique_ptr<CLineError>(
                CLineError::Create(ILineError::eProblem_GeneralParsingError, sev, idString, 0, 
                    "", "", "", msg)));
}

void g_LogGeneralParsingError(EDiagSev sev, const string& msg, objects::ILineErrorListener& listener)
{
    g_LogGeneralParsingError(sev, "", msg, listener);
}

void g_LogGeneralParsingError(const string& msg, objects::ILineErrorListener& listener)
{
    g_LogGeneralParsingError(eDiag_Error, msg, listener);
}
    
END_NCBI_SCOPE
