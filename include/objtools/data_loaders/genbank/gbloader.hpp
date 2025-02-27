#ifndef GBLOADER__HPP_INCLUDED
#define GBLOADER__HPP_INCLUDED

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
*  ===========================================================================
*
*  Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko,
*          Anton Butanayev
*
*  File Description:
*   Data loader base class for object manager
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/ncbi_param.hpp>

#if !defined(NDEBUG) && defined(DEBUG_SYNC)
// for GBLOG_POST()
# include <corelib/ncbithr.hpp>
#endif

#include <objmgr/data_loader.hpp>

#include <util/mutex_pool.hpp>

#include <objtools/data_loaders/genbank/blob_id.hpp>
#include <objtools/data_loaders/genbank/impl/gbload_util.hpp>
#include <objtools/data_loaders/genbank/impl/cache_manager.hpp>

#include <util/cache/icache.hpp>

#define GENBANK_NEW_READER_WRITER

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////////
//
// GBDataLoader
//

class CReader;
class CSeqref;
class CReadDispatcher;
class CGBInfoManager;

// Parameter names used by loader factory

class NCBI_XLOADER_GENBANK_EXPORT CGBLoaderParams
{
public:
    // possible parameters
    typedef string TReaderName;
    typedef CReader* TReaderPtr;
    typedef TPluginManagerParamTree TParamTree;
    typedef const TPluginManagerParamTree* TParamTreePtr;
    enum EPreopenConnection {
        ePreopenNever,
        ePreopenAlways,
        ePreopenByConfig
    };

    // no options
    CGBLoaderParams(void);
    // one option
    CGBLoaderParams(const string& reader_name);
    CGBLoaderParams(TReaderPtr reader_ptr);
    CGBLoaderParams(TParamTreePtr param_tree);
    CGBLoaderParams(EPreopenConnection preopen);

    ~CGBLoaderParams(void);
    // copy
    CGBLoaderParams(const CGBLoaderParams&);
    CGBLoaderParams& operator=(const CGBLoaderParams&);

    void SetLoaderName(const string& loader_name)
        {
            m_LoaderName = loader_name;
        }
    const string& GetLoaderName(void) const
        {
            return m_LoaderName;
        }

    void SetLoaderMethod(const string& loader_method)
    {
        m_LoaderMethod = loader_method;
    }

    const string& GetLoaderMethod(void) const
    {
        return m_LoaderMethod;
    }

    void SetReaderName(const string& reader_name)
        {
            m_ReaderName = reader_name;
        }
    const string& GetReaderName(void) const
        {
            return m_ReaderName;
        }

    void SetWriterName(const string& writer_name)
        {
            m_WriterName = writer_name;
        }
    const string& GetWriterName(void) const
        {
            return m_WriterName;
        }

    void SetReaderPtr(CReader* reader_ptr);
    CReader* GetReaderPtr(void) const;

    void SetParamTree(const TParamTree* params);
    const TParamTree* GetParamTree(void) const
        {
            return m_ParamTree;
        }
    
    void SetPreopenConnection(EPreopenConnection preopen = ePreopenAlways)
        {
            m_Preopen = preopen;
        }
    EPreopenConnection GetPreopenConnection(void) const
        {
            return m_Preopen;
        }
    void SetHUPIncluded(bool include_hup = true, 
                        const string& web_cookie = NcbiEmptyString)
        {
            m_HasHUPIncluded = include_hup;
            m_WebCookie = web_cookie;
        }
    bool HasHUPIncluded(void) const
        {
            return m_HasHUPIncluded;
        }

    const string& GetWebCookie(void) const
        {
            return m_WebCookie;
        }

    const string& GetPSGServiceName(void) const
    {
        return m_PSGServiceName;
    }

    void SetPSGServiceName(const string& service_name)
    {
        m_PSGServiceName = service_name;
    }

    bool GetUsePSG() const;
    void SetUsePSG(bool use_psg = true)
    {
        m_UsePSG = use_psg;
        m_UsePSGInitialized = true;
    }

    bool GetPSGNoSplit(void) const
    {
        return m_PSGNoSplit;
    }

    void SetPSGNoSplit(bool no_split)
    {
        m_PSGNoSplit = no_split;
    }

    bool IsSetEnableSNP(void) const;
    bool GetEnableSNP(void) const;
    void SetEnableSNP(bool enable);

    bool IsSetEnableWGS(void) const;
    bool GetEnableWGS(void) const;
    void SetEnableWGS(bool enable);

    bool IsSetEnableCDD(void) const;
    bool GetEnableCDD(void) const;
    void SetEnableCDD(bool enable);

private:
    string m_ReaderName;
    string m_WriterName;
    string m_LoaderMethod;
    CRef<CReader> m_ReaderPtr;
    const TParamTree* m_ParamTree;
    EPreopenConnection m_Preopen;
    mutable bool m_UsePSGInitialized; // can be changed in GetUsePSG()
    mutable bool m_UsePSG;
    bool m_PSGNoSplit;
    bool m_HasHUPIncluded;
    string m_WebCookie;
    string m_LoaderName;
    string m_PSGServiceName;
    CNullable<bool> m_EnableSNP;
    CNullable<bool> m_EnableWGS;
    CNullable<bool> m_EnableCDD;

    friend class CGBDataLoader_Native;
};

class NCBI_XLOADER_GENBANK_EXPORT CGBDataLoader : public CDataLoader
{
public:
    // typedefs from CReader
    typedef unsigned                  TConn;
    typedef CBlob_id                  TRealBlobId;
    typedef set<string>               TNamedAnnotNames;

    virtual ~CGBDataLoader(void);

    TBlobId GetBlobIdFromSatSatKey(int sat,
        int sat_key,
        int sub_sat = 0) const;

    // Create GB loader and register in the object manager if
    // no loader with the same name is registered yet.
    typedef SRegisterLoaderInfo<CGBDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CReader* reader = 0,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(CReader* reader = 0);

    // enum to include HUP data
    enum EIncludeHUP {
        eIncludeHUP
    };
    
    // Select reader by name. If failed, select default reader.
    // Reader name may be the same as in environment: PUBSEQOS, ID1 etc.
    // Several names may be separated with ":". Empty name or "*"
    // included as one of the names allows to include reader names
    // from environment and registry.
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string&   reader_name,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(const string& reader_name);

    // GBLoader with HUP data included.
    // The reader will be chosen from default configuration,
    // either pubseqos or pubseqos2.
    // The default loader priority will be slightly lower than for main data.
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        EIncludeHUP     include_hup,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(EIncludeHUP     include_hup);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        EIncludeHUP     include_hup,
        const string& web_cookie,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(EIncludeHUP     include_hup,
                                        const string& web_cookie);

    // GBLoader with HUP data included.
    // The reader can be either pubseqos or pubseqos2.
    // The default loader priority will be slightly lower than for main data.
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string&   reader_name, // pubseqos or pubseqos2
        EIncludeHUP     include_hup,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(const string&   reader_name, // pubseqos or pubseqos2
                                        EIncludeHUP     include_hup);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string&   reader_name, // pubseqos or pubseqos2
        EIncludeHUP     include_hup,
        const string& web_cookie,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(const string&   reader_name, // pubseqos or pubseqos2
                                        EIncludeHUP     include_hup,
                                        const string&   web_cookie);

    // Setup loader using param tree. If tree is null or failed to find params,
    // use environment or select default reader.
    typedef TPluginManagerParamTree TParamTree;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const TParamTree& params,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(const TParamTree& params);

    // Setup loader using param tree. If tree is null or failed to find params,
    // use environment or select default reader.
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const CGBLoaderParams& params,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(const CGBLoaderParams& params);

    virtual TNamedAnnotNames GetNamedAnnotAccessions(const CSeq_id_Handle& idh) = 0;
    virtual TNamedAnnotNames GetNamedAnnotAccessions(const CSeq_id_Handle& idh,
        const string& named_acc) = 0;

    CConstRef<CSeqref> GetSatSatkey(const CSeq_id_Handle& idh);
    CConstRef<CSeqref> GetSatSatkey(const CSeq_id& id);

    TRealBlobId GetRealBlobId(const TBlobId& blob_id) const;
    TRealBlobId GetRealBlobId(const CTSE_Info& tse_info) const;

    // params modifying access methods
    // argument params should be not-null
    // returned value is not null - subnode will be created if necessary
    static TParamTree* GetParamsSubnode(TParamTree* params,
                                        const string& subnode_name);
    static TParamTree* GetLoaderParams(TParamTree* params);
    static TParamTree* GetReaderParams(TParamTree* params,
                                       const string& reader_name);
    static void SetParam(TParamTree* params,
                         const string& param_name,
                         const string& param_value);
    // params non-modifying access methods
    // argument params may be null
    // returned value may be null if params argument is null
    // or if subnode is not found
    static const TParamTree* GetParamsSubnode(const TParamTree* params,
                                              const string& subnode_name);
    static const TParamTree* GetLoaderParams(const TParamTree* params);
    static const TParamTree* GetReaderParams(const TParamTree* params,
                                             const string& reader_name);
    static string GetParam(const TParamTree* params,
                           const string& param_name);

    enum ECacheType {
        fCache_Id   = CReaderCacheManager::fCache_Id,
        fCache_Blob = CReaderCacheManager::fCache_Blob,
        fCache_Any  = CReaderCacheManager::fCache_Any
    };
    typedef CReaderCacheManager::TCacheType TCacheType;
    virtual bool HaveCache(TCacheType cache_type = fCache_Any) = 0;

    virtual void PurgeCache(TCacheType            cache_type,
        time_t                access_timeout = 0) = 0;
    virtual void CloseCache(void) = 0;

    // expiration timout in seconds, must be positive
    typedef Uint4 TExpirationTimeout;
    TExpirationTimeout GetIdExpirationTimeout(void) const
        {
            return m_IdExpirationTimeout;
        }
    void SetIdExpirationTimeout(TExpirationTimeout timeout);

    bool GetAlwaysLoadExternal(void) const
        {
            return m_AlwaysLoadExternal;
        }

    void SetAlwaysLoadExternal(bool flag)
        {
            m_AlwaysLoadExternal = flag;
        }

    bool GetAlwaysLoadNamedAcc(void) const
        {
            return m_AlwaysLoadNamedAcc;
        }

    void SetAlwaysLoadNamedAcc(bool flag)
        {
            m_AlwaysLoadNamedAcc = flag;
        }

    bool GetAddWGSMasterDescr(void) const
        {
            return m_AddWGSMasterDescr;
        }

    void SetAddWGSMasterDescr(bool flag)
        {
            m_AddWGSMasterDescr = flag;
        }

    bool HasHUPIncluded(void) const
        {
            return m_HasHUPIncluded;
        }

    
    EGBErrorAction GetPTISErrorAction(void) const
        {
            return m_PTISErrorAction;
        }

    void SetPTISErrorAction(EGBErrorAction action)
        {
            m_PTISErrorAction = action;
        }

    virtual CObjectManager::TPriority GetDefaultPriority(void) const override;
    virtual bool GetTrackSplitSeq() const override;

protected:
    template <class TDataLoader>
        class CGBLoaderMaker : public CLoaderMaker_Base
    {
    public:
        CGBLoaderMaker(const CGBLoaderParams& params)
            : m_Params(params)
            {
                m_Name = CGBDataLoader::GetLoaderNameFromArgs(params);
            }
        
    virtual ~CGBLoaderMaker(void) {}

    virtual CDataLoader* CreateLoader(void) const
        {
            return new TDataLoader(m_Name, m_Params);
        }
    typedef SRegisterLoaderInfo<CGBDataLoader> TRegisterInfo;
    TRegisterInfo GetRegisterInfo(void)
        {
            TRegisterInfo info;
            info.Set(m_RegisterInfo.GetLoader(), m_RegisterInfo.IsCreated());
            return info;
        }
    protected:
        CGBLoaderParams m_Params;
    };
    CGBDataLoader(const string&     loader_name,
                  const CGBLoaderParams& params);

    static string x_GetLoaderMethod(const TParamTree* params);

    TExpirationTimeout      m_IdExpirationTimeout;

    bool                    m_AlwaysLoadExternal;
    bool                    m_AlwaysLoadNamedAcc;
    bool                    m_AddWGSMasterDescr;
    bool                    m_HasHUPIncluded;
    EGBErrorAction          m_PTISErrorAction;
    string                  m_WebCookie;

private:
    CGBDataLoader(const CGBDataLoader&);
    CGBDataLoader& operator=(const CGBDataLoader&);

    virtual TRealBlobId x_GetRealBlobId(const TBlobId& blob_id) const = 0;
public:
    static bool IsUsingPSGLoader(void);
};


NCBI_PARAM_DECL(string, GENBANK, LOADER_METHOD);

END_SCOPE(objects)


extern "C"
{

NCBI_XLOADER_GENBANK_EXPORT
void NCBI_EntryPoint_DataLoader_GB(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_GENBANK_EXPORT
void NCBI_EntryPoint_xloader_genbank(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif
