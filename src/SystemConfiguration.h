/**
 * Base class for parameterisation and configuration of system through command line parameters.
 */

#pragma once

#include <string>
#include <vector>


class SystemConfiguration
{
public:

	struct sParameter
	{
		std::string name;
		std::string nameLower;
		std::string parameter;
		std::string description;

		sParameter(const std::string& name, const std::string& parameter, const std::string& description);
	};

	SystemConfiguration(const std::string& name);

	const std::string& getSystemName() const;

	const std::vector<sParameter>& getParameters() const;
	
	bool processOption(const std::string& name);
	bool processParameter(const std::string& name, const std::string& value);

protected:

	void addParameter(const std::string& name, const std::string& parameter, const std::string& description);
	
	virtual bool handleParameter(int idx, const std::string& value) = 0;

protected:

	std::string             systemName;
	std::vector<sParameter> parameters;
};