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

		XML_String data;
		data.m_pStart = &(m_data[m_posStart]);
		data.m_pEnd = data.m_pStart + (m_posCursor - m_posStart);

		while (m_posCursor < m_posEnd)
		{
			switch (m_context)
			{
			case XML_Parser_Context_CData:
			{
				if (*(data.m_pEnd) == '<')
				{
					if (!__ParseCData(data))
						return XML_Parser_Result_Error;
					assert(*(data.m_pStart) == '<');
					m_context = XML_Parser_Context_Tag;
				}
				else
				{
					m_posCursor += __Next(data.m_pEnd);
				}
				break;
			}
			case XML_Parser_Context_Tag:
			{
				const u32 cb = m_posCursor - m_posStart;
				assert(*(data.m_pStart + 0) == '<');
				if ((cb >= 3)
					&& (*(data.m_pStart + 1) == '!')
					&& (*(data.m_pStart + 2) != '-'))
				{
					m_context = XML_Parser_Context_Decl;
				}
				else if ((cb >= 4)
					&& (*(data.m_pStart + 1) == '!')
					&& (*(data.m_pStart + 2) == '-')
					&& (*(data.m_pStart + 3) == '-'))
				{
					m_context = XML_Parser_Context_Comment;
				}
				else if ((cb >= 5)
					&& (*(data.m_pStart + 1) == '?')
					&& (*(data.m_pStart + 2) == 'x')
					&& (*(data.m_pStart + 3) == 'm')
					&& (*(data.m_pStart + 4) == 'l'))
				{
					m_context = XML_Parser_Context_XMLDecl;
				}
				else if ((cb >= 2)
					&& (*(data.m_pStart + 1) != '!')
					&& (*(data.m_pStart + 1) != '?'))
				{
					m_context = XML_Parser_Context_Element;
				}
				else if ((cb >= 3)
					&& (*(data.m_pEnd - 0) == '>'))
				{
					__Consume(data, cb + 1);
					m_context = XML_Parser_Context_CData;
				}
				else
				{
					m_posCursor += __Next(data.m_pEnd);
				}
				break;
			}
			case XML_Parser_Context_XMLDecl:
			{
				if ((*(data.m_pEnd - 1) == '?')
					&& (*(data.m_pEnd - 0) == '>'))
				{
					const u32 cb = m_posCursor - m_posStart;
					__Consume(data, cb + 1);
					m_context = XML_Parser_Context_CData;
				}
				else
				{
					m_posCursor += __Next(data.m_pEnd);
				}
				break;
			}
			case XML_Parser_Context_Comment:
			{
				if ( (*(data.m_pEnd - 2) == '-')
					&& (*(data.m_pEnd - 1) == '-')
					&& (*(data.m_pEnd - 0) == '>'))
				{
					const u32 cb = m_posCursor - m_posStart;
					__Consume(data, cb + 1);
					m_context = XML_Parser_Context_CData;
				}
				else
				{
					m_posCursor += __Next(data.m_pEnd);
				}
				break;
			}
			case XML_Parser_Context_Decl:
			{
				if ((*(data.m_pEnd - 0) == '>'))
				{
					const u32 cb = m_posCursor - m_posStart;
					__Consume(data, cb + 1);
					m_context = XML_Parser_Context_CData;
				}
				else
				{
					m_posCursor += __Next(data.m_pEnd);
				}
				break;
			}
			case XML_Parser_Context_Element:
			{
				if ((*(data.m_pEnd - 0) == '>'))
				{
					if (!__ParseElement(data))
						return XML_Parser_Result_Error;
					m_context = XML_Parser_Context_CData;
				}
				else
				{
					m_posCursor += __Next(data.m_pEnd);
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
			XML_String data;
			data.m_pStart = &(m_data[m_posStart]);
			data.m_pEnd = data.m_pStart + (m_posCursor - m_posStart);

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
	data.m_pStart = &(m_data[m_posStart]);
	data.m_pEnd = data.m_pStart + (m_posCursor - m_posStart);
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
	assert(*(data.m_pStart) == '<');
	assert(*(data.m_pEnd) == '>');
	const u32 cb = data.Size();
	u8* pSrc = data.m_pStart;

	bool isClose = false;
	bool isOpenClose = false;

	++pSrc;

	while (__IsWhitespace(*pSrc))
		++pSrc;

	// close tag.
	if (*pSrc == '/')
	{
		isClose = true;
		++pSrc;
	}

	while (__IsWhitespace(*pSrc))
		++pSrc;

	// tag name.
	XML_String tagName;
	if (!__ParseName(pSrc, tagName))
	{
		__ParseError("Parsing element, expected element name");
		return false;
	}

	while (__IsWhitespace(*pSrc))
		++pSrc;

	// attributes.
	std::vector<XML_Attrib> attribs;
	while (true)
	{
		XML_Attrib attrib;

		// attrib name.
		if (!__ParseName(pSrc, attrib.m_name))
			break;

		while (__IsWhitespace(*pSrc))
			++pSrc;

		// eq.
		if (*pSrc != '=')
			break;
		++pSrc;

		while (__IsWhitespace(*pSrc))
			++pSrc;

		// attrib value.
		if (!__ParseAttrValue(pSrc, attrib.m_value))
			break;

		while (__IsWhitespace(*pSrc))
			++pSrc;

		attribs.push_back(attrib);
	}

	while (__IsWhitespace(*pSrc))
		++pSrc;

	if (*pSrc == '/')
	{
		isOpenClose = true;
		++pSrc;
	}

	while (__IsWhitespace(*pSrc))
		++pSrc;

	if (*pSrc != '>')
	{
		__ParseError("Parsing element, expected closing tag >");
		return false;
	}

	tagName.Terminate();

	if (!isClose)
	{
		std::vector<const u8*> attrib_ptr;
		attrib_ptr.reserve((attribs.size() + 1) * 2);

		// make a list of attrib pointers, add null terminators.
		for (std::vector<XML_Attrib>::iterator it = attribs.begin(); it != attribs.end(); ++it)
		{
			XML_Attrib& attrib = *it;
			attrib.m_name.Terminate();
			__DecodeString(attrib.m_value);
			attrib.m_value.Terminate();
			attrib_ptr.push_back(attrib.m_name.m_pStart);
			attrib_ptr.push_back(attrib.m_value.m_pStart);
		}

		attrib_ptr.push_back(nullptr);
		attrib_ptr.push_back(nullptr);

		m_pfn_Start(tagName.m_pStart, attrib_ptr.data());
	}

	if (isClose || isOpenClose)
	{
		m_pfn_End(tagName.m_pStart);
	}

	__Consume(data, cb + 1);
	return true;
}

bool XML_Parser::__ParseCData(XML_String& data)
{
	const u32 cb = data.Size();
	u8* pSrc = data.m_pStart;

	// check if the entire data is whitespace.
	while ((pSrc != data.m_pEnd) && __IsWhitespace(*pSrc))
		++pSrc;
	if (pSrc == data.m_pEnd)
	{
		__Consume(data, cb);
		return true;
	}

	__DecodeString(data);

	assert((m_posStart + cb) < static_cast<u32>(m_data.size()));
	const u8 temp = *data.m_pEnd;
	*data.m_pEnd = 0;
	m_pfn_Data(data.m_pStart, data.Size());
	*data.m_pEnd = temp;

	__Consume(data, cb);
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

bool XML_Parser::__ParseName(u8*& pSrc, XML_String& name)
{
	while (__IsWhitespace(*pSrc))
		++pSrc;

	if (!__IsNameStartChar(__GetChar(pSrc)))
		return false;
	name.m_pStart = pSrc;
	__Next(pSrc);

	while (__IsNameChar(__GetChar(pSrc)))
		__Next(pSrc);
		
	name.m_pEnd = pSrc;
	return true;
}

bool XML_Parser::__ParseAttrValue(u8*& pSrc, XML_String& value)
{
	while (__IsWhitespace(*pSrc))
		++pSrc;

	if (*pSrc != '"')
		return false;
	__Next(pSrc);
	value.m_pStart = pSrc;

	while (*pSrc != '"' && *pSrc != '>')
		__Next(pSrc);

	if (*pSrc != '"')
		return false;

	value.m_pEnd = pSrc;
	++pSrc;

	while (__IsWhitespace(*pSrc))
		++pSrc;
	return true;
}

void XML_Parser::__ParseError(const char* pszErrorReason)
{
	m_errorReason = pszErrorReason;
}

u32 XML_Parser::__Next(u8*& pSrc) const
{
	if ((*pSrc & 0x80) == 0)
	{
		++pSrc;
		return 1;
	}
	else if ((*pSrc & 0xe0) == 0xc0)
	{
		++pSrc;
		assert((*pSrc & 0xc0) == 0x80);
		++pSrc;
		return 2;
	}
	else if ((*pSrc & 0xf0) == 0xe0)
	{
		++pSrc;
		assert((*pSrc & 0xc0) == 0x80);
		++pSrc;
		assert((*pSrc & 0xc0) == 0x80);
		++pSrc;
		return 3;
	}
	else if ((*pSrc & 0xf8) == 0xf0)
	{
		++pSrc;
		assert((*pSrc & 0xc0) == 0x80);
		++pSrc;
		assert((*pSrc & 0xc0) == 0x80);
		++pSrc;
		assert((*pSrc & 0xc0) == 0x80);
		++pSrc;
		return 4;
	}
	else
	{
		++pSrc;
		return 1;
	}
}

u32 XML_Parser::__GetChar(const u8* pSrc) const
{
	if (((*(pSrc + 0) & 0x80) == 0))
	{
		return *pSrc;
	}
	else if (((*(pSrc + 0) & 0xe0) == 0xc0))
	{
		return (static_cast<u32>(*(pSrc + 0) & 0x1f) << 6)
			| (static_cast<u32>(*(pSrc + 1) & 0x3f) << 0);
	}
	else if (((*(pSrc + 0) & 0xf0) == 0xe0))
	{
		return (static_cast<u32>(*(pSrc + 0) & 0x0f) << 12)
			| (static_cast<u32>(*(pSrc + 1) & 0x3f) << 6)
			| (static_cast<u32>(*(pSrc + 2) & 0x3f) << 0);
	}
	else if (((*(pSrc + 0) & 0xf8) == 0xf0))
	{
		return (static_cast<u32>(*(pSrc + 0) & 0x07) << 18)
			| (static_cast<u32>(*(pSrc + 1) & 0x3f) << 12)
			| (static_cast<u32>(*(pSrc + 2) & 0x3f) << 6)
			| (static_cast<u32>(*(pSrc + 3) & 0x3f) << 0);
	}
	else
	{
		return *pSrc;
	}
}

bool XML_Parser::__DecodeString(XML_String& s)
{
	u8* pDst = s.m_pStart;
	for (u8* pSrc = s.m_pStart; pSrc != s.m_pEnd; )
	{
		if (*pSrc == '&')
		{
			// escape sequence !
			if (!__DecodeEsc(pSrc, static_cast<u32>(s.m_pEnd - pSrc), pDst))
				return false;
		}
		else
		{
			// copy character.
			u8* pSrcNext = pSrc;
			const u32 cb = __Next(pSrcNext);
			if (pDst != pSrc)
			{
				memmove(pDst, pSrc, cb);
			}
			pDst += cb;
			pSrc += cb;
		}
	}

	s.m_pEnd = pDst;
	return true;
}

bool XML_Parser::__DecodeEsc(u8*& pSrc, u32 size, u8*& pDst)
{
	++pSrc;
	--size;
	if (size >= 4 && *(pSrc + 0) == 'a' && *(pSrc + 1) == 'm' && *(pSrc + 2) == 'p' && *(pSrc + 3) == ';')
	{
		*pDst = '&';
		pSrc += 4;
		pDst++;
		return true;
	}
	else if (size >= 3 && *(pSrc + 0) == 'l' && *(pSrc + 1) == 't' && *(pSrc + 2) == ';')
	{
		*pDst = '<';
		pSrc += 3;
		pDst++;
		return true;
	}
	else if (size >= 3 && *(pSrc + 0) == 'g' && *(pSrc + 1) == 't' && *(pSrc + 2) == ';')
	{
		*pDst = '>';
		pSrc += 3;
		pDst++;
		return true;
	}
	else if (size >= 5 && *(pSrc + 0) == 'q' && *(pSrc + 1) == 'u' && *(pSrc + 2) == 'o' && *(pSrc + 3) == 't' && *(pSrc + 4) == ';')
	{
		*pDst = '"';
		pSrc += 5;
		pDst++;
		return true;
	}
	else if (size >= 5 && *(pSrc + 0) == 'a' && *(pSrc + 1) == 'p' && *(pSrc + 2) == 'o' && *(pSrc + 3) == 's' && *(pSrc + 4) == ';')
	{
		*pDst = '\'';
		pSrc += 5;
		pDst++;
		return true;
	}
	else if (size >= 4 && *(pSrc + 0) == '#' && *(pSrc + 1) == 'x')
	{
		pSrc += 2;
		size -= 2;
		u32 val = 0;
		while ((size > 0) && __IsHexDigit(*pSrc))
		{
			val = val * 16;
			if (__IsDigit(*pSrc))
				val += (*pSrc - '0');
			else if (*pSrc >= 'A' && *pSrc <= 'F')
				val += (*pSrc - 'A') + 10;
			else if (*pSrc >= 'a' && *pSrc <= 'f')
				val += (*pSrc - 'a') + 10;
			++pSrc;
			--size;
		}
		if ((size == 0) || *pSrc != ';')
			return false;
		++pSrc;
		*pDst = static_cast<u8>(val);
		return true;
	}
	else if (size >= 3 && *(pSrc + 0) == '#')
	{
		++pSrc;
		u32 val = 0;
		while ((size > 0) && __IsDigit(*pSrc))
		{
			val = val * 10;
			val += (*pSrc - '0');
			++pSrc;
		}
		if ((size == 0) || *pSrc != ';')
			return false;
		++pSrc;
		*pDst = static_cast<u8>(val);
		return true;
	}
	else
	{
		while ((size > 0) && *pSrc != ';')
		{
			++pSrc;
			--size;
		}
		return true;
	}
	return false;
}

}
