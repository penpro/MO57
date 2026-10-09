/**
 * =============================================================================
 * MOBugReportBundle.h - the bytes of a bug report, in the format the crash reporter already uploads
 * =============================================================================
 *
 * WHY THIS FORMAT: crash reports already reach the website: Unreal's CrashReportClient POSTs a zlib-compressed bundle ("CR1") to the data router URL in
 * [CrashReportClient] DataRouterUrl, and the site stores the body and the query parameters as they arrive. A bug report written in the SAME bundle goes
 * through the same endpoint, is stored in the same place and read by the same tooling (Tools/ue_crash_bundle.py, Tools/crash_triage.py) -- no website change.
 * What marks it as a bug report and not a crash: CrashContext.runtime-xml says <CrashType>BugReport</CrashType>, and the upload carries ReportKind=bugreport.
 *
 * LAYOUT (mirrors Engine/Source/Runtime/CrashReportCore/Private/CrashUpload.cpp, FCompressedHeader / FCompressedCrashFile), BEFORE zlib:
 *   'C' 'R' '1'
 *   DirectoryName   : int32 length (>= 260), ANSI chars, zero padded          ("UECC-Windows-<32 hex>_0000")
 *   FileName        : same encoding                                           ("<DirectoryName>.uecrash")
 *   int32 UncompressedSize (of this whole stream), int32 FileCount
 *   per file        : int32 index, file name (260-char ANSI array), int32 byte count, bytes
 * The same engine routines (FString::SerializeAsANSICharArray, FCompression NAME_Zlib) write it, so the layout cannot drift from the real one.
 *
 * Everything here is pure (no engine state): unit-tested in MOFrameworkTests.cpp, and Parse() is what the tests, and any in-game viewer, read a bundle back with.
 */

#pragma once

#include "CoreMinimal.h"

namespace MOBugReportBundle
{
	/** One file inside a bundle. */
	struct FFile
	{
		FString Name;
		TArray<uint8> Data;
	};

	/** The longest ANSI name field the format carries (the crash reporter's own limit). */
	constexpr int32 NameFieldSize = 260;

	/**
	 * Write `Files` as a zlib-compressed CR1 bundle. Fails (OutError says why) on an empty file list or a name that does not fit the 260-char field.
	 * `OutUncompressedSize` (optional) receives the size of the stream before compression -- the number Parse needs.
	 */
	MOFRAMEWORK_API bool Build(const FString& DirectoryName, const TArray<FFile>& Files, TArray<uint8>& OutCompressed, FString& OutError, int32* OutUncompressedSize = nullptr);

	/**
	 * Read a bundle back (Build's inverse; for tests and tools, never the gameplay path). `UncompressedSize` is what Build reported: the engine's zlib
	 * wrapper inflates into a buffer of exactly that size and refuses anything else (the website tooling, Python's zlib, does not need it). Fails on a
	 * bad zlib stream, a size mismatch, a bad marker or truncated data; the header's own size field must agree with `UncompressedSize`.
	 */
	MOFRAMEWORK_API bool Parse(const TArray<uint8>& Compressed, int32 UncompressedSize, FString& OutDirectoryName, TArray<FFile>& OutFiles, FString& OutError);

	/** The crash reporter's directory-name shape for a report id: "UECC-Windows-<32 hex digits, upper case>_0000". */
	MOFRAMEWORK_API FString MakeDirectoryName(const FGuid& ReportId);

	/** XML text escaping for element content (& < > " '), and removal of characters XML 1.0 cannot carry. */
	MOFRAMEWORK_API FString XmlEscape(const FString& Text);

	/**
	 * A CrashContext.runtime-xml for a bug report: the properties the crash tooling expects (CrashVersion, CrashGUID, CrashType=BugReport, ErrorMessage,
	 * UserDescription, GameName, EngineVersion, ...) in <RuntimeProperties>, and the game's own state as <Field name="..">..</Field> rows in <GameData>.
	 * `RuntimeProperties` and `GameFields` are (name, value) pairs in the order they should appear; values are escaped here.
	 */
	MOFRAMEWORK_API FString BuildContextXml(const TArray<TPair<FString, FString>>& RuntimeProperties, const TArray<TPair<FString, FString>>& GameFields);

	/**
	 * Remove what identifies the person from text that is about to leave the machine: the OS user name and computer name (whole-word, any case) and the
	 * user-profile folder in a path (C:\Users\<name>\ -> C:\Users\<user>\). Names shorter than 3 characters are left alone (they would shred ordinary words).
	 */
	MOFRAMEWORK_API FString Sanitize(const FString& Text, const FString& UserName, const FString& ComputerName);

	/** `Text` cut to at most MaxChars characters (an ellipsis marks the cut). */
	MOFRAMEWORK_API FString ClampText(const FString& Text, int32 MaxChars);

	/**
	 * The last ~MaxChars characters of a log, starting on a line boundary (a half line at the cut would read as garbage) and headed by a marker line
	 * when anything was dropped. Text that already fits is returned unchanged.
	 */
	MOFRAMEWORK_API FString TailText(const FString& Text, int32 MaxChars);
}
