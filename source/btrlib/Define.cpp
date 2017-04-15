#include <btrlib/Define.h>
namespace btr 
{
	static std::string s_path = "..\\..\\resource\\";
	std::string getResourcePath()
	{
		return s_path;
	}
	void setResourcePath(const std::string& str)
	{
		s_path = str;
	}

}



