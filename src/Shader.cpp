#include "Shader.h"

Shader::Shader(const char * vertexPath, const char * fragmentPath)
{
	static const std::string version = "#version 430 core\n";

	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	auto vertexSource = get_file_contents(vertexPath);
	auto fragmentSource = get_file_contents(fragmentPath);

	auto vShaderCode = vertexSource.c_str();
	auto fShaderCode = fragmentSource.c_str();

	const char* vShaderSources[2] = { version.c_str(), vShaderCode};
	const char* fShaderSources[2] = { version.c_str(), fShaderCode};

	glShaderSource(vertexShader, 2, vShaderSources, NULL);
	glShaderSource(fragmentShader, 2, fShaderSources, NULL);

	glCompileShader(vertexShader);

	int success;
	char infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, sizeof(infoLog), NULL, infoLog);
		std::cout << "Error compiling vertex shader '" << vertexPath << "'\n" << infoLog << std::endl;
		//std::cout << "Vertex shader source:\n" << version + defaultDefinesString + definesString + vShaderCode;
		// TODO find a solution for this. It's affecting the next Shader object created
		// when this one fails
	}

	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, sizeof(infoLog), NULL, infoLog);
		std::cout << "Error compiling fragment shader '" << fragmentPath << "'\n" << infoLog << std::endl;
		//std::cout << "Fragment shader source:\n" << version + defaultDefinesString + definesString + fShaderCode;
	}

	id = glCreateProgram();
	glAttachShader(id, vertexShader);
	glAttachShader(id, fragmentShader);

	glLinkProgram(id);

	glGetProgramiv(id, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(id, sizeof(infoLog), NULL, infoLog);
		std::cout << "Error compiling shader program.\nVertex Shader: " << vertexPath << "\nFragment Shader: " << fragmentPath << "'\n" << infoLog << std::endl;
		//std::cout << "Vertex shader source:\n" << version + defaultDefinesString + definesString + vShaderCode << std::endl;
		//std::cout << "Fragment shader source:\n" << version + defaultDefinesString + definesString + fShaderCode << std::endl;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	use();
}

void Shader::use()
{
	glUseProgram(id);
}

void Shader::SetInt(const char * name, int value)
{
	glUniform1i(GetUniformLocation(name), value);
}

void Shader::SetFloat(const char* name, float value)
{
	glUniform1f(GetUniformLocation(name), value);
}

std::string Shader::get_file_contents(const char * path)
{
	std::ifstream in(path);
	if (in)
	{
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		return contents;
	}
	else
	{
		std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: '" << path << "'\n";
		return {};
	}
}
