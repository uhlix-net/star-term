#include "rdpcredprompt.h"

#include <windows.h>
#include <comdef.h>

// Declarations come from the type library inside the installed mstscax.dll, so
// they always match the control this machine actually has.  4192 is the
// "automatically excluding while importing" note for the types #import skips.
#pragma warning(push)
#pragma warning(disable : 4192)
#import "mstscax.dll" rename_namespace("MSTSCLibNS"), raw_interfaces_only, raw_native_types, named_guids
#pragma warning(pop)

bool rdpDisableCredentialPrompt(IUnknown *control)
{
    if (!control) return false;

    MSTSCLibNS::IMsRdpClientNonScriptable5 *ns = nullptr;
    if (FAILED(control->QueryInterface(MSTSCLibNS::IID_IMsRdpClientNonScriptable5,
                                       reinterpret_cast<void **>(&ns))) ||
        !ns)
        return false;

    const bool ok = SUCCEEDED(ns->put_AllowPromptingForCredentials(VARIANT_FALSE));
    ns->Release();
    return ok;
}
