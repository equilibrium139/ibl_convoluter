#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

class Shader
{
public:
	unsigned int id;
	Shader(const char* vertexPath, const char* fragmentPath);

	void use();

	void SetInt(const char* name, int value);
private:
	std::string get_file_contents(const char* path);
	std::unordered_map<std::string, int> cachedUniformLocations;

	int GetUniformLocation(const std::string& name)
	{
		auto iter = cachedUniformLocations.find(name);
		if (iter != cachedUniformLocations.end())
		{
			return iter->second;
		}
		int location = glGetUniformLocation(id, name.c_str());
		if (location < 0)
		{
			std::cerr << "Warning: GetUniformLocation attempting to access invalid uniform '" << name << "'\n";
		}
		//assert(location >= 0);
		cachedUniformLocations[name] = location;
		return location;
	}
};

#endif // !SHADER_H