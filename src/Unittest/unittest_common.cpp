#include "pch.h"

#include "common/parse_xml.h"

#include "unittest_common.h"
#include "unittest.h"

using namespace TB8;

enum unittest_common_parse_xml_obj_seq_type : u32
{
	unittest_common_parse_xml_obj_seq_type_start,
	unittest_common_parse_xml_obj_seq_type_start_attrib_name,
	unittest_common_parse_xml_obj_seq_type_start_attrib_val,
	unittest_common_parse_xml_obj_seq_type_end,
	unittest_common_parse_xml_obj_seq_type_data,
};

struct unittest_common_parse_xml_obj_seq
{
	unittest_common_parse_xml_obj_seq_type		m_type;
	const char*									m_psz;
};

struct unittest_common_parse_xml_obj
{
	unittest_common_parse_xml_obj();

	XML_Parser_Result read(u8* buffer, u32* pSize);
	void start(const char* name, const char** attrs);
	void data(const char* data, u32 len);
	void end(const char* name);

	const u8* m_pData;
	u32 m_size;
	u32 m_cursor;
	u32 m_seqIndex;
	const unittest_common_parse_xml_obj_seq* m_pSeq;
	u32 m_seqCount = 0;
};

unittest_common_parse_xml_obj::unittest_common_parse_xml_obj()
	: m_pData(nullptr)
	, m_size(0)
	, m_cursor(0)
	, m_seqIndex(0)
{
}

XML_Parser_Result unittest_common_parse_xml_obj::read(u8* buffer, u32* pSize)
{
	const u32 toCopy = std::min<u32>(*pSize, m_size - m_cursor);
	if (toCopy == 0)
		return XML_Parser_Result_EOF;

	memcpy(buffer, m_pData + m_cursor, toCopy);
	m_cursor += toCopy;
	*pSize = toCopy;

	return XML_Parser_Result_Success;
}

void unittest_common_parse_xml_obj::start(const char* name, const char** attrs)
{
	if (m_seqIndex >= m_seqCount)
	{
		TESTOUT(unittest_output_error, "Exceeded index count");
		return;
	}
	const unittest_common_parse_xml_obj_seq& seq = m_pSeq[m_seqIndex];
	if (seq.m_type != unittest_common_parse_xml_obj_seq_type_start)
	{
		TESTOUT(unittest_output_error, "Received unexpected start tag");
		return;
	}
	if (strcmp(name, seq.m_psz) != 0)
	{
		TESTOUT(unittest_output_error, "Received unexpected start tag");
		return;
	}
	m_seqIndex += 1;

	while (*attrs != 0)
	{
		if ((m_seqIndex + 1) >= m_seqCount)
		{
			TESTOUT(unittest_output_error, "Exceeded index count");
			return;
		}
		const unittest_common_parse_xml_obj_seq& seqName = m_pSeq[m_seqIndex];
		if (seqName.m_type != unittest_common_parse_xml_obj_seq_type_start_attrib_name)
		{
			TESTOUT(unittest_output_error, "Received unexpected attrib");
			return;
		}
		if (strcmp(*attrs, seqName.m_psz) != 0)
		{
			TESTOUT(unittest_output_error, "Received unexpected attrib name");
			return;
		}
		++m_seqIndex;
		++attrs;
		const unittest_common_parse_xml_obj_seq& seqVal = m_pSeq[m_seqIndex];
		if (seqVal.m_type != unittest_common_parse_xml_obj_seq_type_start_attrib_val)
		{
			TESTOUT(unittest_output_error, "Received unexpected attrib");
			return;
		}
		if (strcmp(*attrs, seqVal.m_psz) != 0)
		{
			TESTOUT(unittest_output_error, "Received unexpected attrib value");
			return;
		}
		++m_seqIndex;
		++attrs;
	}
}

void unittest_common_parse_xml_obj::data(const char* data, u32 len)
{
	if (m_seqIndex >= m_seqCount)
	{
		TESTOUT(unittest_output_error, "Exceeded index count");
		return;
	}
	const unittest_common_parse_xml_obj_seq& seq = m_pSeq[m_seqIndex];
	if (seq.m_type != unittest_common_parse_xml_obj_seq_type_data)
	{
		TESTOUT(unittest_output_error, "Received unexpected data");
		return;
	}
	if (strcmp(data, seq.m_psz) != 0)
	{
		TESTOUT(unittest_output_error, "Received unexpected data");
		return;
	}
	m_seqIndex += 1;
}

void unittest_common_parse_xml_obj::end(const char* name)
{
	if (m_seqIndex >= m_seqCount)
	{
		TESTOUT(unittest_output_error, "Exceeded index count");
		return;
	}
	const unittest_common_parse_xml_obj_seq& seq = m_pSeq[m_seqIndex];
	if (seq.m_type != unittest_common_parse_xml_obj_seq_type_end)
	{
		TESTOUT(unittest_output_error, "Received unexpected end tag");
		return;
	}
	if (strcmp(name, seq.m_psz) != 0)
	{
		TESTOUT(unittest_output_error, "Received unexpected end tag");
		return;
	}
	m_seqIndex += 1;
}

void unittest_common_parse_xml()
{
	// xml with tons of interesting cases.
	const char* szTestXML = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		"<tag1>\n"
		"\t<tag2/>\n"
		"\t<tag2>data</tag2>\n"
		"<!-- comment -->\n"
		"\t<tag2 attrib=\"abc\"/>\n"
		"\t<tag2 a1=\"abc\" a2=\"def\">data</tag2>\n"
		"\t<tag2>data&quot;&apos;&lt;&gt;&amp;&#x41;&#x4a;&#67;&#x6A;&bogus;data</tag2>\n"
		"\t<ns:tag2 ns:a1=\"abc\" ns:a2=\"def\">data</ns:tag2>\n"
		"\t<tag2>\xc8\xa4 d \xe8\x82\x89 a &amp;\xf4\xb4\xa2\x91\x83 ta</tag2>\n"
		"\t<\xc3\x98\xc2\xb7tag/>"
		"</tag1>\n";

	const unittest_common_parse_xml_obj_seq seq[] =
	{
		// "<tag1>\n"
		{ unittest_common_parse_xml_obj_seq_type_start, "tag1" },

		// "\t<tag2/>\n"
		{ unittest_common_parse_xml_obj_seq_type_start, "tag2" },
		{ unittest_common_parse_xml_obj_seq_type_end, "tag2" },
	
		// "\t<tag2>data</tag2>\n"
		{ unittest_common_parse_xml_obj_seq_type_start, "tag2" },
		{ unittest_common_parse_xml_obj_seq_type_data, "data" },
		{ unittest_common_parse_xml_obj_seq_type_end, "tag2" },

		// "\t<tag2 attrib=\"abc\"/>\n"
		{ unittest_common_parse_xml_obj_seq_type_start, "tag2" },
		{ unittest_common_parse_xml_obj_seq_type_start_attrib_name, "attrib" },
		{ unittest_common_parse_xml_obj_seq_type_start_attrib_val, "abc" },
		{ unittest_common_parse_xml_obj_seq_type_end, "tag2" },

		// "\t<tag2 a1=\"abc\" a2=\"def\">data</tag2>\n"
		{ unittest_common_parse_xml_obj_seq_type_start, "tag2" },
		{ unittest_common_parse_xml_obj_seq_type_start_attrib_name, "a1" },
		{ unittest_common_parse_xml_obj_seq_type_start_attrib_val, "abc" },
		{ unittest_common_parse_xml_obj_seq_type_start_attrib_name, "a2" },
		{ unittest_common_parse_xml_obj_seq_type_start_attrib_val, "def" },
		{ unittest_common_parse_xml_obj_seq_type_data, "data" },
		{ unittest_common_parse_xml_obj_seq_type_end, "tag2" },

		// "\t<tag2>data&quot;&apos;&lt;&gt;&amp;&#x41;&#x4a;&#67;&#x6A;&bogus;data</tag2>\n"
		{ unittest_common_parse_xml_obj_seq_type_start, "tag2" },
		{ unittest_common_parse_xml_obj_seq_type_data, "data\"'<>&AJCjdata" },
		{ unittest_common_parse_xml_obj_seq_type_end, "tag2" },

		// "\t<ns:tag2 ns:a1=\"abc\" ns:a2=\"def\">data</ns:tag2>\n"
		{ unittest_common_parse_xml_obj_seq_type_start, "ns:tag2" },
		{ unittest_common_parse_xml_obj_seq_type_start_attrib_name, "ns:a1" },
		{ unittest_common_parse_xml_obj_seq_type_start_attrib_val, "abc" },
		{ unittest_common_parse_xml_obj_seq_type_start_attrib_name, "ns:a2" },
		{ unittest_common_parse_xml_obj_seq_type_start_attrib_val, "def" },
		{ unittest_common_parse_xml_obj_seq_type_data, "data" },
		{ unittest_common_parse_xml_obj_seq_type_end, "ns:tag2" },

		// "\t<tag2>\xc8\xa4 d \xe8\x82\x89 a \xf4\xb4\xa2\x91\x83 ta</tag2>\n"
		{ unittest_common_parse_xml_obj_seq_type_start, "tag2" },
		{ unittest_common_parse_xml_obj_seq_type_data, "\xc8\xa4 d \xe8\x82\x89 a &\xf4\xb4\xa2\x91\x83 ta" },
		{ unittest_common_parse_xml_obj_seq_type_end, "tag2" },

		// "\t<\xc3\x98\xc2\xb7tag/>"
		{ unittest_common_parse_xml_obj_seq_type_start, "\xc3\x98\xc2\xb7tag" },
		{ unittest_common_parse_xml_obj_seq_type_end, "\xc3\x98\xc2\xb7tag" },

		// "</tag1>\n";
		{ unittest_common_parse_xml_obj_seq_type_end, "tag1" },
	};

	TESTBEGIN("XML Parser test");

	unittest_common_parse_xml_obj obj;
	obj.m_pData = reinterpret_cast<const u8*>(szTestXML);
	obj.m_size = static_cast<u32>(strlen(szTestXML));
	obj.m_pSeq = seq;
	obj.m_seqCount = ARRAYSIZE(seq);

	XML_Parser parser;
	parser.SetBufferSize(16);
	parser.SetReader(std::bind(&unittest_common_parse_xml_obj::read, &obj, std::placeholders::_1, std::placeholders::_2));
	parser.SetHandlers(std::bind(&unittest_common_parse_xml_obj::start, &obj, std::placeholders::_1, std::placeholders::_2),
						std::bind(&unittest_common_parse_xml_obj::data, &obj, std::placeholders::_1, std::placeholders::_2),
						std::bind(&unittest_common_parse_xml_obj::end, &obj, std::placeholders::_1));

	XML_Parser_Result r = parser.Parse();
	if (r != XML_Parser_Result_Success)
	{
		TESTOUT(unittest_output_error, "Parse failed.");
		goto Exit;
	}

Exit:
	TESTEND();
}

void unittest_common()
{
	SUITEBEGIN("Starting common tests ...");

	unittest_common_parse_xml();

	SUITEEND();
}