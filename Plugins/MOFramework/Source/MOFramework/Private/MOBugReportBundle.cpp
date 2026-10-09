#include "MOBugReportBundle.h"
#include "Misc/Compression.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace MOBugReportBundle
{
	namespace
	{
		/** Size of the header before the first file: marker + two 260-char name fields (each with its int32 length) + two int32. */
		constexpr int32 HeaderSize = 3 + (4 + NameFieldSize) + (4 + NameFieldSize) + 4 + 4;
		constexpr int32 UncompressedSizeOffset = 3 + (4 + NameFieldSize) + (4 + NameFieldSize);

		/** Read an ANSI field written by FString::SerializeAsANSICharArray: int32 length, then that many bytes (zero padded). */
		bool ReadAnsiField(FMemoryReader& Reader, FString& Out)
		{
			int32 Length = 0;
			Reader << Length;
			if (Reader.IsError() || Length < 0 || Length > 4096 || Reader.Tell() + Length > Reader.TotalSize())
			{
				return false;
			}
			TArray<ANSICHAR> Chars;
			Chars.SetNumUninitialized(Length + 1);
			if (Length > 0)
			{
				Reader.Serialize(Chars.GetData(), Length);
			}
			Chars[Length] = 0;
			Out = FString(ANSI_TO_TCHAR(Chars.GetData())); // up to the first NUL: the padding is dropped
			return true;
		}
	}

	bool Build(const FString& DirectoryName, const TArray<FFile>& Files, TArray<uint8>& OutCompressed, FString& OutError, int32* OutUncompressedSize)
	{
		OutCompressed.Reset();
		if (Files.Num() == 0)
		{
			OutError = TEXT("a bundle needs at least one file");
			return false;
		}
		if (DirectoryName.Len() >= NameFieldSize)
		{
			OutError = TEXT("the directory name does not fit the 260-character field");
			return false;
		}
		for (const FFile& File : Files)
		{
			if (File.Name.IsEmpty() || File.Name.Len() >= NameFieldSize)
			{
				OutError = FString::Printf(TEXT("file name '%s' is empty or does not fit the 260-character field"), *File.Name);
				return false;
			}
		}

		TArray<uint8> Uncompressed;
		Uncompressed.Reserve(HeaderSize + 1024);
		FMemoryWriter Writer(Uncompressed, /*bIsPersistent=*/false, /*bSetOffset=*/true);

		const uint8 Marker[3] = { 'C', 'R', '1' };
		Writer.Serialize(const_cast<uint8*>(Marker), sizeof(Marker));
		DirectoryName.SerializeAsANSICharArray(Writer, NameFieldSize);
		(DirectoryName + TEXT(".uecrash")).SerializeAsANSICharArray(Writer, NameFieldSize);
		int32 UncompressedSize = 0; // patched below, once the stream is complete (the real crash reporter writes the header last, too)
		int32 FileCount = Files.Num();
		Writer << UncompressedSize;
		Writer << FileCount;

		int32 Index = 0;
		for (const FFile& File : Files)
		{
			Writer << Index;
			File.Name.SerializeAsANSICharArray(Writer, NameFieldSize);
			TArray<uint8> Bytes = File.Data;
			Writer << Bytes;
			++Index;
		}

		UncompressedSize = Uncompressed.Num();
		FMemory::Memcpy(Uncompressed.GetData() + UncompressedSizeOffset, &UncompressedSize, sizeof(int32));
		if (OutUncompressedSize)
		{
			*OutUncompressedSize = UncompressedSize;
		}

		int32 CompressedSize = FCompression::CompressMemoryBound(NAME_Zlib, UncompressedSize);
		OutCompressed.SetNumUninitialized(CompressedSize);
		if (!FCompression::CompressMemory(NAME_Zlib, OutCompressed.GetData(), CompressedSize, Uncompressed.GetData(), UncompressedSize))
		{
			OutCompressed.Reset();
			OutError = TEXT("zlib compression failed");
			return false;
		}
		OutCompressed.SetNum(CompressedSize, EAllowShrinking::Yes);
		return true;
	}

	bool Parse(const TArray<uint8>& Compressed, int32 UncompressedSize, FString& OutDirectoryName, TArray<FFile>& OutFiles, FString& OutError)
	{
		OutDirectoryName.Reset();
		OutFiles.Reset();
		if (Compressed.Num() < 8 || UncompressedSize < HeaderSize)
		{
			OutError = TEXT("too short to be a bundle");
			return false;
		}
		if (UncompressedSize > (256 << 20))
		{
			OutError = TEXT("larger than any bug report bundle");
			return false;
		}

		TArray<uint8> Uncompressed;
		Uncompressed.SetNumUninitialized(UncompressedSize);
		if (!FCompression::UncompressMemory(NAME_Zlib, Uncompressed.GetData(), UncompressedSize, Compressed.GetData(), Compressed.Num()))
		{
			OutError = TEXT("not a zlib stream of that size");
			return false;
		}
		if (Uncompressed[0] != 'C' || Uncompressed[1] != 'R' || Uncompressed[2] != '1')
		{
			OutError = TEXT("missing the CR1 marker");
			return false;
		}
		int32 StoredSize = 0;
		FMemory::Memcpy(&StoredSize, Uncompressed.GetData() + UncompressedSizeOffset, sizeof(int32));
		if (StoredSize != UncompressedSize)
		{
			OutError = TEXT("the header's uncompressed size disagrees with the stream");
			return false;
		}

		FMemoryReader Reader(Uncompressed);
		Reader.Seek(3);
		FString FileName;
		int32 FileCount = 0;
		if (!ReadAnsiField(Reader, OutDirectoryName) || !ReadAnsiField(Reader, FileName))
		{
			OutError = TEXT("truncated header");
			return false;
		}
		Reader.Seek(UncompressedSizeOffset + 4);
		Reader << FileCount;
		if (FileCount < 0 || FileCount > 1024)
		{
			OutError = TEXT("the file count is not plausible");
			return false;
		}
		for (int32 FileIndex = 0; FileIndex < FileCount; ++FileIndex)
		{
			int32 StoredIndex = 0;
			FFile File;
			Reader << StoredIndex;
			if (!ReadAnsiField(Reader, File.Name))
			{
				OutError = FString::Printf(TEXT("truncated name of file %d"), FileIndex);
				return false;
			}
			int32 Count = 0;
			Reader << Count;
			if (Reader.IsError() || Count < 0 || Reader.Tell() + Count > Reader.TotalSize())
			{
				OutError = FString::Printf(TEXT("truncated data of file '%s'"), *File.Name);
				return false;
			}
			File.Data.SetNumUninitialized(Count);
			if (Count > 0)
			{
				Reader.Serialize(File.Data.GetData(), Count);
			}
			OutFiles.Add(MoveTemp(File));
		}
		return true;
	}

	FString MakeDirectoryName(const FGuid& ReportId)
	{
		return FString::Printf(TEXT("UECC-Windows-%s_0000"), *ReportId.ToString(EGuidFormats::Digits).ToUpper());
	}

	FString XmlEscape(const FString& Text)
	{
		FString Out;
		Out.Reserve(Text.Len() + 16);
		for (const TCHAR Character : Text)
		{
			switch (Character)
			{
			case TEXT('&'): Out += TEXT("&amp;"); break;
			case TEXT('<'): Out += TEXT("&lt;"); break;
			case TEXT('>'): Out += TEXT("&gt;"); break;
			case TEXT('"'): Out += TEXT("&quot;"); break;
			case TEXT('\''): Out += TEXT("&apos;"); break;
			default:
				// XML 1.0 allows tab, LF, CR and everything from U+0020; a stray control character would make the whole document unreadable.
				if (Character == TEXT('\t') || Character == TEXT('\n') || Character == TEXT('\r') || Character >= 0x20)
				{
					Out.AppendChar(Character);
				}
				break;
			}
		}
		return Out;
	}

	FString BuildContextXml(const TArray<TPair<FString, FString>>& RuntimeProperties, const TArray<TPair<FString, FString>>& GameFields)
	{
		FString Xml = TEXT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<FGenericCrashContext>\n\t<RuntimeProperties>\n");
		for (const TPair<FString, FString>& Property : RuntimeProperties)
		{
			Xml += FString::Printf(TEXT("\t\t<%s>%s</%s>\n"), *Property.Key, *XmlEscape(Property.Value), *Property.Key);
		}
		Xml += TEXT("\t</RuntimeProperties>\n\t<PlatformProperties />\n\t<EngineData />\n\t<GameData>\n");
		for (const TPair<FString, FString>& Field : GameFields)
		{
			Xml += FString::Printf(TEXT("\t\t<Field name=\"%s\">%s</Field>\n"), *XmlEscape(Field.Key), *XmlEscape(Field.Value));
		}
		Xml += TEXT("\t</GameData>\n</FGenericCrashContext>\n");
		return Xml;
	}

	namespace
	{
		bool IsWordCharacter(TCHAR Character)
		{
			return FChar::IsAlnum(Character) || Character == TEXT('_');
		}

		/** Replace whole-word, case-insensitive occurrences of `Word` in `Text` with `Replacement`. */
		FString ReplaceWholeWord(const FString& Text, const FString& Word, const FString& Replacement)
		{
			if (Word.Len() < 3)
			{
				return Text;
			}
			FString Out;
			Out.Reserve(Text.Len());
			int32 Cursor = 0;
			while (Cursor < Text.Len())
			{
				const int32 Found = Text.Find(Word, ESearchCase::IgnoreCase, ESearchDir::FromStart, Cursor);
				if (Found == INDEX_NONE)
				{
					break;
				}
				const int32 End = Found + Word.Len();
				const bool bStartOk = Found == 0 || !IsWordCharacter(Text[Found - 1]);
				const bool bEndOk = End >= Text.Len() || !IsWordCharacter(Text[End]);
				Out += Text.Mid(Cursor, Found - Cursor);
				Out += (bStartOk && bEndOk) ? Replacement : Text.Mid(Found, Word.Len());
				Cursor = End;
			}
			Out += Text.Mid(Cursor);
			return Out;
		}
	}

	FString Sanitize(const FString& Text, const FString& UserName, const FString& ComputerName)
	{
		FString Out = Text;

		// The profile folder in a path, whatever the name: C:\Users\Someone\... and /Users/someone/...
		for (const TCHAR* Prefix : { TEXT("\\Users\\"), TEXT("/Users/"), TEXT("\\users\\") })
		{
			int32 SearchFrom = 0;
			const FString PrefixString(Prefix);
			while (true)
			{
				const int32 At = Out.Find(PrefixString, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchFrom);
				if (At == INDEX_NONE)
				{
					break;
				}
				const int32 NameStart = At + PrefixString.Len();
				int32 NameEnd = NameStart;
				while (NameEnd < Out.Len() && Out[NameEnd] != TEXT('\\') && Out[NameEnd] != TEXT('/') && Out[NameEnd] != TEXT('"') && Out[NameEnd] != TEXT('\'') && !FChar::IsWhitespace(Out[NameEnd]))
				{
					++NameEnd;
				}
				const FString Name = Out.Mid(NameStart, NameEnd - NameStart);
				if (Name.Len() > 0 && !Name.Equals(TEXT("<user>")) && !Name.Equals(TEXT("Public"), ESearchCase::IgnoreCase) && !Name.Equals(TEXT("Default"), ESearchCase::IgnoreCase))
				{
					Out = Out.Left(NameStart) + TEXT("<user>") + Out.Mid(NameEnd);
					SearchFrom = NameStart + 6;
				}
				else
				{
					SearchFrom = NameEnd;
				}
			}
		}

		Out = ReplaceWholeWord(Out, UserName, TEXT("<user>"));
		Out = ReplaceWholeWord(Out, ComputerName, TEXT("<computer>"));
		return Out;
	}

	FString ClampText(const FString& Text, int32 MaxChars)
	{
		if (MaxChars <= 0 || Text.Len() <= MaxChars)
		{
			return Text;
		}
		return Text.Left(FMath::Max(0, MaxChars - 3)) + TEXT("...");
	}

	FString TailText(const FString& Text, int32 MaxChars)
	{
		if (MaxChars <= 0 || Text.Len() <= MaxChars)
		{
			return Text;
		}
		const int32 CutAt = Text.Len() - MaxChars;
		FString Tail = Text.Mid(CutAt);
		if (Text[CutAt - 1] != TEXT('\n'))
		{
			// The cut landed inside a line: drop the rest of that line.
			int32 Newline = INDEX_NONE;
			if (Tail.FindChar(TEXT('\n'), Newline))
			{
				Tail = Tail.Mid(Newline + 1);
			}
		}
		return TEXT("[... earlier log omitted ...]\n") + Tail;
	}
}
