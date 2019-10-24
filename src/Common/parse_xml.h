#pragma once

#include <functional>
#include <vector>

#include "common/basic_types.h"

namespace TB8
{

enum XML_Parser_Result : u32
{
	XML_Parser_Result_Success,
	XML_Parser_Result_EOF,
	XML_Parser_Result_Error,
	XML_Parser_Result_NeedMore,
	XML_Parser_Result_Pending,
};

enum XML_Parser_Context : u32
{
	XML_Parser_Context_CData,
	XML_Parser_Context_Tag,
	XML_Parser_Context_XMLDecl,
	XML_Parser_Context_Comment,
	XML_Parser_Context_Decl,
	XML_Parser_Context_Element,
};

struct XML_String
{
	u8*	m_pStart;
	u8*	m_pEnd;

	u32 Size() const { return static_cast<u32>(m_pEnd - m_pStart); }
	void Terminate() { *m_pEnd = 0; }
};

struct XML_Attrib
{
	XML_String m_name;
	XML_String m_value;
};

using pfn_Read = std::function<XML_Parser_Result(u8* buffer, u32* pSize)>;
using pfn_Start = std::function<void(const u8* name, const u8** attrs)>;
using pfn_Data = std::function<void(const u8* data, u32 len)>;
using pfn_End = std::function<void(const u8* name)>;

class XML_Parser
{
public:
	XML_Parser();
	~XML_Parser();

	void SetBufferSize(u32 bufferSize) { m_bufferSize = bufferSize; }
	void SetReader(pfn_Read pfnRead) { m_pfn_Read = pfnRead; }
	void SetHandlers(pfn_Start pfnStart, pfn_Data pfnData, pfn_End pfnEnd);

	void AddData(const u8* pData, u32 dataSize);
	XML_Parser_Result Parse();

protected:
	XML_Parser_Result __ReadData();
	bool __ParseElement(XML_String& data);
	bool __ParseCData(XML_String& data);
	bool __ParseName(u8*& pSrc, XML_String& name);
	bool __ParseAttrValue(u8*& pSrc, XML_String& value);
	bool __DecodeString(XML_String& str);
	bool __DecodeEsc(u8*& pSrc, u32 size, u8*& pDst);
	bool __IsWhitespace(u32 val) { return val == 0x20 || val == 0x9 || val == 0xd || val == 0xa; }
	bool __IsDigit(u8 val) { return (val >= '0' && val <= '9'); }
	bool __IsHexDigit(u8 val) { return __IsDigit(val) || (val >= 'A' && val <= 'F') || (val >= 'a' && val <= 'f'); }
	bool __IsNameStartChar(u32 ch) const;
	bool __IsNameChar(u32 ch) const;
	u32 __GetChar(const u8* pSrc) const;
	u32 __Next(u8*& pSrc) const;
	void __CompactBuffer();
	u32 __GetBufferSize() const { return m_bufferSize; }
	void __EnsureBuffer(u32 size);
	void __Consume(XML_String& data, u32 cb);
	void __ParseError(const char* pszErrorReason);

	pfn_Read			m_pfn_Read;
	pfn_Start			m_pfn_Start;
	pfn_Data			m_pfn_Data;
	pfn_End				m_pfn_End;

	u32					m_bufferSize;

	std::vector<u8>		m_data;
	u32					m_posStart;
	u32					m_posEnd;
	u32					m_posCursor;
	XML_Parser_Context	m_context;

	std::string			m_errorReason;
};

}
