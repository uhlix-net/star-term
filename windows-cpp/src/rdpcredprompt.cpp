#include "rdpcredprompt.h"

#include <windows.h>
#include <unknwn.h>

// The RDP client's non-scriptable interfaces are not in the Windows SDK; they
// exist only in the type library embedded in mstscax.dll.  MSVC's #import can
// generate them, but it writes its .tlh next to the object file, and that write
// fails when the build tree sits on the WSL share ("Incorrect function").  So
// the chain is declared here instead, copied verbatim from the .tlh that
// #import produced from the installed DLL.
//
// Only put_AllowPromptingForCredentials is ever called.  Everything above it is
// present because a COM interface is a vtable: each inherited method has to
// occupy its slot for the one we want to land at the right offset.  Nothing
// here may be reordered, and nothing may be left out.
namespace {

// Placeholders for types the declarations mention but this file never touches.
// Only their size matters, and every one of them is a pointer or an int.
struct IMsRdpDeviceCollection;
struct IMsRdpDriveCollection;
struct _RemotableHandle;
typedef _RemotableHandle *wireHWND;
typedef int RedirectionWarningType;

struct __declspec(uuid("c1e6743a-41c1-4a74-832a-0dd06c1c7a0e"))
IMsTscNonScriptable : IUnknown
{
    virtual HRESULT __stdcall put_ClearTextPassword(BSTR) = 0;
    virtual HRESULT __stdcall put_PortablePassword(BSTR) = 0;
    virtual HRESULT __stdcall get_PortablePassword(BSTR *) = 0;
    virtual HRESULT __stdcall put_PortableSalt(BSTR) = 0;
    virtual HRESULT __stdcall get_PortableSalt(BSTR *) = 0;
    virtual HRESULT __stdcall put_BinaryPassword(BSTR) = 0;
    virtual HRESULT __stdcall get_BinaryPassword(BSTR *) = 0;
    virtual HRESULT __stdcall put_BinarySalt(BSTR) = 0;
    virtual HRESULT __stdcall get_BinarySalt(BSTR *) = 0;
    virtual HRESULT __stdcall ResetPassword() = 0;
};

struct __declspec(uuid("2f079c4c-87b2-4afd-97ab-20cdb43038ae"))
IMsRdpClientNonScriptable : IMsTscNonScriptable
{
    virtual HRESULT __stdcall NotifyRedirectDeviceChange(UINT_PTR, LONG_PTR) = 0;
    virtual HRESULT __stdcall SendKeys(long, VARIANT_BOOL *, long *) = 0;
};

struct __declspec(uuid("17a5e535-4072-4fa4-af32-c8d0d47345e9"))
IMsRdpClientNonScriptable2 : IMsRdpClientNonScriptable
{
    virtual HRESULT __stdcall put_UIParentWindowHandle(wireHWND) = 0;
    virtual HRESULT __stdcall get_UIParentWindowHandle(wireHWND *) = 0;
};

struct __declspec(uuid("b3378d90-0728-45c7-8ed7-b6159fb92219"))
IMsRdpClientNonScriptable3 : IMsRdpClientNonScriptable2
{
    virtual HRESULT __stdcall put_ShowRedirectionWarningDialog(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_ShowRedirectionWarningDialog(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_PromptForCredentials(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_PromptForCredentials(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_NegotiateSecurityLayer(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_NegotiateSecurityLayer(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_EnableCredSspSupport(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_EnableCredSspSupport(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_RedirectDynamicDrives(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_RedirectDynamicDrives(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_RedirectDynamicDevices(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_RedirectDynamicDevices(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall get_DeviceCollection(IMsRdpDeviceCollection **) = 0;
    virtual HRESULT __stdcall get_DriveCollection(IMsRdpDriveCollection **) = 0;
    virtual HRESULT __stdcall put_WarnAboutSendingCredentials(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_WarnAboutSendingCredentials(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_WarnAboutClipboardRedirection(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_WarnAboutClipboardRedirection(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_ConnectionBarText(BSTR) = 0;
    virtual HRESULT __stdcall get_ConnectionBarText(BSTR *) = 0;
};

struct __declspec(uuid("f50fa8aa-1c7d-4f59-b15c-a90cacae1fcb"))
IMsRdpClientNonScriptable4 : IMsRdpClientNonScriptable3
{
    virtual HRESULT __stdcall put_RedirectionWarningType(RedirectionWarningType) = 0;
    virtual HRESULT __stdcall get_RedirectionWarningType(RedirectionWarningType *) = 0;
    virtual HRESULT __stdcall put_MarkRdpSettingsSecure(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_MarkRdpSettingsSecure(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_PublisherCertificateChain(VARIANT *) = 0;
    virtual HRESULT __stdcall get_PublisherCertificateChain(VARIANT *) = 0;
    virtual HRESULT __stdcall put_WarnAboutPrinterRedirection(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_WarnAboutPrinterRedirection(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_AllowCredentialSaving(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_AllowCredentialSaving(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_PromptForCredsOnClient(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_PromptForCredsOnClient(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_LaunchedViaClientShellInterface(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_LaunchedViaClientShellInterface(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_TrustedZoneSite(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_TrustedZoneSite(VARIANT_BOOL *) = 0;
};

struct __declspec(uuid("4f6996d5-d7b1-412c-b0ff-063718566907"))
IMsRdpClientNonScriptable5 : IMsRdpClientNonScriptable4
{
    virtual HRESULT __stdcall put_UseMultimon(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_UseMultimon(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall get_RemoteMonitorCount(unsigned long *) = 0;
    virtual HRESULT __stdcall GetRemoteMonitorsBoundingBox(long *, long *, long *, long *) = 0;
    virtual HRESULT __stdcall get_RemoteMonitorLayoutMatchesLocal(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_DisableConnectionBar(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall put_DisableRemoteAppCapsCheck(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_DisableRemoteAppCapsCheck(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_WarnAboutDirectXRedirection(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_WarnAboutDirectXRedirection(VARIANT_BOOL *) = 0;
    virtual HRESULT __stdcall put_AllowPromptingForCredentials(VARIANT_BOOL) = 0;
    virtual HRESULT __stdcall get_AllowPromptingForCredentials(VARIANT_BOOL *) = 0;
};

} // namespace

bool rdpDisableCredentialPrompt(IUnknown *control)
{
    if (!control) return false;

    IMsRdpClientNonScriptable5 *ns = nullptr;
    if (FAILED(control->QueryInterface(__uuidof(IMsRdpClientNonScriptable5),
                                       reinterpret_cast<void **>(&ns))) ||
        !ns)
        return false;

    const bool ok = SUCCEEDED(ns->put_AllowPromptingForCredentials(VARIANT_FALSE));
    ns->Release();
    return ok;
}
