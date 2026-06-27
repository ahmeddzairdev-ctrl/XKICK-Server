
#ifndef __TABLE_H__
#define __TABLE_H__

#include "Include.h"

////////////////////////////////////////////////////////////////////////////////
/// \class cCSVAlias
/// \brief CSV 파일을 수정했을 때 발생하는 인덱스 문제를 줄이기 위한 
/// 별명 객체.
///
/// 예를 들어 0번 컬럼이 A에 관한 내용을 포함하고, 1번 컬럼이 B에 관한 내용을 
/// 포함하고 있었는데...
///
/// <pre>
/// int a = row.AsInt(0);
/// int b = row.AsInt(1);
/// </pre>
///
/// 그 사이에 C에 관한 내용을 포함하는 컬럼이 끼어든 경우, 하드코딩되어 있는 
/// 1번을 찾아서 고쳐야 하는데, 상당히 에러가 발생하기 쉬운 작업이다. 
///
/// <pre>
/// int a = row.AsInt(0);
/// int c = row.AsInt(1);
/// int b = row.AsInt(2); <-- 이 부분을 일일이 신경써야 한다.
/// </pre>
/// 
/// 이 부분을 문자열로 처리하면 유지보수에 들어가는 수고를 약간이나마 줄일 수 
/// 있다.
////////////////////////////////////////////////////////////////////////////////

class cCSVAlias
{
private:

	typedef std::map<std::string, size_t> NAME2INDEX_MAP;
	typedef std::map<size_t, std::string> INDEX2NAME_MAP;

	NAME2INDEX_MAP m_Name2Index;  ///< 셀 인덱스 대신으로 사용하기 위한 이름들
	INDEX2NAME_MAP m_Index2Name;  ///< 잘못된 alias를 검사하기 위한 추가적인 맵


public:
	/// \brief 생성자
	cCSVAlias() {} 

	/// \brief 소멸자
	virtual ~cCSVAlias() {}


public:
	/// \brief 셀을 액세스할 때, 숫자 대신 사용할 이름을 등록한다.
	void AddAlias(const char* name, size_t index);

	/// \brief 모든 데이터를 삭제한다.
	void Destroy();

	/// \brief 숫자 인덱스를 이름으로 변환한다.
	const char* operator [] (size_t index) const;

	/// \brief 이름을 숫자 인덱스로 변환한다.
	size_t operator [] (const char* name) const;


private:
	/// \brief 복사 생성자 금지
	cCSVAlias(const cCSVAlias&) {}

	/// \brief 대입 연산자 금지
	const cCSVAlias& operator = (const cCSVAlias&) { return *this; }
};


////////////////////////////////////////////////////////////////////////////////
/// \class cCSVRow 
/// \brief CSV 파일의 한 행을 캡슐화한 클래스
///
/// CSV의 기본 포맷은 엑셀에서 보이는 하나의 셀을 ',' 문자로 구분한 것이다.
/// 하지만, 셀 안에 특수 문자로 쓰이는 ',' 문자나 '"' 문자가 들어갈 경우, 
/// 모양이 약간 이상하게 변한다. 다음은 그 변화의 예이다.
/// 
/// <pre>
/// 엑셀에서 보이는 모양 | 실제 CSV 파일에 들어가있는 모양
/// ---------------------+----------------------------------------------------
/// ItemPrice            | ItemPrice
/// Item,Price           | "Item,Price"
/// Item"Price           | "Item""Price"
/// "ItemPrice"          | """ItemPrice"""
/// "Item,Price"         | """Item,Price"""
/// Item",Price          | "Item"",Price"
/// </pre>
/// 
/// 이 예로서 다음과 같은 사항을 알 수 있다.
/// - 셀 내부에 ',' 또는 '"' 문자가 들어갈 경우, 셀 좌우에 '"' 문자가 생긴다.
/// - 셀 내부의 '"' 문자는 2개로 치환된다.
///
/// \sa cCSVFile
////////////////////////////////////////////////////////////////////////////////

class cCSVRow : public std::vector<std::string>
{
public:
	/// \brief 기본 생성자
	cCSVRow() {}

	/// \brief 소멸자
	~cCSVRow() {}


public:
	/// \brief 해당 셀의 데이터를 int 형으로 반환한다.
	int AsInt(size_t index) const { return atoi(at(index).c_str()); }

	/// \brief 해당 셀의 데이터를 double 형으로 반환한다.
	double AsDouble(size_t index) const { return atof(at(index).c_str()); }

	/// \brief 해당 셀의 데이터를 문자열로 반환한다.
	const char* AsString(size_t index) const { return at(index).c_str(); }

	/// \brief 해당하는 이름의 셀 데이터를 int 형으로 반환한다.
	int AsInt(const char* name, const cCSVAlias& alias) const {
		return atoi( at(alias[name]).c_str() ); 
	}

	/// \brief 해당하는 이름의 셀 데이터를 int 형으로 반환한다.
	double AsDouble(const char* name, const cCSVAlias& alias) const {
		return atof( at(alias[name]).c_str() ); 
	}

	/// \brief 해당하는 이름의 셀 데이터를 문자열로 반환한다.
	const char* AsString(const char* name, const cCSVAlias& alias) const { 
		return at(alias[name]).c_str(); 
	}


private:
	/// \brief 복사 생성자 금지
	cCSVRow(const cCSVRow&) {}

	/// \brief 대입 연산자 금지
	const cCSVRow& operator = (const cCSVRow&) { return *this; }
};


////////////////////////////////////////////////////////////////////////////////
/// \class cCSVFile
/// \brief CSV(Comma Seperated Values) 파일을 read/write하기 위한 클래스
///
/// <b>sample</b>
/// <pre>
/// cCSVFile file;
///
/// cCSVRow row1, row2, row3;
/// row1.push_back("ItemPrice");
/// row1.push_back("Item,Price");
/// row1.push_back("Item\"Price");
///
/// row2.reserve(3);
/// row2[0] = "\"ItemPrice\"";
/// row2[1] = "\"Item,Price\"";
/// row2[2] = "Item\",Price\"";
///
/// row3 = "\"ItemPrice\"\"Item,Price\"Item\",Price\"";
///
/// file.add(row1);
/// file.add(row2);
/// file.add(row3);
/// file.save("test.csv", false);
/// </pre>
///
/// \todo 파일에서만 읽어들일 것이 아니라, 메모리 소스로부터 읽는 함수도 
/// 있어야 할 듯 하다.
////////////////////////////////////////////////////////////////////////////////

class cCSVFile
{
private:
	typedef std::vector<cCSVRow*> ROWS;

	ROWS m_Rows; ///< 행 컬렉션


public:
	/// \brief 생성자
	cCSVFile() {}

	/// \brief 소멸자
	virtual ~cCSVFile() { Destroy(); }


public:
	/// \brief 지정된 이름의 CSV 파일을 로드한다.
	bool Load(const char* fileName, const char seperator=',', const char quote='"');

	/// \brief 가지고 있는 내용을 CSV 파일에다 저장한다.
	bool Save(const char* fileName, bool append=false, char seperator=',', char quote='"') const;

	/// \brief 모든 데이터를 메모리에서 삭제한다.
	void Destroy();

	/// \brief 해당하는 인덱스의 행을 반환한다.
	cCSVRow* operator [] (size_t index);

	/// \brief 해당하는 인덱스의 행을 반환한다.
	const cCSVRow* operator [] (size_t index) const;

	/// \brief 행의 갯수를 반환한다.
	size_t GetRowCount() const { return m_Rows.size(); }


private:
	/// \brief 복사 생성자 금지
	cCSVFile(const cCSVFile&) {}

	/// \brief 대입 연산자 금지
	const cCSVFile& operator = (const cCSVFile&) { return *this; }
};


////////////////////////////////////////////////////////////////////////////////
/// \class cCSVTable
/// \brief CSV 파일을 이용해 테이블 데이터를 로드하는 경우가 많은데, 이 클래스는 
/// 그 작업을 좀 더 쉽게 하기 위해 만든 유틸리티 클래스다.
///
/// CSV 파일을 로드하는 경우, 숫자를 이용해 셀을 액세스해야 하는데, CSV 
/// 파일의 포맷이 바뀌는 경우, 이 숫자들을 변경해줘야한다. 이 작업이 꽤 
/// 신경 집중을 요구하는 데다가, 에러가 발생하기 쉽다. 그러므로 숫자로 
/// 액세스하기보다는 문자열로 액세스하는 것이 약간 느리지만 낫다고 할 수 있다.
///
/// <b>sample</b>
/// <pre>
/// cCSVTable table;
///
/// table.alias(0, "ItemClass");
/// table.alias(1, "ItemType");
///
/// if (table.load("test.csv"))
/// {
///     while (table.next())
///     {
///         std::string item_class = table.AsString("ItemClass");
///         int         item_type  = table.AsInt("ItemType"); 
///     }
/// }
/// </pre>
////////////////////////////////////////////////////////////////////////////////

class cCSVTable
{
private:
	cCSVFile  m_File;   ///< CSV 파일 객체
	cCSVAlias m_Alias;  ///< 문자열을 셀 인덱스로 변환하기 위한 객체
	int       m_CurRow; ///< 현재 횡단 중인 행 번호


public:
	/// \brief 생성자
	cCSVTable();

	/// \brief 소멸자
	virtual ~cCSVTable();


public:
	/// \brief 지정된 이름의 CSV 파일을 로드한다.
	bool Load(const char* fileName, const char seperator=',', const char quote='"');
	bool Save(const char* fileName, bool append=false, const char seperator=',', const char quote='"');

	/// \brief 셀을 액세스할 때, 숫자 대신 사용할 이름을 등록한다.
	void AddAlias(const char* name, size_t index) { m_Alias.AddAlias(name, index); }

	/// \brief 다음 행으로 넘어간다.
	bool Next();

	/// \brief 현재 행의 셀 숫자를 반환한다.
	size_t ColCount() const;

	/// \brief 인덱스를 이용해 int 형으로 셀값을 반환한다.
	int AsInt(size_t index) const;

	/// \brief 인덱스를 이용해 double 형으로 셀값을 반환한다.
	double AsDouble(size_t index) const;

	/// \brief 인덱스를 이용해 std::string 형으로 셀값을 반환한다.
	const char* AsString(size_t index) const;

	/// \brief 셀 이름을 이용해 int 형으로 셀값을 반환한다.
	int AsInt(const char* name) const { return AsInt(m_Alias[name]); }

	/// \brief 셀 이름을 이용해 double 형으로 셀값을 반환한다.
	double AsDouble(const char* name) const { return AsDouble(m_Alias[name]); }

	/// \brief 셀 이름을 이용해 std::string 형으로 셀값을 반환한다.
	const char* AsString(const char* name) const { return AsString(m_Alias[name]); }

	/// \brief alias를 포함해 모든 데이터를 삭제한다.
	void Destroy();


private:
	/// \brief 현재 행을 반환한다.
	const cCSVRow* const CurRow() const;

	/// \brief 복사 생성자 금지
	cCSVTable(const cCSVTable&) {}

	/// \brief 대입 연산자 금지
	const cCSVTable& operator = (const cCSVTable&) { return *this; }
};

#include <fstream>

#ifndef Assert
	#include <assert.h>
	#define Assert assert
	#define LogToFile (void)(0);
#endif

namespace
{
	/// 파싱용 state 열거값
	enum ParseState
	{
		STATE_NORMAL = 0, ///< 일반 상태
		STATE_QUOTE       ///< 따옴표 뒤의 상태
	};

	/// 문자열 좌우의 공백을 제거해서 반환한다.
	std::string Trim(std::string str)
	{
		str = str.erase(str.find_last_not_of(" \t\r\n") + 1);
		str = str.erase(0, str.find_first_not_of(" \t\r\n"));
		return str;
	}
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 셀을 액세스할 때, 숫자 대신 사용할 이름을 등록한다.
/// \param name 셀 이름
/// \param index 셀 인덱스
////////////////////////////////////////////////////////////////////////////////
void cCSVAlias::AddAlias(const char* name, size_t index)
{
	std::string converted(name);

	Assert(m_Name2Index.find(converted) == m_Name2Index.end());
	Assert(m_Index2Name.find(index) == m_Index2Name.end());

	m_Name2Index.insert(NAME2INDEX_MAP::value_type(converted, index));
	m_Index2Name.insert(INDEX2NAME_MAP::value_type(index, name));
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 모든 데이터를 삭제한다.
////////////////////////////////////////////////////////////////////////////////
void cCSVAlias::Destroy()
{
	m_Name2Index.clear();
	m_Index2Name.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 숫자 인덱스를 이름으로 변환한다.
/// \param index 숫자 인덱스
/// \return const char* 이름
////////////////////////////////////////////////////////////////////////////////
const char* cCSVAlias::operator [] (size_t index) const
{
	INDEX2NAME_MAP::const_iterator itr(m_Index2Name.find(index));
	if (itr == m_Index2Name.end())
	{
		LogToFile(NULL, "cannot find suitable conversion for %d", index);
		Assert(false && "cannot find suitable conversion");
		return NULL;
	}

	return itr->second.c_str();
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 이름을 숫자 인덱스로 변환한다.
/// \param name 이름
/// \return size_t 숫자 인덱스
////////////////////////////////////////////////////////////////////////////////
size_t cCSVAlias::operator [] (const char* name) const
{
	NAME2INDEX_MAP::const_iterator itr(m_Name2Index.find(name));
	if (itr == m_Name2Index.end())
	{
		LogToFile(NULL, "cannot find suitable conversion for %s", name);
		Assert(false && "cannot find suitable conversion");
		return 0;
	}

	return itr->second;
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 지정된 이름의 CSV 파일을 로드한다.
/// \param fileName CSV 파일 이름
/// \param seperator 필드 분리자로 사용할 글자. 기본값은 ','이다.
/// \param quote 따옴표로 사용할 글자. 기본값은 '"'이다.
/// \return bool 무사히 로드했다면 true, 아니라면 false
////////////////////////////////////////////////////////////////////////////////
bool cCSVFile::Load(const char* fileName, const char seperator, const char quote)
{
	Assert(seperator != quote);

	std::ifstream file(fileName, std::ios::in);
	if (!file) return false;

	Destroy(); // 기존의 데이터를 삭제

	cCSVRow* row = NULL;
	ParseState state = STATE_NORMAL;
	std::string token = "";
	char buf[2048+1] = {0,};

	while (file.good())
	{
		file.getline(buf, 2048);
		buf[sizeof(buf)-1] = 0;

		std::string line(Trim(buf));
		if (line.empty() || (state == STATE_NORMAL && line[0] == '#')) continue;
		
		std::string text  = std::string(line) + "  "; // 파싱 lookahead 때문에 붙여준다.
		size_t cur = 0;

		while (cur < text.size())
		{
			// 현재 모드가 QUOTE 모드일 때,
			if (state == STATE_QUOTE)
			{
				// '"' 문자의 종류는 두 가지이다.
				// 1. 셀 내부에 특수 문자가 있을 경우 이를 알리는 셀 좌우의 것
				// 2. 셀 내부의 '"' 문자가 '"' 2개로 치환된 것
				// 이 중 첫번째 경우의 좌측에 있는 것은 CSV 파일이 정상적이라면, 
				// 무조건 STATE_NORMAL에 걸리게 되어있다.
				// 그러므로 여기서 걸리는 것은 1번의 우측 경우나, 2번 경우 뿐이다.
				// 2번의 경우에는 무조건 '"' 문자가 2개씩 나타난다. 하지만 1번의
				// 우측 경우에는 아니다. 이를 바탕으로 해서 코드를 짜면...
				if (text[cur] == quote)
				{
					// 다음 문자가 '"' 문자라면, 즉 연속된 '"' 문자라면 
					// 이는 셀 내부의 '"' 문자가 치환된 것이다.
					if (text[cur+1] == quote)
					{
						token += quote;
						++cur;
					}
					// 다음 문자가 '"' 문자가 아니라면 
					// 현재의 '"'문자는 셀의 끝을 알리는 문자라고 할 수 있다.
					else
					{
						state = STATE_NORMAL;
					}
				}
				else
				{
					token += text[cur];
				}
			}
			// 현재 모드가 NORMAL 모드일 때,
			else if (state == STATE_NORMAL)
			{
				if (row == NULL)
					row = new cCSVRow();

				// ',' 문자를 만났다면 셀의 끝의 의미한다.
				// 토큰으로서 셀 리스트에다가 집어넣고, 토큰을 초기화한다.
				if (text[cur] == seperator)
				{
					row->push_back(token);
					token.clear();
				}
				// '"' 문자를 만났다면, QUOTE 모드로 전환한다.
				else if (text[cur] == quote)
				{
					state = STATE_QUOTE;
				}
				// 다른 일반 문자라면 현재 토큰에다가 덧붙인다.
				else
				{
					token += text[cur];
				}
			}

			++cur;
		}

		// 마지막 셀은 끝에 ',' 문자가 없기 때문에 여기서 추가해줘야한다.
		// 단, 처음에 파싱 lookahead 때문에 붙인 스페이스 문자 두 개를 뗀다.
		if (state == STATE_NORMAL)
		{
			Assert(row != NULL);
			row->push_back(token.substr(0, token.size()-2));
			m_Rows.push_back(row);
			token.clear();
			row = NULL;
		}
		else
		{
			token = token.substr(0, token.size()-2) + "\r\n";
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 가지고 있는 내용을 CSV 파일에다 저장한다.
/// \param fileName CSV 파일 이름
/// \param append true일 경우, 기존의 파일에다 덧붙인다. false인 경우에는 
/// 기존의 파일 내용을 삭제하고, 새로 쓴다.
/// \param seperator 필드 분리자로 사용할 글자. 기본값은 ','이다.
/// \param quote 따옴표로 사용할 글자. 기본값은 '"'이다.
/// \return bool 무사히 저장했다면 true, 에러가 생긴 경우에는 false
////////////////////////////////////////////////////////////////////////////////
bool cCSVFile::Save(const char* fileName, bool append, char seperator, char quote) const
{
	Assert(seperator != quote);

	// 출력 모드에 따라 파일을 적당한 플래그로 생성한다.
	std::ofstream file;
	if (append) { file.open(fileName, std::ios::out | std::ios::app); }
	else { file.open(fileName, std::ios::out | std::ios::trunc); }

	// 파일을 열지 못했다면, false를 리턴한다.
	if (!file) return false;

	char special_chars[5] = { seperator, quote, '\r', '\n', 0 };
	char quote_escape_string[3] = { quote, quote, 0 };

	// 모든 행을 횡단하면서...
	for (size_t i=0; i<m_Rows.size(); i++)
	{
		const cCSVRow& row = *((*this)[i]);

		std::string line;

		// 행 안의 모든 토큰을 횡단하면서...
		for (size_t j=0; j<row.size(); j++)
		{
			const std::string& token = row[j];

			// 일반적인('"' 또는 ','를 포함하지 않은) 
			// 토큰이라면 그냥 저장하면 된다.
			if (token.find_first_of(special_chars) == std::string::npos)
			{
				line += token;
			}
			// 특수문자를 포함한 토큰이라면 문자열 좌우에 '"'를 붙여주고,
			// 문자열 내부의 '"'를 두 개로 만들어줘야한다.
			else
			{
				line += quote;

				for (size_t k=0; k<token.size(); k++)
				{
					if (token[k] == quote) line += quote_escape_string;
					else line += token[k];
				}

				line += quote;
			}

			// 마지막 셀이 아니라면 ','를 토큰의 뒤에다 붙여줘야한다.
			if (j != row.size() - 1) { line += seperator; }
		}

		// 라인을 출력한다.
		file << line << std::endl;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 모든 데이터를 메모리에서 삭제한다.
////////////////////////////////////////////////////////////////////////////////
void cCSVFile::Destroy()
{
	for (ROWS::iterator itr(m_Rows.begin()); itr != m_Rows.end(); ++itr)
		delete *itr;

	m_Rows.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 해당하는 인덱스의 행을 반환한다.
/// \param index 인덱스
/// \return cCSVRow* 해당 행
////////////////////////////////////////////////////////////////////////////////
cCSVRow* cCSVFile::operator [] (size_t index)
{
	Assert(index < m_Rows.size());
	return m_Rows[index];
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 해당하는 인덱스의 행을 반환한다.
/// \param index 인덱스
/// \return const cCSVRow* 해당 행
////////////////////////////////////////////////////////////////////////////////
const cCSVRow* cCSVFile::operator [] (size_t index) const
{
	Assert(index < m_Rows.size());
	return m_Rows[index];
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 생성자
////////////////////////////////////////////////////////////////////////////////
cCSVTable::cCSVTable()
: m_CurRow(-1)
{
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 소멸자
////////////////////////////////////////////////////////////////////////////////
cCSVTable::~cCSVTable()
{
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 지정된 이름의 CSV 파일을 로드한다.
/// \param fileName CSV 파일 이름
/// \param seperator 필드 분리자로 사용할 글자. 기본값은 ','이다.
/// \param quote 따옴표로 사용할 글자. 기본값은 '"'이다.
/// \return bool 무사히 로드했다면 true, 아니라면 false
////////////////////////////////////////////////////////////////////////////////
bool cCSVTable::Load(const char* fileName, const char seperator, const char quote)
{
	Destroy();
	return m_File.Load(fileName, seperator, quote);
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 지정된 이름의 CSV 파일을 로드한다.
/// \param fileName CSV 파일 이름
/// \param seperator 필드 분리자로 사용할 글자. 기본값은 ','이다.
/// \param quote 따옴표로 사용할 글자. 기본값은 '"'이다.
/// \return bool 무사히 로드했다면 true, 아니라면 false
////////////////////////////////////////////////////////////////////////////////
bool cCSVTable::Save(const char* fileName, bool append, const char seperator, const char quote)
{
	return m_File.Save(fileName, append, seperator, quote);
}


////////////////////////////////////////////////////////////////////////////////
/// \brief 다음 행으로 넘어간다.
/// \return bool 다음 행으로 무사히 넘어간 경우 true를 반환하고, 더 이상
/// 넘어갈 행이 존재하지 않는 경우에는 false를 반환한다.
////////////////////////////////////////////////////////////////////////////////
bool cCSVTable::Next()
{
	// 20억번 정도 호출하면 오버플로가 일어날텐데...괜찮겠지?
	return ++m_CurRow < (int)m_File.GetRowCount() ? true : false;
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 현재 행의 셀 숫자를 반환한다.
/// \return size_t 현재 행의 셀 숫자
////////////////////////////////////////////////////////////////////////////////
size_t cCSVTable::ColCount() const
{
	return CurRow()->size();
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 인덱스를 이용해 int 형으로 셀 값을 반환한다.
/// \param index 셀 인덱스
/// \return int 셀 값
////////////////////////////////////////////////////////////////////////////////
int cCSVTable::AsInt(size_t index) const
{
	const cCSVRow* const row = CurRow();
	Assert(row);
	Assert(index < row->size());
	return row->AsInt(index);
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 인덱스를 이용해 double 형으로 셀 값을 반환한다.
/// \param index 셀 인덱스
/// \return double 셀 값
////////////////////////////////////////////////////////////////////////////////
double cCSVTable::AsDouble(size_t index) const
{
	const cCSVRow* const row = CurRow();
	Assert(row);
	Assert(index < row->size());
	return row->AsDouble(index);
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 인덱스를 이용해 std::string 형으로 셀 값을 반환한다.
/// \param index 셀 인덱스
/// \return const char* 셀 값
////////////////////////////////////////////////////////////////////////////////
const char* cCSVTable::AsString(size_t index) const
{
	const cCSVRow* const row = CurRow();
	Assert(row);
	Assert(index < row->size());
	return row->AsString(index);
}

////////////////////////////////////////////////////////////////////////////////
/// \brief alias를 포함해 모든 데이터를 삭제한다.
////////////////////////////////////////////////////////////////////////////////
void cCSVTable::Destroy()
{
	m_File.Destroy();
	m_Alias.Destroy();
	m_CurRow = -1;
}

////////////////////////////////////////////////////////////////////////////////
/// \brief 현재 행을 반환한다.
/// \return const cCSVRow* 액세스가 가능한 현재 행이 존재하는 경우에는 그 행의
/// 포인터를 반환하고, 더 이상 액세스 가능한 행이 없는 경우에는 NULL을 
/// 반환한다.
////////////////////////////////////////////////////////////////////////////////
const cCSVRow* const cCSVTable::CurRow() const
{
	if (m_CurRow < 0)
	{
		Assert(false && "call Next() first!");
		return NULL;
	}
	else if (m_CurRow >= (int)m_File.GetRowCount())
	{
		Assert(false && "no more rows!");
		return NULL;
	}

	return m_File[m_CurRow];
}

#endif

