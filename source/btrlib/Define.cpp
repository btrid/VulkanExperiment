#include <btrlib/Define.h>
namespace btr 
{
	static std::string s_app_path = "..\\..\\resource\\";
	static std::string s_lib_path = "..\\..\\resource\\applib\\";
	static std::string s_btrlib_path = "..\\..\\resource\\btrlib\\";
	std::string getResourceAppPath()
	{
		return s_app_path;
	}
	void setResourceAppPath(const std::string& str)
	{
		s_app_path = str;
	}
	std::string getResourceLibPath()
	{
		return s_lib_path;
	}
	std::string getResourceShaderPath()
	{
		return "../../resource/shader/binary/";
	}
	void setResourceLibPath(const std::string& str)
	{
		s_lib_path = str;
	}
	std::string getResourceBtrLibPath()
	{
		return s_btrlib_path;
	}

}



