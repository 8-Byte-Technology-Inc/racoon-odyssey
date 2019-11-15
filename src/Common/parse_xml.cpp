/*
	Copyright (C) 2019 8 Byte Technology Inc. - All Rights Reserved
*/
#include "pch.h"

#include "parse_xml.h"

namespace TB8
{

const u32 DEFAULT_BUFFER_SIZE = 1024;
const u32 BUFFER_EXTRA = 4;

XML_Parser::XML_Parser()
	: m_pfn_Read(nullptr)
	, m_pfn_Start(nullptr)
	, m_pfn_Data(nullptr)
	, m_pfn_End(nullptr)
	, m_bufferSize(DEFAULT_BUFFER_SIZE)
	, m_data()
	, m_posStart(0)
	, m_posEnd(0)
	, m_posCursor(0)
	, m_context(XML_Parser_Context_CData)
{
}

XML_Parser::~XML_Parser()
{
}

void XML_Parser::SetHandlers(pfn_Start pfnStart, pfn_Data pfnData, pfn_End pfnEnd)
{
	m_pfn_Start = pfnStart;
	m_pfn_Data = pfnData;
	m_pfn_End = pfnEnd;
}

void XML_Parser::AddData(const u8* pData, u32 dataSize)
{
	__CompactBuffer();
	__EnsureBuffer(dataSize);

	memcpy(m_data.data() + m_posEnd, pData, dataSize);
	m_posEnd += dataSize;
}

XML_Parser_Result XML_Parser::Parse()
{
	XML_Parser_Result result = XML_Parser_Result_Success;
	while ((result == XML_Parser_Result_Success)
		|| (m_posCursor < m_posEnd))
	{
		result = __ReadData();
		if ((result != XML_Parser_Result_Success)
			&& (result != XML_Parser_Result_EOF)
			&& (result != XML_Parser_Result_Pending))
			return result;

		XML_String data(&(m_data[m_posStart]), &(m_data[m_posCursor]));
		while (m_posCursor < m_posEnd)
		{
			const u32 cbData = data.size();
			switch (m_context)
			{
			case XML_Parser_Context_CData:
			{
				if ((cbData > 0)
					&& (*(data.back()) == '<'))
				{
					if (!__ParseCData(data))
						return XML_Parser_Result_Error;
					assert(*(data.front()) == '<');
					m_context = XML_Parser_Context_Tag;
				}
				else
				{
					__Accumulate(data);
				}
				break;
			}
			case XML_Parser_Context_Tag:
			{
				assert(*(data.front() + 0) == '<');
				if ((cbData >= 3)
					&& (*(data.front() + 1) == '!')
					&& (*(data.front() + 2) != '-'))
				{
					m_context = XML_Parser_Context_Decl;
				}
				else if ((cbData >= 4)
					&& (*(data.front() + 1) == '!')
					&& (*(data.front() + 2) == '-')
					&& (*(data.front() + 3) == '-'))
				{
					m_context = XML_Parser_Context_Comment;
				}
				else if ((cbData >= 5)
					&& (*(data.front() + 1) == '?')
					&& (*(data.front() + 2) == 'x')
					&& (*(data.front() + 3) == 'm')
					&& (*(data.front() + 4) == 'l'))
				{
					m_context = XML_Parser_Context_XMLDecl;
				}
				else if ((cbData >= 2)
					&& (*(data.front() + 1) != '!')
					&& (*(data.front() + 1) != '?'))
				{
					m_context = XML_Parser_Context_Element;
				}
				else if ((cbData >= 3)
					&& (*(data.back()) == '>'))
				{
					__Consume(data, cbData);
					m_context = XML_Parser_Context_CData;
				}
				else
				{
					__Accumulate(data);
				}
				break;
			}
			case XML_Parser_Context_XMLDecl:
			{
				if ((*(data.back() - 1) == '?')
					&& (*(data.back() - 0) == '>'))
				{
					__Consume(data, cbData);
					m_context = XML_Parser_Context_CData;
				}
				else
				{
					__Accumulate(data);
				}
				break;
			}
			case XML_Parser_Context_Comment:
			{
				if ( (*(data.back() - 2) == '-')
					&& (*(data.back() - 1) == '-')
					&& (*(data.back() - 0) == '>'))
				{
					__Consume(data, cbData);
					m_context = XML_Parser_Context_CData;
				}
				else
				{
					__Accumulate(data);
				}
				break;
			}
			case XML_Parser_Context_Decl:
			{
				if (*(data.back() - 0) == '>')
				{
					__Consume(data, cbData);
					m_context = XML_Parser_Context_CData;
				}
				else
				{
					__Accumulate(data);
				}
				break;
			}
			case XML_Parser_Context_Element:
			{
				if (*(data.back() - 0) == '>')
				{
					if (!__ParseElement(data))
						return XML_Parser_Result_Error;
					m_context = XML_Parser_Context_CData;
				}
				else
				{
					__Accumulate(data);
				}
				break;
			}
			default:
			{
				assert(!"unhandled context state");
				break;
			}
			}
		}
	}

	if (result == XML_Parser_Result_EOF)
	{
		if (m_context == XML_Parser_Context_CData)
		{
			XML_String data(&(m_data[m_posStart]), &(m_data[m_posCursor]));
			if (!__ParseCData(data))
				return XML_Parser_Result_Error;
		}

		if (m_posStart == m_posEnd)
			return XML_Parser_Result_Success;
	}

	return result;
}

void XML_Parser::__Consume(XML_String& data, u32 cb)
{
	m_posStart += cb;
	m_posCursor = m_posStart;
	data.m_pStart = &(m_data[m_posCursor]);
	data.m_pEnd = data.m_pStart;
}

void XML_Parser::__CompactBuffer()
{
	if (m_posStart == 0)
		return;

	if (m_posEnd > m_posStart)
	{
		const size_t cbMove = m_posEnd - m_posStart;
		memmove(&(m_data[0]), &(m_data[m_posStart]), cbMove);
	}

	m_posCursor -= m_posStart;
	m_posEnd -= m_posStart;
	m_posStart = 0;
}

void XML_Parser::__EnsureBuffer(u32 dataSize)
{
	const u32 cbNeeded = m_posEnd + dataSize;

	// is it already large enough?
	if (!m_data.empty()
		&& (cbNeeded <= m_bufferSize))
		return;

	// figure out how much bigger to make it, multiples of 2.
	while (m_bufferSize < cbNeeded)
		m_bufferSize *= 2;

	// resize the buffer.
	m_data.resize(m_bufferSize + BUFFER_EXTRA);
}

XML_Parser_Result XML_Parser::__ReadData()
{
	if (!m_pfn_Read)
		return XML_Parser_Result_Pending;

	// shuffle down.
	__CompactBuffer();

	// make sure we have enough space in the buffer.
	__EnsureBuffer(__GetBufferSize() / 2);

	// read some more data.
	u32 cbRead = __GetBufferSize() - m_posEnd;
	XML_Parser_Result r = m_pfn_Read(m_data.data() + m_posEnd, &cbRead);
	if (r == XML_Parser_Result_Success)
	{
		m_posEnd += cbRead;
	}
	return r;
}

bool XML_Parser::__ParseElement(XML_String& data)
{
	assert(*(data.front()) == '<');
	assert(*(data.back()) == '>');
	XML_String src = data;

	bool isClose = false;
	bool isOpenClose = false;

	src.increment_front(1);

	__SkipWhitespace(src);

	// close tag.
	if (*src.front() == '/')
	{
		isClose = true;
		src.increment_front(1);
	}

	__SkipWhitespace(src);

	// tag name.
	XML_String tagName;
	if (!__ParseName(src, tagName))
	{
		__ParseError("Parsing element, expected element name");
		return false;
	}

	__SkipWhitespace(src);

	// attributes.
	std::vector<XML_Attrib> attribs;
	while (true)
	{
		XML_Attrib attrib;

		// attrib name.
		if (!__ParseName(src, attrib.m_name))
			break;

		__SkipWhitespace(src);

		// eq.
		if (*src.front() != '=')
			break;
		src.increment_front(1);

		__SkipWhitespace(src);

		// attrib value.
		if (!__ParseAttrValue(src, attrib.m_value))
			break;

		__SkipWhitespace(src);

		attribs.push_back(attrib);
	}

	__SkipWhitespace(src);

	if (*src.front() == '/')
	{
		isOpenClose = true;
		src.increment_front(1);
	}

	__SkipWhitespace(src);

	if (*src.front() != '>')
	{
		__ParseError("Parsing element, expected closing tag >");
		return false;
	}

	tagName.Terminate();

	if (!isClose)
	{
		std::vector<const char*> attrib_ptr;
		attrib_ptr.reserve((attribs.size() + 1) * 2);

		// make a list of attrib pointers, add null terminators.
		for (std::vector<XML_Attrib>::iterator it = attribs.begin(); it != attribs.end(); ++it)
		{
			XML_Attrib& attrib = *it;
			attrib.m_name.Terminate();
			__DecodeString(attrib.m_value);
			attrib.m_value.Terminate();
			attrib_ptr.push_back(reinterpret_cast<const char*>(attrib.m_name.front()));
			attrib_ptr.push_back(reinterpret_cast<const char*>(attrib.m_value.front()));
		}

		attrib_ptr.push_back(nullptr);
		attrib_ptr.push_back(nullptr);

		m_pfn_Start(reinterpret_cast<const char*>(tagName.front()), attrib_ptr.data());
	}

	if (isClose || isOpenClose)
	{
		m_pfn_End(reinterpret_cast<const char*>(tagName.front()));
	}

	__Consume(data, data.size());
	return true;
}

bool XML_Parser::__ParseCData(XML_String& data)
{
	XML_String src = data;

	// if there is a tailing <, remove it.
	if (!src.empty() && (*src.back() == '<'))
		src.m_pEnd--;

	// is the entire data whitespace?
	XML_String srcTemp = src;
	__SkipWhitespace(srcTemp);
	if (srcTemp.empty())
	{
		__Consume(data, src.size());
		return true;
	}

	const u32 cbSrc = src.size();

	// decode the data.
	__DecodeString(src);

	// there should be a little extra in the buffer so we can temporarily add a null terminator.
	assert(static_cast<u32>((src.m_pEnd + 1) - m_data.data()) < static_cast<u32>(m_data.size()));
	const u8 temp = *src.m_pEnd;
	*src.m_pEnd = 0;
	m_pfn_Data(reinterpret_cast<const char*>(src.front()), src.size());
	*src.m_pEnd = temp;

	__Consume(data, cbSrc);
	return true;
}

bool XML_Parser::__IsNameStartChar(u32 val) const
{
	return (val == ':')
		|| ((val >= 'A') && (val <= 'Z'))
		|| (val == '_')
		|| ((val >= 'a') && (val <= 'z'))
		|| ((val >= 0xC0) && (val <= 0xD6))
		|| ((val >= 0xD8) && (val <= 0xF6))
		|| ((val >= 0xF8) && (val <= 0x2FF))
		|| ((val >= 0x370) && (val <= 0x37D))
		|| ((val >= 0x37F) && (val <= 0x1FFF))
		|| ((val >= 0x200C) && (val <= 0x200D))
		|| ((val >= 0x2070) && (val <= 0x218F))
		|| ((val >= 0x2C00) && (val <= 0x2FEF))
		|| ((val >= 0x3001) && (val <= 0xD7FF))
		|| ((val >= 0xF900) && (val <= 0xFDCF))
		|| ((val >= 0xFDF0) && (val <= 0xFFFD))
		|| ((val >= 0x10000) && (val <= 0xEFFFF));
}

bool XML_Parser::__IsNameChar(u32 val) const
{
	return __IsNameStartChar(val)
		|| (val == '-')
		|| (val == '.')
		|| ((val >= '0') && (val <= '9'))
		|| (val == 0xB7)
		|| ((val >= 0x0300) && (val <= 0x036F))
		|| ((val >= 0x203F) && (val <= 0x2040));
}

bool XML_Parser::__ParseName(XML_String& src, XML_String& name)
{
	__SkipWhitespace(src);

	u32 cbSize;
	u32 ch;
	if (!__GetChar(src, &cbSize, &ch)
		|| !__IsNameStartChar(ch))
		return false;

	name.m_pStart = src.m_pStart;
	src.increment_front(cbSize);

	while (__GetChar(src, &cbSize, &ch)
		&& __IsNameChar(ch))
	{
		src.increment_front(cbSize);
	}
		
	name.m_pEnd = src.m_pStart;
	return true;
}

bool XML_Parser::__ParseAttrValue(XML_String& src, XML_String& value)
{
	__SkipWhitespace(src);

	if (*src.front() != '"')
		return false;
	src.increment_front(1);
	value.m_pStart = src.m_pStart;

	while ((*src.front() != '"') && (*src.front() != '>'))
		__Next(src);

	if (*src.front() != '"')
		return false;

	value.m_pEnd = src.m_pStart;
	src.increment_front(1);

	__SkipWhitespace(src);
	return true;
}

void XML_Parser::__ParseError(const char* pszErrorReason)
{
	m_errorReason = pszErrorReason;
}

void XML_Parser::__SkipWhitespace(XML_String& src)
{
	// [3]   	S	   :: = (#x20 | #x9 | #xD | #xA) +
	while (!src.empty()
		&& __IsWhitespaceChar(*src.front()))
	{
		src.increment_front(1);
	}
}

bool XML_Parser::__GetCharSize(const XML_String& src, u32* pSize) const
{
	// how many encoded bytes do we expect?
	if (src.empty())
		*pSize = 0;
	else if ((*src.front() & 0x80) == 0)
		*pSize = 1;
	else if ((*src.front() & 0xe0) == 0xc0)
		*pSize = 2;
	else if ((*src.front() & 0xf0) == 0xe0)
		*pSize = 3;
	else if ((*src.front() & 0xf8) == 0xf0)
		*pSize = 4;

	// if any trailing bytes are invalid, treat it as regular bytes.
	const u32 cbCheckTrail = std::min<u32>(*pSize, src.size());
	for (u32 i = 1; i < cbCheckTrail; ++i)
	{
		if ((*(src.front() + i) & 0xc0) == 0x80)
			continue;
		*pSize = 1;
		break;
	}

	return (*pSize > 0) && (*pSize <= src.size());
}

bool XML_Parser::__Next(XML_String& src) const
{
	u32 cbEncoded;
	if (!__GetCharSize(src, &cbEncoded))
		return false;

	src.increment_front(cbEncoded);
	return true;
}

bool XML_Parser::__GetChar(XML_String& src, u32* pSize, u32* pch) const
{
	if (!__GetCharSize(src, pSize))
		return false;

	if (*pSize == 1)
	{
		*pch = *src.front();
	}
	else if (*pSize == 2)
	{
		*pch = (static_cast<u32>(*(src.front() + 0) & 0x1f) << 6)
			| (static_cast<u32>(*(src.front() + 1) & 0x3f) << 0);
	}
	else if (*pSize == 3)
	{
		*pch = (static_cast<u32>(*(src.front() + 0) & 0x0f) << 12)
			| (static_cast<u32>(*(src.front() + 1) & 0x3f) << 6)
			| (static_cast<u32>(*(src.front() + 2) & 0x3f) << 0);
	}
	else if (*pSize == 4)
	{
		*pch = (static_cast<u32>(*(src.front() + 0) & 0x07) << 18)
			| (static_cast<u32>(*(src.front() + 1) & 0x3f) << 12)
			| (static_cast<u32>(*(src.front() + 2) & 0x3f) << 6)
			| (static_cast<u32>(*(src.front() + 3) & 0x3f) << 0);
	}
	return true;
}

bool XML_Parser::__DecodeString(XML_String& s)
{
	XML_String src = s;
	u8* pDst = s.m_pStart;
	while (!src.empty())
	{
		if (*src.front() == '&')
		{
			// escape sequence !
			if (!__DecodeEsc(src, pDst))
				return false;
		}
		else
		{
			// copy character.
			u32 cb;
			if (!__GetCharSize(src, &cb))
				return false;
			if (pDst != src.m_pStart)
			{
				memmove(pDst, src.m_pStart, cb);
			}
			src.increment_front(cb);
			pDst += cb;
		}
	}

	s.m_pEnd = pDst;
	return true;
}

struct XML_Parser_EscSeq
{
	const char*		m_pszSeq;		// sequence
	u32				m_size;			// size of sequence
	u8				m_val;			// replacement value
};

static const XML_Parser_EscSeq s_escSeq[] =
{
	{ "&amp;", 5, '&' },
	{ "&lt;", 4, '<' },
	{ "&gt;", 4, '>' },
	{ "&quot;", 6, '\"' },
	{ "&apos;", 6, '\'' },
};

bool XML_Parser::__DecodeEsc(XML_String& src, u8*& pDst)
{
	assert(*src.front() == '&');
	const u32 size = src.size();

	// look thru fixed definitions.
	for (u32 i = 0; i < ARRAYSIZE(s_escSeq); ++i)
	{
		const XML_Parser_EscSeq& seq = s_escSeq[i];
		if (size < seq.m_size)
			continue;
		if (memcmp(src.front(), seq.m_pszSeq, seq.m_size) != 0)
			continue;
		*(pDst++) = seq.m_val;
		src.increment_front(seq.m_size);
		return true;
	}

	if ((size >= 5) && *(src.front() + 1) == '#' && *(src.front() + 2) == 'x')
	{
		src.increment_front(3);
		u32 val = 0;
		while (!src.empty() && __IsHexDigit(*src.front()))
		{
			const u8 ch = *src.front();
			val = val * 16;
			if (__IsDigit(ch))
				val += (ch - '0');
			else if (ch >= 'A' && ch <= 'F')
				val += (ch - 'A') + 10;
			else if (ch >= 'a' && ch <= 'f')
				val += (ch - 'a') + 10;
			src.increment_front(1);
		}
		*(pDst++) = static_cast<u8>(val);
	}
	else if ((size >= 4) && *(src.front() + 1) == '#')
	{
		src.increment_front(2);
		u32 val = 0;
		while (!src.empty() && __IsDigit(*src.front()))
		{
			const u8 ch = *src.front();
			val = val * 10;
			val += (ch - '0');
			src.increment_front(1);
		}
		*(pDst++) = static_cast<u8>(val);
	}
	else
	{
		src.increment_front(1);
		while (!src.empty() && *src.front() != ';')
			src.increment_front(1);
	}

	if (src.empty() || *src.front() != ';')
		return false;
	src.increment_front(1);
	return true;
}

}
