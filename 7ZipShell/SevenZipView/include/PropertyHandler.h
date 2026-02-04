#pragma once
/*
** SevenZipView - Windows Explorer Shell Extension
** Property Handler
*/

#ifndef SEVENZIPVIEW_PROPERTYHANDLER_H
#define SEVENZIPVIEW_PROPERTYHANDLER_H

#include "Common.h"
#include "Archive.h"

namespace SevenZipView {

// Custom property keys for 7z archives
extern const PROPERTYKEY PKEY_7z_FileCount;
extern const PROPERTYKEY PKEY_7z_FolderCount;
extern const PROPERTYKEY PKEY_7z_CompressionRatio;
extern const PROPERTYKEY PKEY_7z_IsEncrypted;
extern const PROPERTYKEY PKEY_7z_Method;

// Property handler for archive files
class PropertyHandler :
    public IPropertyStore,
    public IPropertyStoreCapabilities,
    public IInitializeWithFile {
public:
    PropertyHandler();
    virtual ~PropertyHandler();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IPropertyStore
    STDMETHODIMP GetCount(DWORD* cProps) override;
    STDMETHODIMP GetAt(DWORD iProp, PROPERTYKEY* pkey) override;
    STDMETHODIMP GetValue(REFPROPERTYKEY key, PROPVARIANT* pv) override;
    STDMETHODIMP SetValue(REFPROPERTYKEY key, REFPROPVARIANT propvar) override;
    STDMETHODIMP Commit() override;

    // IPropertyStoreCapabilities
    STDMETHODIMP IsPropertyWritable(REFPROPERTYKEY key) override;

    // IInitializeWithFile
    STDMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override;

private:
    LONG _RefCount;
    std::wstring _ArchivePath;
    std::shared_ptr<Archive> _Archive;
    bool _Loaded;
    
    // Cached property values
    UINT32 _FileCount;
    UINT32 _FolderCount;
    UINT64 _TotalSize;
    UINT64 _CompressedSize;
    bool _IsEncrypted;
    std::wstring _Method;
};

// Property handler for items inside archive (displayed in Details pane)
class ItemPropertyHandler : public IPropertyStore {
public:
    ItemPropertyHandler();
    virtual ~ItemPropertyHandler();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IPropertyStore
    STDMETHODIMP GetCount(DWORD* cProps) override;
    STDMETHODIMP GetAt(DWORD iProp, PROPERTYKEY* pkey) override;
    STDMETHODIMP GetValue(REFPROPERTYKEY key, PROPVARIANT* pv) override;
    STDMETHODIMP SetValue(REFPROPERTYKEY key, REFPROPVARIANT propvar) override;
    STDMETHODIMP Commit() override;

    // Initialize with entry data
    void SetEntry(const ArchiveEntry& entry) { _Entry = entry; }

private:
    LONG _RefCount;
    ArchiveEntry _Entry;
};

} // namespace SevenZipView

#endif // SEVENZIPVIEW_PROPERTYHANDLER_H
