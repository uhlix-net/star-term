#pragma once

struct IUnknown;

// Turns off the RDP ActiveX control's built-in credential dialog, so a refused
// logon comes back to us as a disconnect instead of raising the Windows
// credential prompt.
//
// The switch lives on IMsRdpClientNonScriptable5, which derives from IUnknown
// rather than IDispatch.  ActiveQt only exposes the control's default dispatch
// interface, so setProperty("AllowPromptingForCredentials", ...) never reaches
// it — it just becomes a dynamic property on the QAxWidget.  Hence the direct
// COM call, kept in its own translation unit so the #import of mstscax.dll does
// not have to coexist with the Qt headers.
//
// `control` is the control's IUnknown, borrowed: the caller keeps ownership.
// Returns false if the control does not offer the interface.
bool rdpDisableCredentialPrompt(IUnknown *control);
